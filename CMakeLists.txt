cmake_minimum_required(VERSION 3.10)

project(ghar VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
    add_link_options($<$<CONFIG:RELEASE>:-Wl,--gc-sections>)
endif()

add_executable(ghar
    dont_read_me_src/main.cpp
    dont_read_me_src/util.cpp
    dont_read_me_src/tsv.cpp
    dont_read_me_src/store.cpp
    dont_read_me_src/output.cpp
    dont_read_me_src/config.cpp
    dont_read_me_src/verify.cpp
    dont_read_me_src/work.cpp
    dont_read_me_src/doctor.cpp
    dont_read_me_src/compile_check.cpp
    dont_read_me_src/cuda_check.cpp
    dont_read_me_src/symbols_check.cpp
    dont_read_me_src/import_check.cpp
    dont_read_me_src/run_check.cpp
    dont_read_me_src/bench_check.cpp
    dont_read_me_src/assert_check.cpp
    dont_read_me_src/test_check.cpp
    dont_read_me_src/python_check.cpp
    dont_read_me_src/gate.cpp
)

target_include_directories(ghar PRIVATE dont_read_me_src)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(ghar PRIVATE -static-libgcc -static-libstdc++)
endif()

install(TARGETS ghar RUNTIME DESTINATION bin)

# --- tests & benchmarks (ctest) ----------------------------------------------
enable_testing()
set(GHAR_BIN "$<TARGET_FILE:ghar>")

add_test(NAME integration_cli_smoke
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_cli_smoke.sh")
add_test(NAME integration_compile_run
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_compile_run.sh")
add_test(NAME integration_import_assert_gate
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_import_assert_gate.sh")
add_test(NAME integration_cuda
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_cuda.sh")
add_test(NAME integration_verify
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_verify.sh")
add_test(NAME integration_work
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_work.sh")
add_test(NAME integration_bugs
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_bugs.sh")
add_test(NAME integration_python_torch
    COMMAND bash "${CMAKE_SOURCE_DIR}/tests/test_python_torch.sh")
add_test(NAME hallucination_suite
    COMMAND bash "${CMAKE_SOURCE_DIR}/benchmarks/run_hallucination_suite.sh")
add_test(NAME py_torch_suite
    COMMAND bash "${CMAKE_SOURCE_DIR}/benchmarks/run_py_torch_suite.sh")
add_test(NAME perf_uplift
    COMMAND bash "${CMAKE_SOURCE_DIR}/benchmarks/run_perf_uplift.sh")
add_test(NAME real_model_eval
    COMMAND bash "${CMAKE_SOURCE_DIR}/benchmarks/run_real_model_eval.sh")
add_test(NAME uplift_report
    COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/run_uplift_report.sh")

set_tests_properties(
    integration_cli_smoke
    integration_compile_run
    integration_import_assert_gate
    integration_cuda
    integration_verify
    integration_work
    integration_bugs
    integration_python_torch
    hallucination_suite
    py_torch_suite
    perf_uplift
    real_model_eval
    uplift_report
    PROPERTIES
        ENVIRONMENT "GHAR=${CMAKE_BINARY_DIR}/ghar;GHAR_ROOT=${CMAKE_SOURCE_DIR};OUT_DIR=${CMAKE_BINARY_DIR}/results;SCRATCH=${CMAKE_BINARY_DIR}/results/real_model_scratch"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Long suites not in default "ctest" quick path unless label matched
set_tests_properties(hallucination_suite py_torch_suite perf_uplift real_model_eval uplift_report
    PROPERTIES LABELS "bench;uplift")
set_tests_properties(
    integration_cli_smoke integration_compile_run
    integration_import_assert_gate integration_cuda integration_verify
    integration_work integration_bugs integration_python_torch
    PROPERTIES LABELS "integration")
