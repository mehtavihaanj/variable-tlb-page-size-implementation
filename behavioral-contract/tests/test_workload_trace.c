#include "workload_trace.h"
#include "tlb_contract.h"
#include <stdio.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
int main(void)
{
    const uint64_t expected[3][3] = {
        {0x01000010, 0x10000020, 0x40000030},
        {0x01001050, 0x10007060, 0x4000D070},
        {0x01000090, 0x1000E0A0, 0x4001A0B0}};
    for (unsigned i = 0; i < 3; ++i) {
        for (uint32_t asid = 1; asid <= 3; ++asid) {
            CHECK(workload_address(i, asid) == expected[i][asid - 1]);
        }
    }
    for (uint32_t asid = 1; asid <= 3; ++asid) {
        bool seen[1024] = {false};
        uint64_t blocks = region_size[asid] / 4096;
        for (uint64_t i = 0; i < blocks; ++i) {
            uint64_t va = workload_address(i, asid);
            CHECK(va >= region_base[asid] && va < region_base[asid] + region_size[asid]);
            if (va < region_base[asid] || va >= region_base[asid] + region_size[asid]) continue;
            uint64_t block = (va - region_base[asid]) / 4096;
            CHECK(!seen[block]);
            seen[block] = true;
            CHECK((va & 63) == asid * 16);
        }
        for (uint64_t block = 0; block < blocks; ++block) CHECK(seen[block]);
        CHECK(workload_address(UINT64_MAX, asid) >= region_base[asid]);
        CHECK(workload_address(UINT64_MAX, asid) < region_base[asid] + region_size[asid]);
    }
    /* Translating the same addresses through either policy preserves PA. */
    const unsigned orders[2][3] = {{12, 12, 12}, {12, 16, 21}};
    uint64_t hashes[2] = {WORKLOAD_HASH_SEED, WORKLOAD_HASH_SEED};
    uint64_t misses[2] = {0, 0};
    for (unsigned policy = 0; policy < 2; ++policy) {
        tlb_t tlb;
        tlb_init(&tlb);
        for (uint64_t i = 0; i < 1024; ++i) {
            for (uint32_t asid = 1; asid <= 3; ++asid) {
                uint64_t va = workload_address(i, asid);
                if (va < region_base[asid] || va >= region_base[asid] + region_size[asid]) continue;
                uint64_t mask = UINT32_MAX & ~( (UINT64_C(1) << orders[policy][asid-1]) - 1);
                uint64_t base = va & mask;
                tlb_entry_t entry = {base, ((uint64_t)asid << 28) + base - region_base[asid],
                                    mask, asid, TLB_PERMISSION_READ, true};
                uint64_t pa = 0;
                CHECK(tlb_lookup(&entry, 1, va, 32, asid, TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
                CHECK(pa == ((uint64_t)asid << 28) + va - region_base[asid]);
                tlb_result_t result = tlb_probe(&tlb, va, 32, asid, TLB_ACCESS_READ, &pa);
                if (result == TLB_RESULT_MISS) {
                    ++misses[policy];
                    result = tlb_refill(&tlb, &entry, va, 32, asid, TLB_ACCESS_READ, &pa);
                }
                CHECK(result == TLB_RESULT_HIT);
                CHECK(pa == ((uint64_t)asid << 28) + va - region_base[asid]);
                hashes[policy] = workload_hash(hashes[policy], va, asid);
            }
        }
        CHECK(tlb.stats.accesses == 3072);
        CHECK(tlb.stats.hits + tlb.stats.misses == tlb.stats.accesses);
        CHECK(tlb.stats.refills == misses[policy]);
    }
    CHECK(hashes[0] == hashes[1] && hashes[0] != WORKLOAD_HASH_SEED);
    if (TLB_ENTRY_COUNT == 8) {
        CHECK(misses[0] == 2050 && misses[1] == 5);
    }
    /* A complete address cycle repeats, including the cache-line offsets. */
    for (uint32_t asid = 1; asid <= 3; ++asid) {
        for (uint64_t i = 0; i < 64; ++i) {
            CHECK(workload_address(i, asid) == workload_address(i + 1024, asid));
        }
    }
    return failures ? 1 : 0;
}
