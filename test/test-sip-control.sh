#!/bin/sh

set -euo pipefail

tmp=$(mktemp)

DYLD_PRINT_LIBRARIES=1 DYLD_INSERT_LIBRARIES="$1" /usr/bin/make -v > "$tmp" 2>&1 || true

if grep -q "$1" "$tmp"; then
  echo '[test-sip-control] Interposer was loaded into /usr/bin/make on this system.'
  echo '[test-sip-control] This binary/path is not acting as a SIP negative control here.'
  rm -f "$tmp"
  exit 1
else
  echo '[test-sip-control] PASS: interposer was not loaded for /usr/bin/make (expected on SIP-protected context).'
  rm -f "$tmp"
fi
