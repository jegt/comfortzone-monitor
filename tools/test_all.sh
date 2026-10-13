#!/bin/sh
set -eu
repo_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
cd "$repo_dir"
python3 -m unittest discover -s tools -p 'test_*.py'
${CXX:-c++} -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I comfortzone_wifi tests/measurements_test.cpp -o "$test_dir/measurements"
"$test_dir/measurements" < tests/fixtures/measurements.txt
${CXX:-c++} -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  -I comfortzone_wifi tests/controls_test.cpp -o "$test_dir/controls"
"$test_dir/controls" < tests/fixtures/end-poll.txt
