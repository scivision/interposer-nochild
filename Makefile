
ifeq ($(OS),Windows_NT)

.DEFAULT_GOAL := all

all: no-children.exe

no-children.exe: no-children_windows.cpp
	$(CXX) $(CXXFLAGS) -municode -o $@ $< $(LDFLAGS)

else

UNAME_S := $(shell uname)

.DEFAULT_GOAL := all

COPTS = -std=c99 -Wall -O2 -fPIC
SELFTEST = nochild-selftest

.PHONY: all clean test test-sip-control

ifeq ($(UNAME_S),Darwin)

all: no-children.dylib

no-children.dylib: no-children_unix.c
	$(CC) -dynamiclib $(COPTS) -o $@ $^ -flat_namespace -undefined dynamic_lookup

$(SELFTEST): nochild-selftest.c
	$(CC) -std=c99 -Wall -O2 -o $@ $<

test: all $(SELFTEST)
	@echo "[test] Running interposer self-test with DYLD_INSERT_LIBRARIES"
	@env DYLD_INSERT_LIBRARIES="$(CURDIR)/no-children.dylib" ./$(SELFTEST)

test-sip-control: all
	@echo "[test-sip-control] Probing a protected system binary (/usr/bin/make)"
	@tmp=$$(mktemp); \
	  DYLD_PRINT_LIBRARIES=1 DYLD_INSERT_LIBRARIES="$(CURDIR)/no-children.dylib" /usr/bin/make -v > $$tmp 2>&1 || true; \
	  if grep -q "$(CURDIR)/no-children.dylib" $$tmp; then \
	    echo "[test-sip-control] Interposer was loaded into /usr/bin/make on this system."; \
	    echo "[test-sip-control] This binary/path is not acting as a SIP negative control here."; \
	    $(RM) $$tmp; \
	    exit 1; \
	  else \
	    echo "[test-sip-control] PASS: interposer was not loaded for /usr/bin/make (expected on SIP-protected context)."; \
	    $(RM) $$tmp; \
	  fi

else ifeq ($(UNAME_S),Linux)

all: no-children.so

no-children.so: no-children_unix.c
	$(CC) -shared $(COPTS) -o $@ $^ -ldl

$(SELFTEST): nochild-selftest.c
	$(CC) -std=c99 -Wall -O2 -o $@ $<

test: all $(SELFTEST)
	@echo "[test] Running interposer self-test with LD_PRELOAD"
	@env LD_PRELOAD="$(CURDIR)/no-children.so" ./$(SELFTEST)

endif

clean:
	$(RM) no-children.so no-children.dylib $(SELFTEST)

endif
