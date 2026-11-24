# `fx` -- insert effects and the master limiter.
#
# Header-only, like the synth: every allocation happens in prepare(), every
# process() call is allocation-free and noexcept. DSPark types are confined to
# this directory.
add_library(drumas_fx INTERFACE)
add_library(drumas::fx ALIAS drumas_fx)

target_include_directories(drumas_fx INTERFACE "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_fx INTERFACE cxx_std_20)
target_link_libraries(drumas_fx INTERFACE drumas::core)
