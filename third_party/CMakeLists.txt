# Third-party dependencies, vendored at fixed versions (see README.md in this
# directory for the exact tags, commits and hashes).

# ---------------------------------------------------------------------------
# miniaudio 0.11.26 -- single-file C audio I/O library, public domain or MIT-0.
# Only the device layer is used; the decoders, encoder, resource manager, node
# graph, engine and waveform generators are compiled out.
# ---------------------------------------------------------------------------
add_library(drumas_miniaudio STATIC miniaudio/miniaudio.c)
add_library(drumas::miniaudio ALIAS drumas_miniaudio)
target_include_directories(drumas_miniaudio SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio")
target_compile_definitions(
    drumas_miniaudio
    PRIVATE MA_NO_DECODING MA_NO_ENCODING MA_NO_RESOURCE_MANAGER MA_NO_NODE_GRAPH MA_NO_ENGINE
            MA_NO_GENERATION)
set_target_properties(drumas_miniaudio PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
if (MSVC)
    # Never /WX or /W4 on third-party code.
    target_compile_options(drumas_miniaudio PRIVATE /W3)
endif ()

# ---------------------------------------------------------------------------
# DSPark 1.7.0 -- header-only C++20 DSP, MIT. Only the Core/ headers are
# vendored for this stage; the include directory is SYSTEM so its diagnostics
# never take part in our /WX policy.
# ---------------------------------------------------------------------------
add_library(drumas_dspark INTERFACE)
add_library(drumas::dspark ALIAS drumas_dspark)
# The include root is `third_party/`, so DSPark headers are spelled
# <dspark/Core/...>. Prefixing them is not cosmetic: on Windows the filesystem is
# case-insensitive, and a bare <Core/SpscQueue.h> resolves to our own
# src/core/SpscQueue.h instead of DSPark's.
target_include_directories(drumas_dspark SYSTEM INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(drumas_dspark INTERFACE cxx_std_20)
