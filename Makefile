
ifeq ($(OS),Windows_NT)

nochild.exe: no-children-windows.cpp
	$(CXX) $(CXXFLAGS) -municode -o $@ $< $(LDFLAGS)

else

UNAME_S := $(shell uname)

ifeq ($(UNAME_S),Darwin)

no-children.dylib: no-children_macos.c
	$(CC) -dynamiclib -o $@ $< -flat_namespace -undefined dynamic_lookup

else ifeq ($(UNAME_S),Linux)

seccomp_run: no-children_seccomp.c
	$(CC) -o $@ $< -lseccomp

endif

endif
