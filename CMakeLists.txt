cmake_minimum_required(VERSION 3.22)

project(KBReverbBlackAngel VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(BA_BUILD_PLUGIN "Build the VST3 plugin (downloads JUCE)" ON)
option(BA_BUILD_TESTS  "Build the DSP test suite (downloads Catch2)" ON)
option(BA_SANITIZE     "Build tests with Address/UB sanitizers (GCC/Clang)" OFF)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# ------------------------------------------------------------------------------
# DSP core: no GUI, no JUCE. Shared by the plugin, the tests and the tools.
add_library(BlackAngelDSP STATIC
    Source/DSP/ConvolutionEngine.cpp
    Source/DSP/IRSynth.cpp
    Source/DSP/HybridReverb.cpp)
target_include_directories(BlackAngelDSP PUBLIC Source)

if(MSVC)
    target_compile_options(BlackAngelDSP PRIVATE /W4 /O2)
else()
    target_compile_options(BlackAngelDSP PRIVATE -Wall -Wextra -O2)
endif()

if(BA_SANITIZE AND NOT MSVC)
    target_compile_options(BlackAngelDSP PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(BlackAngelDSP PUBLIC -fsanitize=address,undefined)
    target_compile_definitions(BlackAngelDSP PUBLIC BA_SANITIZED=1)
endif()

# ------------------------------------------------------------------------------
if(BA_BUILD_PLUGIN)
    # JUCE is used under the AGPLv3 (see LICENSE). Version pinned for reproducible builds.
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(Source)
endif()

# ------------------------------------------------------------------------------
if(BA_BUILD_TESTS)
    enable_testing()
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)
    add_subdirectory(Tests)
endif()
