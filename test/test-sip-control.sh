#!/bin/sh

set -euo pipefail

skip_code=77
tmp=$(mktemp)

DYLD_PRINT_LIBRARIES=1 DYLD_INSERT_LIBRARIES="$1" /usr/bin/make -v > "$tmp" 2>&1 || true

if grep -q "$1" "$tmp"; then
  echo '[test-sip-control] SKIP: interposer was loaded into /usr/bin/make on this system.'
  echo '[test-sip-control] This runner does not provide a usable SIP negative control for this check.'
  rm -f "$tmp"
  exit "$skip_code"
else
  echo '[test-sip-control] PASS: interposer was not loaded for /usr/bin/make (expected on SIP-protected context).'
  rm -f "$tmp"
fi
