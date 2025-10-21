@echo off
REM ESP32 Audio Streamer - Test Execution Script (Windows)
REM This script runs the complete test suite and validation procedures

echo ESP32 Audio Streamer - Test Execution Script
echo ===========================================
echo.

echo 1. COMPILATION VERIFICATION
echo ----------------------------

echo Building project...
platformio run
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Build failed! Please check compilation errors above.
    exit /b 1
)
echo ✅ PASSED: PlatformIO Build

echo.
echo 2. UNIT TESTS
echo -------------

echo Running unit tests...
platformio test -e unit -v
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Unit tests failed!
    set UNIT_TEST_STATUS=1
) else (
    echo ✅ PASSED: Unit Tests
    set UNIT_TEST_STATUS=0
)

echo.
echo 3. INTEGRATION TESTS
echo --------------------

echo Running integration tests...
platformio test -e integration -v
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Integration tests failed!
    set INTEGRATION_TEST_STATUS=1
) else (
    echo ✅ PASSED: Integration Tests
    set INTEGRATION_TEST_STATUS=0
)

echo.
echo 4. STRESS TESTS
echo ---------------

echo Running stress tests...
platformio test -e stress -v
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Stress tests failed!
    set STRESS_TEST_STATUS=1
) else (
    echo ✅ PASSED: Stress Tests
    set STRESS_TEST_STATUS=0
)

echo.
echo 5. PERFORMANCE TESTS
echo --------------------

echo Running performance tests...
platformio test -e performance -v
if %ERRORLEVEL% NEQ 0 (
    echo ❌ Performance tests failed!
    set PERFORMANCE_TEST_STATUS=1
) else (
    echo ✅ PASSED: Performance Tests
    set PERFORMANCE_TEST_STATUS=0
)

echo.
echo 6. MEMORY ANALYSIS
echo ------------------

echo Checking memory usage...
if exist ".pio\build\*\firmware.bin" (
    for %%F in (".pio\build\*\firmware.bin") do (
        echo Binary size: %%~zF bytes
        if %%~zF LSS 1000000 (
            echo ✅ Binary size acceptable
        ) else (
            echo ⚠️  Binary size large
        )
    )
) else (
    echo ❌ Binary not found
)

echo.
echo 7. SUMMARY
echo ----------
echo Test Results:
if %UNIT_TEST_STATUS% EQU 0 (echo ✅ PASSED: Unit Tests) else (echo ❌ FAILED: Unit Tests)
if %INTEGRATION_TEST_STATUS% EQU 0 (echo ✅ PASSED: Integration Tests) else (echo ❌ FAILED: Integration Tests)
if %STRESS_TEST_STATUS% EQU 0 (echo ✅ PASSED: Stress Tests) else (echo ❌ FAILED: Stress Tests)
if %PERFORMANCE_TEST_STATUS% EQU 0 (echo ✅ PASSED: Performance Tests) else (echo ❌ FAILED: Performance Tests)

echo.
set /a OVERALL_STATUS=%UNIT_TEST_STATUS%+%INTEGRATION_TEST_STATUS%+%STRESS_TEST_STATUS%+%PERFORMANCE_TEST_STATUS%

if %OVERALL_STATUS% EQU 0 (
    echo 🎉 ALL TESTS PASSED!
    echo The ESP32 Audio Streamer is ready for deployment.
) else (
    echo ❌ SOME TESTS FAILED
    echo Please review the failed tests above.
)

echo.
echo 8. NEXT STEPS
echo -------------
echo If all tests pass:
echo 1. Flash firmware: platformio run --target upload
echo 2. Monitor serial: platformio device monitor
echo 3. Test with hardware: Connect INMP441 and verify audio streaming
echo 4. Run long-term stability test
echo.
echo For hardware testing, use these serial commands:
echo - STATS     : View system statistics
echo - SIGNAL    : Check WiFi signal strength
echo - STATUS    : View current system state
echo - DEBUG 3   : Enable info-level logging

exit /b %OVERALL_STATUS%