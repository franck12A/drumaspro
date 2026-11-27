qt_standard_project_setup(REQUIRES 6.8)

set(_qml_dir "${DRUMAS_SRC_DIR}/ui/qt")
set(_qml_files
    "${_qml_dir}/Main.qml"
    "${_qml_dir}/Theme.qml"
    "${_qml_dir}/ThemeAmber.qml"
    "${_qml_dir}/Knob.qml"
    "${_qml_dir}/ChordButton.qml"
    "${_qml_dir}/ToggleButton.qml"
    "${_qml_dir}/LcdDisplay.qml"
    "${_qml_dir}/LevelMeter.qml"
    "${_qml_dir}/RotarySelector.qml"
    "${_qml_dir}/Selector.qml"
    "${_qml_dir}/SectionTitle.qml"
    "${_qml_dir}/Screw.qml"
    "${_qml_dir}/PianoKeyboard.qml"
    "${_qml_dir}/SettingsDialog.qml"
    "${_qml_dir}/ChordEditorDialog.qml"
    "${_qml_dir}/EffectSlot.qml")

foreach (_qml_file IN LISTS _qml_files)
    get_filename_component(_qml_name "${_qml_file}" NAME)
    set_source_files_properties("${_qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${_qml_name}")
endforeach ()
set_source_files_properties("${_qml_dir}/Theme.qml" PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(drumas_ui_tests
    test_ui.cpp
    "${_qml_dir}/EngineBridge.cpp"
    "${_qml_dir}/EngineBridge.h"
    "${_qml_dir}/Models.cpp"
    "${_qml_dir}/Models.h"
    "${_qml_dir}/ValueFormat.cpp"
    "${_qml_dir}/ValueFormat.h"
    "${_qml_dir}/Settings.cpp"
    "${_qml_dir}/Settings.h")
qt_add_qml_module(drumas_ui_tests
    URI DrumasProTests
    VERSION 1.0
    QML_FILES ${_qml_files})

target_include_directories(drumas_ui_tests PRIVATE "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_ui_tests PRIVATE cxx_std_20)
target_compile_definitions(drumas_ui_tests PRIVATE QT_NO_KEYWORDS
    "DRUMAS_SCREENSHOT_DIR=\"${PROJECT_SOURCE_DIR}/docs/screenshots\"")
if (MSVC)
    target_compile_options(drumas_ui_tests PRIVATE /wd4702)
endif ()
target_link_libraries(drumas_ui_tests PRIVATE
    drumas::compile_options
    drumas::bridge
    drumas::engine
    drumas::synth
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts
    Qt6::Test)

get_filename_component(_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
add_test(
    NAME qt_ui_tests
    COMMAND
        "${CMAKE_COMMAND}"
        "-DDRUMAS_TEST_EXECUTABLE=$<TARGET_FILE:drumas_ui_tests>"
        "-DDRUMAS_QT_BIN_DIR=${_qt_bin_dir}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_ui_tests.cmake")
set_tests_properties(qt_ui_tests PROPERTIES TIMEOUT 120)

# The production module is not linked into the test executable, so this cheap
# check inspects the qmldir CMake actually generated for the application.
add_test(
    NAME qml_theme_singleton
    COMMAND
        "${CMAKE_COMMAND}"
        "-DDRUMAS_QMLDIR=${CMAKE_BINARY_DIR}/src/ui/qt/DrumasPro/qmldir"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_theme_singleton.cmake")
