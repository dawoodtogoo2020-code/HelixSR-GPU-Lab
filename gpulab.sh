#!/bin/sh
# HelixSR GPU Lab: runs tools/gpulab.py with Python 3.10+.
#   sh gpulab.sh status | on GAME | off GAME | install | uninstall
cd "$(dirname "$0")" || exit 1
for p in python3 python; do
  if command -v "$p" >/dev/null 2>&1 && "$p" -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
    exec "$p" -B tools/gpulab.py "$@"
  fi
done
echo "Python 3.10 or newer is needed: install it from https://www.python.org/downloads/ and run this again."
exit 1
