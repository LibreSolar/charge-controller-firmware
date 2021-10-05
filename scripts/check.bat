@ECHO OFF
:: This file performs continuous integration checks and should be run before pushing new commits

echo "---------- Trailing whitespace check -------------"

:: check uncommitted changes
git diff --check HEAD
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

:: check all commits starting from initial commit
for /F %i in ('"git rev-list --max-parents=0 HEAD"') do git diff --check %i..HEAD
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

echo "---------- Running compile check -------------"

cd zephyr

west build -b mppt_2420_hc@0.2
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

west build -b mppt_2420_lc
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

west build -b mppt_1210_hus@0.7
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

west build -b pwm_2420_lus@0.3
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)

cd ..

echo "---------- Running unit-tests -------------"

cd tests
west build -b native_posix -t run
if errorlevel 1 (
    echo Failure Reason is %errorlevel%
    exit /b %errorlevel%
)
cd ..

:: echo "---------- Running static code checks -------------"

:: platformio check -e mppt_1210_hus -e pwm_2420_lus --skip-packages --fail-on-defect high
:: if errorlevel 1 (
::     echo Failure Reason is %errorlevel%
::     exit /b %errorlevel%
:: )

echo "---------- All tests passed -------------"
