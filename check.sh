#!/bin/bash
#
# This file performs the same checks as TravisCI and should be run after each commit and before
# pushing the commits
#

echo "---------- Trailing whitespace check -------------"
git diff --check `git rev-list HEAD | tail -n 1`..$TRAVIS_BRANCH
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running compile check -------------"
platformio run -e mppt_2420_lc -e mppt_1210_hus -e mppt_2420_hpx -e pwm_2420_lus
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running unit-tests -------------"
platformio test -e unit_test
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running static code checks -------------"
platformio check -e mppt_1210_hus -e pwm_2420_lus
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- All tests passed -------------"
