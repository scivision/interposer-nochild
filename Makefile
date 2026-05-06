
ifeq ($(OS),Windows_NT)

.DEFAULT_GOAL := all

all: no-children.exe

no-children.exe: no-children-windows.cpp
	$(CXX) $(CXXFLAGS) -municode -o $@ $< $(LDFLAGS)

else

UNAME_S := $(shell uname)

.DEFAULT_GOAL := all

COPTS = -std=c99 -Wall -O2 -fPIC

log_blocked.o: log_blocked.c
	$(CC) $(COPTS) -c -o $@ $<

ifeq ($(UNAME_S),Darwin)

all: no-children.dylib

no-children.dylib: no-children_macos.c log_blocked.o
	$(CC) -dynamiclib $(COPTS) -o $@ $^ -flat_namespace -undefined dynamic_lookup

else ifeq ($(UNAME_S),Linux)

all: no-children.so

no-children.so: no-children_linux.c log_blocked.o
	$(CC) -shared $(COPTS) -o $@ $^ -ldl

endif

clean:
	$(RM) no-children.so no-children.dylib log_blocked.o

endif
