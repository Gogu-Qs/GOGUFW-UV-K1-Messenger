#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cc -std=c11 -O2 -Wall -Wextra host_decmts01.c -o host_decmts01
./host_decmts01
rm -f host_decmts01
