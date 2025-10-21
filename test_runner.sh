#!/bin/bash

# ESP32 Audio Streamer - Test Execution Script
# This script runs the complete test suite and validation procedures

echo "ESP32 Audio Streamer - Test Execution Script"
echo "==========================================="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print status
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✅ PASSED${NC}: $2"
    else
        echo -e "${RED}❌ FAILED${NC}: $2"
    fi
}

# Function to run a test
run_test() {
    echo "Running: $1"
    eval $1
    return $?
}

echo "1. COMPILATION VERIFICATION"
echo "----------------------------"

# Check if PlatformIO is available
if ! command -v pio &> /dev/null; then
    echo -e "${YELLOW}⚠️  PlatformIO not found${NC}"
    echo "Please install PlatformIO or run: pip install platformio"
    exit 1
fi

echo "Building project..."
run_test "pio run"
BUILD_STATUS=$?
print_status $BUILD_STATUS "PlatformIO Build"

if [ $BUILD_STATUS -ne 0 ]; then
    echo -e "${RED}Build failed! Please check compilation errors above.${NC}"
    exit 1
fi

echo ""
echo "2. STATIC ANALYSIS"
echo "-------------------"

echo "Running Cppcheck..."
if command -v cppcheck &> /dev/null; then
    run_test "cppcheck --enable=all --suppress=missingIncludeSystem src/ --error-exitcode=1"
    CPPCHECK_STATUS=$?
    print_status $CPPCHECK_STATUS "Static Analysis (Cppcheck)"
else
    echo -e "${YELLOW}Cppcheck not available${NC}"
fi

echo ""
echo "3. UNIT TESTS"
echo "-------------"

echo "Running unit tests..."
run_test "pio test -e unit -v"
UNIT_TEST_STATUS=$?
print_status $UNIT_TEST_STATUS "Unit Tests"

echo ""
echo "4. INTEGRATION TESTS"
echo "--------------------"

echo "Running integration tests..."
run_test "pio test -e integration -v"
INTEGRATION_TEST_STATUS=$?
print_status $INTEGRATION_TEST_STATUS "Integration Tests"

echo ""
echo "5. STRESS TESTS"
echo "---------------"

echo "Running stress tests..."
run_test "pio test -e stress -v"
STRESS_TEST_STATUS=$?
print_status $STRESS_TEST_STATUS "Stress Tests"

echo ""
echo "6. PERFORMANCE TESTS"
echo "--------------------"

echo "Running performance tests..."
run_test "pio test -e performance -v"
PERFORMANCE_TEST_STATUS=$?
print_status $PERFORMANCE_TEST_STATUS "Performance Tests"

echo ""
echo "7. MEMORY ANALYSIS"
echo "------------------"

echo "Checking memory usage..."
if [ -f ".pio/build/*/firmware.bin" ]; then
    BINARY_SIZE=$(stat -c%s .pio/build/*/firmware.bin)
    echo "Binary size: $BINARY_SIZE bytes"
    
    if [ $BINARY_SIZE -lt 1000000 ]; then
        echo -e "${GREEN}✅ Binary size acceptable${NC}"
    else
        echo -e "${YELLOW}⚠️  Binary size large${NC}"
    fi
else
    echo -e "${RED}❌ Binary not found${NC}"
fi

echo ""
echo "8. SUMMARY"
echo "----------"

echo "Test Results:"
print_status $BUILD_STATUS "Compilation"
print_status $UNIT_TEST_STATUS "Unit Tests"
print_status $INTEGRATION_TEST_STATUS "Integration Tests"
print_status $STRESS_TEST_STATUS "Stress Tests"
print_status $PERFORMANCE_TEST_STATUS "Performance Tests"

# Overall status
OVERALL_STATUS=0
if [ $BUILD_STATUS -ne 0 ] || [ $UNIT_TEST_STATUS -ne 0 ] || [ $INTEGRATION_TEST_STATUS -ne 0 ] || [ $STRESS_TEST_STATUS -ne 0 ] || [ $PERFORMANCE_TEST_STATUS -ne 0 ]; then
    OVERALL_STATUS=1
fi

echo ""
if [ $OVERALL_STATUS -eq 0 ]; then
    echo -e "${GREEN}🎉 ALL TESTS PASSED!${NC}"
    echo "The ESP32 Audio Streamer is ready for deployment."
else
    echo -e "${RED}❌ SOME TESTS FAILED${NC}"
    echo "Please review the failed tests above."
fi

echo ""
echo "9. NEXT STEPS"
echo "-------------"
echo "If all tests pass:"
echo "1. Flash firmware: pio run --target upload"
echo "2. Monitor serial: pio device monitor"
echo "3. Test with hardware: Connect INMP441 and verify audio streaming"
echo "4. Run long-term stability test"
echo ""
echo "For hardware testing, use these serial commands:"
echo "- STATS     : View system statistics"
echo "- SIGNAL    : Check WiFi signal strength"
echo "- STATUS    : View current system state"
echo "- DEBUG 3   : Enable info-level logging"

exit $OVERALL_STATUS