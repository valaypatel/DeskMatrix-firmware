#!/usr/bin/env bash
# tests/native/run_tests.sh
set -e
cd "$(dirname "$0")"
FAIL=0
for src in test_*.cpp; do
  bin="${src%.cpp}"
  echo "--- Building $src ---"
  g++ -std=c++17 -I. -I../../firmware/DeskMatrix -nostdinc++ -isystem /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include/c++/v1 -isysroot /Library/Developer/CommandLineTools/SDKs/MacOSX.sdk "$src" -o "/tmp/$bin"
  echo "--- Running $bin ---"
  "/tmp/$bin" || FAIL=1
done
exit $FAIL
