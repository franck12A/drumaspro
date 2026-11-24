# Test framework: Catch2 v3, fetched at configure time and pinned to an
# immutable commit rather than to a branch or a moving tag.
#
# Why FetchContent and not vcpkg: Catch2 is the only development dependency, it
# is used by the test executable alone, and FetchContent keeps the Windows build
# to "configure and go" without a second package manager, a toolchain file or a
# baseline that has to be kept in sync. The pin below is the commit of tag
# v3.9.1.
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG dfc2dff8d70d083c60c1c6986030e5389a867a93 # v3.9.1
    GIT_SHALLOW FALSE
    SYSTEM)
FetchContent_MakeAvailable(Catch2)

add_executable(
    drumas_tests
    support/AllocationCounter.cpp
    core/test_spsc_queue.cpp
    core/test_smoothed_value.cpp
    core/test_parameter_registry.cpp
    synth/test_sine_oscillator.cpp
    synth/test_oscillator.cpp
    synth/test_master_limiter.cpp
    synth/test_adsr.cpp
    synth/test_voice_pool.cpp
    engine/test_audio_graph.cpp)

target_include_directories(drumas_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(drumas_tests PRIVATE cxx_std_20)
target_link_libraries(
    drumas_tests
    PRIVATE drumas::compile_options
            drumas::core
            drumas::synth
            drumas::engine
            Catch2::Catch2WithMain)

# Tests are timing-sensitive by nature (block timing) but must never hang CI.
include(Catch)
catch_discover_tests(drumas_tests PROPERTIES TIMEOUT 120)
