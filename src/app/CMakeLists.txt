# `app` -- the only target that knows both the engine and the synth.
add_executable(drumaspro_console main.cpp)
target_include_directories(drumaspro_console PRIVATE "${DRUMAS_SRC_DIR}")
target_compile_features(drumaspro_console PRIVATE cxx_std_20)
target_link_libraries(drumaspro_console PRIVATE
    drumas::compile_options
    drumas::engine
    drumas::synth)
set_target_properties(drumaspro_console PROPERTIES OUTPUT_NAME drumaspro_console)
