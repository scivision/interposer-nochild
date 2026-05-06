
ifeq ($(OS),Windows_NT)

.DEFAULT_GOAL := all

all: no-children.exe

no-children.exe: no-children_windows.cpp
	$(CXX) $(CXXFLAGS) -municode -o $@ $< $(LDFLAGS)

else

UNAME_S := $(shell uname)

.DEFAULT_GOAL := all

COPTS = -std=c99 -Wall -O2 -fPIC

ifeq ($(UNAME_S),Darwin)

all: no-children.dylib

no-children.dylib: no-children_unix.c
	$(CC) -dynamiclib $(COPTS) -o $@ $^ -flat_namespace -undefined dynamic_lookup

else ifeq ($(UNAME_S),Linux)

all: no-children.so

no-children.so: no-children_unix.c
	$(CC) -shared $(COPTS) -o $@ $^ -ldl

endif

clean:
	$(RM) no-children.so no-children.dylib

endif
