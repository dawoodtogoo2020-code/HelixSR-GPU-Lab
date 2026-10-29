#!/usr/bin/env sh
# SPDX-License-Identifier: MIT
# Host-only metadata tests; no console or GPU operations.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$root/.build"
${CC:-cc} -std=c11 -Wall -Wextra -Werror -O1 -g -I "$root/native" \
  "$root/native/adapter_contract.c" "$root/tests/test_adapter_contract.c" \
  -lm -o "$root/.build/test_adapter_contract"
"$root/.build/test_adapter_contract"
