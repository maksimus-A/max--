#!/usr/bin/env bash
# OLD
# clang mx_out.s -o mx_out.out

#NEW
clang mxout_new.s -o mxout_new.out
./mxout_new.out
rc=$?
echo "Exit code: $rc"