cmake_minimum_required(VERSION 3.16)
project(tlb_behavioral_contract C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(tlb_contract src/tlb_contract.c)
target_include_directories(tlb_contract PUBLIC include)

add_executable(test_contract tests/test_contract.c)
target_link_libraries(test_contract PRIVATE tlb_contract)

enable_testing()
add_test(NAME behavioral_contract COMMAND test_contract)