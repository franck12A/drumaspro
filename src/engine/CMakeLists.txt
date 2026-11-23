# `engine` -- device I/O, the real-time graph and its statistics.
#
# Depends on `core` only: the engine drives a core::IAudioProcessor and knows
# nothing about synths, effects or UI. miniaudio is a private implementation
# detail of this target.
add_library(drumas_engine STATIC AudioEngine.cpp AudioGraph.cpp)
add_library(drumas::engine ALIAS drumas_engine)

target_include_directories(drumas_engine PUBLIC "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_engine PUBLIC cxx_std_20)
target_link_libraries(drumas_engine PUBLIC drumas::core PRIVATE drumas::miniaudio)
