if (NOT DEFINED DRUMAS_TEST_EXECUTABLE OR NOT DEFINED DRUMAS_QT_BIN_DIR)
    message(FATAL_ERROR "The UI test executable and Qt bin directory are required")
endif ()

set(ENV{PATH} "${DRUMAS_QT_BIN_DIR};$ENV{PATH}")
set(ENV{QT_QPA_PLATFORM} "offscreen")
set(ENV{QT_QUICK_BACKEND} "software")
set(ENV{QSG_RHI_BACKEND} "software")

execute_process(
    COMMAND "${DRUMAS_TEST_EXECUTABLE}"
    RESULT_VARIABLE _test_result)
if (NOT _test_result EQUAL 0)
    message(FATAL_ERROR "Qt UI tests failed with exit code ${_test_result}")
endif ()
