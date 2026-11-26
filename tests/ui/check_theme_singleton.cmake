# Regression guard for the production QML module: Theme.qml must be declared
# with QT_QML_SINGLETON_TYPE before qt_add_qml_module, otherwise the generated
# qmldir registers it as a plain type and every Theme.* binding is undefined.
if(NOT DEFINED DRUMAS_QMLDIR)
    message(FATAL_ERROR "DRUMAS_QMLDIR is required")
endif()
if(NOT EXISTS "${DRUMAS_QMLDIR}")
    message(FATAL_ERROR "Generated qmldir not found: ${DRUMAS_QMLDIR}")
endif()

file(READ "${DRUMAS_QMLDIR}" qmldir_contents)
if(NOT qmldir_contents MATCHES "singleton[ \t\r\n]+Theme[ \t\r\n]+")
    message(FATAL_ERROR
        "Theme is not registered as a singleton in ${DRUMAS_QMLDIR}:\n"
        "${qmldir_contents}")
endif()
