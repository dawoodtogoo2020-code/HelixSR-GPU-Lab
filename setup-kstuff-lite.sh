#!/bin/sh
# Install the bundled Lite + A53 Local payload; no reference files or build needed.
exec sh "$(dirname "$0")/gpulab.sh" setup-lite "$@"
