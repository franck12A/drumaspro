# `synth` -- oscillators, envelopes, voices and the voice pool.
#
# Header-only: every class here is small, inlinable and allocation-free after
# prepare(). No UI, no audio device, no Qt. DSP primitives come from DSPark but
# are only ever named inside the concrete implementations in this directory.
add_library(drumas_synth INTERFACE)
add_library(drumas::synth ALIAS drumas_synth)

target_include_directories(drumas_synth INTERFACE "${DRUMAS_SRC_DIR}")
target_compile_features(drumas_synth INTERFACE cxx_std_20)
target_link_libraries(drumas_synth INTERFACE drumas::core)
