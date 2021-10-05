#!/bin/bash
#
# This file performs continuous integration checks and should be run before pushing new commits
#

# exit when any command fails
set -e

# use board-specific directory
west config build.dir-fmt "build/{board}"

echo "---------- Trailing whitespace check -------------"

# check uncommitted changes
git diff --check HEAD

# check all commits starting from initial commit
git diff --check `git rev-list --max-parents=0 HEAD`..HEAD

echo "---------- Running compile check -------------"

cd zephyr
west build -b mppt_2420_hc@0.2
west build -b mppt_2420_lc
west build -b mppt_1210_hus@0.7
west build -b pwm_2420_lus@0.3
cd ..

echo "---------- Running unit-tests -------------"

cd tests
west build -b native_posix -t run
cd ..

#echo "---------- Running static code checks -------------"

#platformio check -e mppt_1210_hus -e pwm_2420_lus --skip-packages --fail-on-defect high

echo "---------- All tests passed -------------"
