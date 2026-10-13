#!/bin/sh
set -eu

cd "$(dirname "$0")/.."
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM

gcc -std=c11 -Wall -Wextra -Werror \
  tests/test_ap_stage.c break_hv_payload/break_hv.c \
  -o "$test_dir/test_ap_stage"
"$test_dir/test_ap_stage"

gcc -std=c11 -Wall -Wextra -Werror -I./include \
  tests/test_khook_contract.c khook.c \
  -o "$test_dir/test_khook_contract"
"$test_dir/test_khook_contract"
