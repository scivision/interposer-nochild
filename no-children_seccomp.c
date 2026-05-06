#define _GNU_SOURCE

#include <seccomp.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <poll.h>
#include <sys/uio.h>
#include <sys/wait.h>

static int add_rule(scmp_filter_ctx ctx, uint32_t action, int syscall_nr)
{
    int rc = seccomp_rule_add(ctx, action, syscall_nr, 0);
    if (rc < 0) {
        fprintf(stderr, "seccomp_rule_add(%d) failed: %s\n", syscall_nr, strerror(-rc));
        return -1;
    }
    return 0;
}

static const char *cmd_basename(const char *path)
{
    const char *slash;
    if (path == NULL) {
        return NULL;
    }
    slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static int is_allowed_exec(const char *path)
{
    const char *base = cmd_basename(path);
    if (base == NULL || *base == '\0') {
        return 0;
    }
    return strcmp(base, "ninja") == 0 || strcmp(base, "make") == 0;
}

static int read_remote_cstring(pid_t pid, unsigned long long remote_addr, char *buf, size_t buf_size)
{
    struct iovec local_iov;
    struct iovec remote_iov;
    ssize_t nread;
    size_t i;

    if (remote_addr == 0 || buf == NULL || buf_size < 2) {
        return -1;
    }

    local_iov.iov_base = buf;
    local_iov.iov_len = buf_size - 1;
    remote_iov.iov_base = (void *)(uintptr_t)remote_addr;
    remote_iov.iov_len = buf_size - 1;

    nread = process_vm_readv(pid, &local_iov, 1, &remote_iov, 1, 0);
    if (nread <= 0) {
        return -1;
    }

    for (i = 0; i < (size_t)nread; i++) {
        if (buf[i] == '\0') {
            return 0;
        }
    }

    buf[nread] = '\0';
    return 0;
}

static void respond_deny(struct seccomp_notif_resp *resp, uint64_t id, int err)
{
    resp->id = id;
    resp->val = -1;
    resp->error = err;
    resp->flags = 0;
}

static void respond_continue(struct seccomp_notif_resp *resp, uint64_t id)
{
    resp->id = id;
    resp->val = 0;
    resp->error = 0;
    resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
}

static int handle_notification(int notify_fd, struct seccomp_notif *req, struct seccomp_notif_resp *resp)
{
    int rc;
    char path[512];
    int have_path = 0;
    int allow = 0;
    unsigned long long path_ptr = 0;

    memset(req, 0, sizeof(*req));
    memset(resp, 0, sizeof(*resp));

    rc = seccomp_notify_receive(notify_fd, req);
    if (rc < 0) {
        if (errno == EINTR) {
            return 0;
        }
        perror("seccomp_notify_receive");
        return -1;
    }

    rc = seccomp_notify_id_valid(notify_fd, req->id);
    if (rc < 0) {
        if (errno == ENOENT) {
            return 0;
        }
        perror("seccomp_notify_id_valid");
        return -1;
    }

    if (req->data.nr == SCMP_SYS(execve)) {
        path_ptr = req->data.args[0];
    } else if (req->data.nr == SCMP_SYS(execveat)) {
        path_ptr = req->data.args[1];
    } else {
        respond_deny(resp, req->id, EPERM);
        goto send_response;
    }

    path[0] = '\0';
    if (read_remote_cstring(req->pid, path_ptr, path, sizeof(path)) == 0) {
        have_path = 1;
        allow = is_allowed_exec(path);
    }

    if (allow) {
        respond_continue(resp, req->id);
    } else {
        fprintf(stderr, "[seccomp] blocked exec from pid %d: %s\n", req->pid,
                have_path ? path : "(unknown)");
        respond_deny(resp, req->id, EPERM);
    }

send_response:
    rc = seccomp_notify_respond(notify_fd, resp);
    if (rc < 0) {
        if (errno == ENOENT) {
            return 0;
        }
        perror("seccomp_notify_respond");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    int notify_fd;
    pid_t child;
    struct seccomp_notif *req = NULL;
    struct seccomp_notif_resp *resp = NULL;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <command> [args...]\n", argv[0]);
        return EXIT_FAILURE;
    }

    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_ALLOW);
    if (ctx == NULL) {
        perror("seccomp_init");
        return EXIT_FAILURE;
    }

    // Route exec notifications to userspace and decide by command basename.
    if (add_rule(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(execve)) < 0 ||
        add_rule(ctx, SCMP_ACT_NOTIFY, SCMP_SYS(execveat)) < 0) {
        seccomp_release(ctx);
        return EXIT_FAILURE;
    }

    if (seccomp_load(ctx) < 0) {
        perror("seccomp_load");
        seccomp_release(ctx);
        return EXIT_FAILURE;
    }

    notify_fd = seccomp_notify_fd(ctx);
    if (notify_fd < 0) {
        perror("seccomp_notify_fd");
        seccomp_release(ctx);
        return EXIT_FAILURE;
    }

    if (seccomp_notify_alloc(&req, &resp) < 0) {
        perror("seccomp_notify_alloc");
        seccomp_release(ctx);
        return EXIT_FAILURE;
    }

    child = fork();
    if (child < 0) {
        perror("fork");
        seccomp_notify_free(req, resp);
        seccomp_release(ctx);
        return EXIT_FAILURE;
    }

    if (child == 0) {
        seccomp_notify_free(req, resp);
        seccomp_release(ctx);
        execvp(argv[1], &argv[1]);
        perror("execvp");
        _exit(EXIT_FAILURE);
    }

    for (;;) {
        struct pollfd pfd;
        int prc;
        int status;
        pid_t w;

        w = waitpid(child, &status, WNOHANG);
        if (w == child) {
            seccomp_notify_free(req, resp);
            seccomp_release(ctx);
            if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
            }
            if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
            }
            return EXIT_FAILURE;
        }

        pfd.fd = notify_fd;
        pfd.events = POLLIN;
        pfd.revents = 0;

        prc = poll(&pfd, 1, 250);
        if (prc < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }
        if (prc == 0) {
            continue;
        }

        if (pfd.revents & POLLIN) {
            if (handle_notification(notify_fd, req, resp) < 0) {
                break;
            }
        }
    }

    seccomp_notify_free(req, resp);
    seccomp_release(ctx);
    return EXIT_FAILURE;
}
