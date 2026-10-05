#include "tlb_contract.h"
#include "workload_trace.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PROCESS_COUNT 3
#define DEFAULT_ITERATIONS UINT64_C(50000)
#define MAX_ITERATIONS UINT64_C(1000000)

typedef struct {
    bool mixed_page_sizes;
} workload_policy_t;

typedef struct {
    uint64_t accesses;
    uint64_t hits;
    uint64_t misses;
} process_stats_t;

typedef struct {
    tlb_stats_t tlb;
    uint64_t trace_hash;
    process_stats_t process[PROCESS_COUNT + 1];
    double elapsed_ms;
    bool timing_available;
} workload_result_t;

static unsigned page_order_for(uint32_t asid, bool mixed_page_sizes)
{
    if (!mixed_page_sizes || asid == 1)
    {
        return 12;
    }
    return asid == 2 ? 16 : 21;
}

static bool walk_page(void *context, uint64_t virtual_address,
                      unsigned address_bits, uint32_t asid,
                      tlb_access_t access, tlb_entry_t *translation)
{
    const workload_policy_t *policy = context;
    if (address_bits != 32 || asid == 0 || asid > PROCESS_COUNT ||
        virtual_address < region_base[asid] ||
        virtual_address >= region_base[asid] + region_size[asid])
    {
        return false;
    }

    unsigned order = page_order_for(asid, policy->mixed_page_sizes);
    uint64_t page_size = UINT64_C(1) << order;
    uint64_t address_mask = UINT32_MAX & ~(page_size - 1);
    uint64_t virtual_base = virtual_address & address_mask;
    uint64_t page_index = (virtual_base - region_base[asid]) / page_size;
    uint64_t physical_base = ((uint64_t)asid << 28) + page_index * page_size;
    *translation = (tlb_entry_t){virtual_base, physical_base, address_mask,
                                 asid, TLB_PERMISSION_READ |
                                 TLB_PERMISSION_WRITE, true};
    (void)access;
    return true;
}

static bool run_workload(bool mixed_page_sizes, uint64_t iterations,
                         workload_result_t *result)
{
    memset(result, 0, sizeof(*result));
    result->trace_hash = WORKLOAD_HASH_SEED;
    tlb_t tlb;
    tlb_init(&tlb);
    workload_policy_t policy = {mixed_page_sizes};
    clock_t start = clock();

    for (uint64_t iteration = 0; iteration < iterations; ++iteration)
    {
        for (uint32_t asid = 1; asid <= PROCESS_COUNT; ++asid)
        {
            uint64_t virtual_address = workload_address(iteration, asid);
            result->trace_hash = workload_hash(result->trace_hash, virtual_address, asid);
            uint64_t previous_hits = tlb.stats.hits;
            uint64_t previous_misses = tlb.stats.misses;
            uint64_t physical_address = 0;

            tlb_result_t access_result = tlb_access(
                &tlb, virtual_address, 32, asid, TLB_ACCESS_READ,
                walk_page, &policy, &physical_address);
            if (access_result != TLB_RESULT_HIT || physical_address !=
                ((uint64_t)asid << 28) + virtual_address - region_base[asid])
            {
                return false;
            }

            process_stats_t *process = &result->process[asid];
            ++process->accesses;
            process->hits += tlb.stats.hits > previous_hits;
            process->misses += tlb.stats.misses > previous_misses;
        }
    }

    clock_t finish = clock();
    result->tlb = tlb.stats;
    result->timing_available = start != (clock_t)-1 &&
                               finish != (clock_t)-1 && finish >= start;
    if (result->timing_available)
    {
        result->elapsed_ms =
            1000.0 * (double)(finish - start) / (double)CLOCKS_PER_SEC;
    }
    return true;
}

static void print_result(const char *name, const workload_result_t *result)
{
    printf("trace version=1 hash=%" PRIu64 "\n", result->trace_hash);
    printf("%s: accesses=%" PRIu64 " hits=%" PRIu64 " misses=%" PRIu64
           " walks=%" PRIu64 " refills=%" PRIu64 " evictions=%" PRIu64
           " occupancy=%zu/%zu storage=%zu bytes",
           name, result->tlb.accesses, result->tlb.hits, result->tlb.misses,
           result->tlb.page_walks, result->tlb.refills, result->tlb.evictions,
           result->tlb.current_occupancy, (size_t)TLB_ENTRY_COUNT,
           tlb_storage_bytes());
    if (result->timing_available && result->tlb.accesses != 0)
    {
        printf(" cpu_ms=%.3f ns_per_access=%.1f", result->elapsed_ms,
               result->elapsed_ms * 1000000.0 / result->tlb.accesses);
    }
    puts("");
    for (size_t asid = 1; asid <= PROCESS_COUNT; ++asid)
    {
        const process_stats_t *process = &result->process[asid];
        printf("  ASID %zu: accesses=%" PRIu64 " hits=%" PRIu64
               " misses=%" PRIu64 "\n", asid, process->accesses,
               process->hits, process->misses);
    }
}

static uint64_t parse_iterations(int argc, char **argv)
{
    if (argc == 1) return DEFAULT_ITERATIONS;
    if (argc != 3 || strcmp(argv[1], "--iterations") != 0) return 0;
    char *end = NULL;
    uint64_t parsed = strtoull(argv[2], &end, 10);
    return end != argv[2] && *end == '\0' && parsed > 0 &&
           parsed <= MAX_ITERATIONS ? parsed : 0;
}

int main(int argc, char **argv)
{
    uint64_t iterations = parse_iterations(argc, argv);
    if (iterations == 0)
    {
        fprintf(stderr, "Usage: tlb_workload [--iterations N]\n");
        return 2;
    }

    workload_result_t fixed, mixed;
    if (!run_workload(false, iterations, &fixed) ||
        !run_workload(true, iterations, &mixed))
    {
        fputs("workload produced an unexpected translation result\n", stderr);
        return 1;
    }
    uint64_t small_working_set = iterations < 2 ? iterations : 2;
    if (fixed.process[1].misses != small_working_set ||
        fixed.process[2].misses != iterations ||
        fixed.process[3].misses != iterations ||
        mixed.process[1].misses != small_working_set ||
        mixed.process[2].misses != 1 ||
        mixed.process[3].misses != small_working_set ||
        mixed.tlb.misses >= fixed.tlb.misses)
    {
        fputs("determinism or mixed-page miss-count check failed\n", stderr);
        return 1;
    }

    printf("Deterministic round-robin process mix; %" PRIu64
           " accesses per ASID\n", iterations);
    print_result("all-4KiB", &fixed);
    print_result("mixed-4KiB-64KiB-2MiB", &mixed);
    return 0;
}