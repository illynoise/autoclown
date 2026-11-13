add_executable(AutoClownTests
    TestMain.cpp
    PitchDetectorTests.cpp
    ScaleMapperTests.cpp
    RetuneTests.cpp
    ShifterTests.cpp
    AnalysisTests.cpp)
target_link_libraries(AutoClownTests PRIVATE AutoClownDSP)
target_compile_definitions(AutoClownTests PRIVATE AC_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")
add_test(NAME AutoClownTests COMMAND AutoClownTests)

add_executable(AutoClownBench CpuBenchmark.cpp)
target_link_libraries(AutoClownBench PRIVATE AutoClownDSP)
