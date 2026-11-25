# `music` -- pure music-theory value types and transformations.
add_library(drumas_music INTERFACE)
add_library(drumas::music ALIAS drumas_music)

target_include_directories(drumas_music INTERFACE "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_music INTERFACE cxx_std_20)
