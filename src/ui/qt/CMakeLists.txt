qt_standard_project_setup(REQUIRES 6.8)

qt_add_executable(drumaspro_qt
    main.cpp
    EngineBridge.cpp
    EngineBridge.h
    Models.cpp
    Models.h)
set_target_properties(drumaspro_qt PROPERTIES OUTPUT_NAME drumaspro)

# The singleton property must be set before qt_add_qml_module reads the
# source list; otherwise the generated qmldir registers Theme as a plain type
# and every Theme.* binding is undefined at runtime.
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(drumaspro_qt
    URI DrumasPro
    VERSION 1.0
    QML_FILES
        Main.qml
        Theme.qml
        ThemeAmber.qml
        Knob.qml
        ChordButton.qml
        ToggleButton.qml
        LcdDisplay.qml
        LevelMeter.qml
        RotarySelector.qml
        Selector.qml
        SectionTitle.qml
        EffectSlot.qml)

target_include_directories(drumaspro_qt PRIVATE "${DRUMAS_SRC_DIR}")
target_compile_features(drumaspro_qt PRIVATE cxx_std_20)
target_compile_definitions(drumaspro_qt PRIVATE QT_NO_KEYWORDS)
if (MSVC)
    target_compile_options(drumaspro_qt PRIVATE /wd4702)
endif ()
target_link_libraries(drumaspro_qt PRIVATE
    drumas::compile_options
    drumas::bridge
    drumas::engine
    drumas::synth
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickLayouts)
