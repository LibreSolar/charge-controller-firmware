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
platformio run -e mppt-2420-lc-v0.10 -e mppt-1210-hus-v0.7 -e pwm-2420-lus-v0.3
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running unit-tests -------------"
platformio test -e unit-test-native
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- Running static code checks -------------"
platformio check -e mppt-1210-hus-v0.7 -e pwm-2420-lus-v0.3
if [ $? != 0 ]; then
        exit 1;
fi

echo "---------- All tests passed -------------"
