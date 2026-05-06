#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

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

#include <string>
#include <string_view>
#include <unordered_set>
#include <iostream>
#include <span>

class SeccompContext {
public:
    explicit SeccompContext(uint32_t default_action = SCMP_ACT_ALLOW)
        : ctx_(seccomp_init(default_action)) {
        if (!ctx_) throw std::runtime_error("seccomp_init failed");
    }

    ~SeccompContext() { if (ctx_) seccomp_release(ctx_); }

    SeccompContext(const SeccompContext&) = delete;
    SeccompContext& operator=(const SeccompContext&) = delete;

    void add_notify_rule(int syscall) {
        if (seccomp_rule_add(ctx_, SCMP_ACT_NOTIFY, syscall, 0) < 0) {
            throw std::runtime_error("seccomp_rule_add failed");
        }
    }

    void load() {
        if (seccomp_load(ctx_) < 0) {
            throw std::runtime_error("seccomp_load failed");
        }
    }

    int notify_fd() const {
        int fd = seccomp_notify_fd(ctx_);
        if (fd < 0) throw std::runtime_error("seccomp_notify_fd failed");
        return fd;
    }

private:
    scmp_filter_ctx ctx_ = nullptr;
};

class SeccompNotifier {
public:
    explicit SeccompNotifier(int notify_fd) : notify_fd_(notify_fd) {
        if (seccomp_notify_alloc(&req_, &resp_) < 0) {
            throw std::runtime_error("seccomp_notify_alloc failed");
        }
    }

    ~SeccompNotifier() { seccomp_notify_free(req_, resp_); }

    SeccompNotifier(const SeccompNotifier&) = delete;
    SeccompNotifier& operator=(const SeccompNotifier&) = delete;

    struct seccomp_notif* req() const { return req_; }
    struct seccomp_notif_resp* resp() const { return resp_; }

private:
    int notify_fd_;
    struct seccomp_notif* req_ = nullptr;
    struct seccomp_notif_resp* resp_ = nullptr;
};

// ====================== Allowed Commands ======================
static const std::unordered_set<std::string_view> ALLOWED_COMMANDS = {
    "ninja", "make", "gmake", "cmake",
    "cc1", "cc1plus", "collect2",
    "sh", "bash", "dash",
    "c++", "cc", "gcc", "g++",
    "clang", "clang++",
    "gfortran",
    "ld", "ar", "ranlib", "as",
    "uname", "pkg-config", "git"   // ← added common tools CMake uses
};

static std::string_view get_basename(std::string_view path) {
    size_t pos = path.find_last_of('/');
    return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
}

static bool is_allowed_exec(const char* path) {
    if (!path || *path == '\0') return false;
    return ALLOWED_COMMANDS.contains(get_basename(path));
}

static bool read_remote_cstring(pid_t pid, unsigned long long remote_addr, std::span<char> buffer) {
    if (remote_addr == 0 || buffer.size() < 2) return false;

    struct iovec local  = { .iov_base = buffer.data(), .iov_len = buffer.size() - 1 };
    struct iovec remote = { .iov_base = (void*)(uintptr_t)remote_addr, .iov_len = buffer.size() - 1 };

    ssize_t nread = process_vm_readv(pid, &local, 1, &remote, 1, 0);
    if (nread <= 0) return false;

    buffer[nread] = '\0';
    return true;
}

static void respond_continue(struct seccomp_notif_resp* resp, uint64_t id) {
    resp->id = id;
    resp->val = 0;
    resp->error = 0;
    resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
}

static void respond_deny(struct seccomp_notif_resp* resp, uint64_t id, int err) {
    resp->id = id;
    resp->val = 0;
    resp->error = -err;
    resp->flags = 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <command> [args...]\n";
        return EXIT_FAILURE;
    }

    try {
        SeccompContext ctx;
        ctx.add_notify_rule(SCMP_SYS(execve));
        ctx.add_notify_rule(SCMP_SYS(execveat));
        ctx.load();

        int notify_fd = ctx.notify_fd();
        SeccompNotifier notifier(notify_fd);

        pid_t child = fork();
        if (child < 0) {
            perror("fork");
            return EXIT_FAILURE;
        }

        if (child == 0) {  // Child
            execvp(argv[1], &argv[1]);
            perror("execvp");
            _exit(EXIT_FAILURE);
        }

        // Parent
        bool first_exec_seen = false;
        char path_buf[512];

        while (true) {
            int status;
            pid_t w = waitpid(child, &status, WNOHANG);
            if (w == child) {
                if (WIFEXITED(status)) return WEXITSTATUS(status);
                if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
                return EXIT_FAILURE;
            }

            struct pollfd pfd = { .fd = notify_fd, .events = POLLIN };
            int prc = poll(&pfd, 1, 250);

            if (prc < 0) {
                if (errno == EINTR) continue;
                perror("poll");
                break;
            }
            if (prc == 0) continue;

            if (!(pfd.revents & POLLIN)) continue;

            auto* req = notifier.req();
            auto* resp = notifier.resp();

            memset(req, 0, sizeof(*req));
            memset(resp, 0, sizeof(*resp));

            if (seccomp_notify_receive(notify_fd, req) < 0) {
                if (errno == EINTR) continue;
                perror("seccomp_notify_receive");
                break;
            }

            if (seccomp_notify_id_valid(notify_fd, req->id) < 0) {
                if (errno == ENOENT) continue;
                perror("seccomp_notify_id_valid");
                break;
            }

            // === Decision ===
            if (static_cast<pid_t>(req->pid) == child && !first_exec_seen) {
                first_exec_seen = true;
                respond_continue(resp, req->id);
            }
            else if (req->data.nr != SCMP_SYS(execve) && req->data.nr != SCMP_SYS(execveat)) {
                respond_deny(resp, req->id, EPERM);
            }
            else {
                unsigned long long path_ptr = (req->data.nr == SCMP_SYS(execve))
                                            ? req->data.args[0]
                                            : req->data.args[1];

                bool allow = false;
                if (read_remote_cstring(req->pid, path_ptr, std::span<char>(path_buf))) {   // ← Fixed: use req->pid
                    allow = is_allowed_exec(path_buf);
                }

                if (path_buf[0] == '\0' && req->data.nr == SCMP_SYS(execveat)) {
                    allow = false;
                }

                if (allow) {
                    respond_continue(resp, req->id);
                } else {
                    std::cerr << "[seccomp] blocked exec from pid " << req->pid
                              << ": " << (path_buf[0] ? path_buf : "(unknown)") << '\n';
                    respond_deny(resp, req->id, EPERM);
                }
            }

            if (seccomp_notify_respond(notify_fd, resp) < 0 && errno != ENOENT) {
                perror("seccomp_notify_respond");
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_FAILURE;
}
