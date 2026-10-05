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

add_executable(test_tlb_lru tests/test_tlb_lru.c)
target_link_libraries(test_tlb_lru PRIVATE tlb_contract)

add_executable(test_tlb_metrics tests/test_tlb_metrics.c)
target_link_libraries(test_tlb_metrics PRIVATE tlb_contract)

add_executable(tlb_workload tests/tlb_workload.c)
target_link_libraries(tlb_workload PRIVATE tlb_contract)

enable_testing()
add_test(NAME behavioral_contract COMMAND test_contract)
add_test(NAME tlb_state COMMAND test_tlb_state)
add_test(NAME tlb_invalidation COMMAND test_tlb_invalidation)
add_test(NAME tlb_lru COMMAND test_tlb_lru)
add_test(NAME tlb_metrics COMMAND test_tlb_metrics)
add_test(NAME workload_integration COMMAND tlb_workload --iterations 2000)
add_executable(test_tlb_refill tests/test_tlb_refill.c)
target_link_libraries(test_tlb_refill PRIVATE tlb_contract)
add_test(NAME tlb_refill COMMAND test_tlb_refill)

add_executable(test_tlb_probe tests/test_tlb_probe.c)
target_link_libraries(test_tlb_probe PRIVATE tlb_contract)
add_test(NAME tlb_probe COMMAND test_tlb_probe)

add_executable(test_tlb_overlap tests/test_tlb_overlap.c)
target_link_libraries(test_tlb_overlap PRIVATE tlb_contract)
add_test(NAME tlb_overlap COMMAND test_tlb_overlap)
