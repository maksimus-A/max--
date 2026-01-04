#!/usr/bin/env bash

clang mx_out.s -o mx_out.out
./mx_out.out
rc=$?
echo "Exit code: $rc"