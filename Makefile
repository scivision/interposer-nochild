
ifeq ($(shell uname),Darwin)

no-children.dylib: no-children_macos.c
	$(CC) -dynamiclib -o $@ $< -flat_namespace -undefined dynamic_lookup

else ifeq ($(shell uname),Linux)

seccomp_run: no-children_seccomp.c
	$(CC) -o $@ $< -lseccomp

endif
