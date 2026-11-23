# `core` -- types and primitives shared by every module.
#
# Header-only by design: no UI, no Qt, no audio device knowledge. Each type here
# is either a plain value type or a thin, replaceable wrapper around a DSPark
# primitive.
add_library(drumas_core INTERFACE)
add_library(drumas::core ALIAS drumas_core)

target_include_directories(drumas_core INTERFACE "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_core INTERFACE cxx_std_20)
target_link_libraries(drumas_core INTERFACE drumas::compile_options drumas::dspark)
