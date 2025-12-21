#!/usr/bin/env bash

# Usage:
# ./compile.sh [-arena_tests] [-compile_commands]
set -euo pipefail

ARENA_TESTS=OFF
SANITIZE=OFF
DUMP=OFF
COMPILE_COMMANDS=OFF

for arg in "$@"; do
  case "$arg" in
    -arena_tests)
      ARENA_TESTS=ON
      SANITIZE=ON
      DUMP=ON
      ;;
    -compile_commands)
      COMPILE_COMMANDS=ON
      ;;
    *)
      echo "Unknown option: $arg"
      echo "Usage: $0 [-arena_tests] [-compile_commands]"
      exit 1
      ;;
  esac
done

cmake -S . -B build \
  -DMAXC_ARENA_TESTS=${ARENA_TESTS} \
  -DMAXC_SANITIZE=${SANITIZE} \
  -DMAXC_ARENA_DUMP=${DUMP} \
  -DMAXC_EXPORT_COMPILE_COMMANDS=${COMPILE_COMMANDS}

cmake --build build

