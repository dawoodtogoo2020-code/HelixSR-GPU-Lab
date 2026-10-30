#!/bin/sh
# Remove everything HelixSR GPU Lab installed on your PS5:  sh uninstall.sh
exec sh "$(dirname "$0")/gpulab.sh" uninstall "$@"
