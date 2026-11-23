# `app` -- the only target that knows both the engine and the synth.
add_executable(drumas_app main.cpp)
target_include_directories(drumas_app PRIVATE "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_app PRIVATE cxx_std_20)
target_link_libraries(drumas_app PRIVATE drumas::compile_options drumas::engine drumas::synth)
set_target_properties(drumas_app PROPERTIES OUTPUT_NAME drumaspro)
