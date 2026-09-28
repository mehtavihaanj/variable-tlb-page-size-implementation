cmake_minimum_required(VERSION 3.16)
project(tlb_behavioral_contract C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(tlb_contract src/tlb_contract.c)
target_include_directories(tlb_contract PUBLIC include)

add_executable(test_contract tests/test_contract.c)
target_link_libraries(test_contract PRIVATE tlb_contract)

add_executable(test_tlb_state tests/test_tlb_state.c)
target_link_libraries(test_tlb_state PRIVATE tlb_contract)

add_executable(test_tlb_invalidation tests/test_tlb_invalidation.c)
target_link_libraries(test_tlb_invalidation PRIVATE tlb_contract)

enable_testing()
add_test(NAME behavioral_contract COMMAND test_contract)
add_test(NAME tlb_state COMMAND test_tlb_state)
add_test(NAME tlb_invalidation COMMAND test_tlb_invalidation)