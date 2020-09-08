#!/bin/bash
#
# This file performs the same checks as TravisCI and should be run after each commit and before
# pushing the commits
#

echo "---------- Trailing whitespace check -------------"
# check uncommitted changes
git diff --check HEAD
if [ $? != 0 ]; then
        exit 1;
fi
# check all commits starting from initial commit
git diff --check `git rev-list --max-parents=0 HEAD`..HEAD
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running compile check -------------"
platformio run -e mppt_2420_lc -e mppt_1210_hus -e mppt_2420_hc -e pwm_2420_lus
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running unit-tests -------------"
platformio test -e unit_test
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running static code checks -------------"
platformio check -e mppt_1210_hus -e pwm_2420_lus --skip-packages --fail-on-defect high
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- All tests passed -------------"
