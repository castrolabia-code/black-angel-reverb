add_executable(BlackAngelDSPTests
    DSPTests/TestFFT.cpp
    DSPTests/TestConvolution.cpp
    DSPTests/TestAlgorithmic.cpp
    DSPTests/TestHybrid.cpp)
target_link_libraries(BlackAngelDSPTests PRIVATE BlackAngelDSP Catch2::Catch2WithMain)
target_include_directories(BlackAngelDSPTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(MSVC)
    target_compile_options(BlackAngelDSPTests PRIVATE /O2)
else()
    target_compile_options(BlackAngelDSPTests PRIVATE -O2)
endif()

include(CTest)
include(Catch)
catch_discover_tests(BlackAngelDSPTests)
