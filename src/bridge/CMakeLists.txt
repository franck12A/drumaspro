# `bridge` -- UI-independent control adapters and snapshots.
add_library(drumas_bridge INTERFACE)
add_library(drumas::bridge ALIAS drumas_bridge)

target_include_directories(drumas_bridge INTERFACE "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_bridge INTERFACE cxx_std_20)
target_link_libraries(drumas_bridge INTERFACE drumas::core drumas::engine drumas::music)
