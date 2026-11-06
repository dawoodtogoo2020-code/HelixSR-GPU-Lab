#!/bin/sh
# Build and install one Lite + A53 Local payload from your verified inputs.
exec sh "$(dirname "$0")/gpulab.sh" setup-lite "$@"
