@ECHO OFF
:: This batch file performs the same checks as TravisCI and should be run after each commit and before
:: pushing the commits

echo "---------- Trailing whitespace check -------------"
REM git diff --check 'git rev-list --max-count=1 --reverse HEAD' ..$TRAVIS_BRANCH
git diff --check master
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

echo "---------- Running compile check -------------"
platformio run -e mppt_2420_lc -e mppt_1210_hus -e mppt_2420_hpx -e pwm_2420_lus
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

echo "---------- Running unit-tests -------------"
platformio test -e unit_test
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

echo "---------- Running static code checks -------------"
platformio check -e mppt_1210_hus -e pwm_2420_lus --skip-packages --fail-on-defect high
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

echo "---------- All tests passed -------------"