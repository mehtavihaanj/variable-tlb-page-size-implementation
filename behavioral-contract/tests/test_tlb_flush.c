#include "tlb_contract.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static tlb_entry_t entry(uint32_t asid)
{
    return (tlb_entry_t){0, 0, 0xFFFFF000, asid, TLB_PERMISSION_READ, true};
}
int main(void)
{
    tlb_t tlb, before;
    uint64_t pa = 99;
    size_t removed = 99;
    CHECK(tlb_init_config(&tlb, TLB_PAGE_ORDERS_X86_64));
    for (unsigned i = 0; i < TLB_ENTRY_COUNT; ++i) {
        tlb_entry_t e = entry(i % 2);
        e.virtual_base = e.physical_base = (uint64_t)i << 12;
        CHECK(tlb_refill(&tlb, &e, e.virtual_base, 32, e.asid,
                        TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    }
    CHECK(tlb_probe(&tlb, 0, 32, 0, TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    uint64_t token = tlb.invalidation_generation;
    memcpy(&before, &tlb, sizeof(tlb));
    CHECK(tlb_flush_asid(&tlb, 0, &removed) == TLB_INVALIDATION_REMOVED);
    CHECK(removed == (TLB_ENTRY_COUNT + 1) / 2);
    CHECK(tlb.stats.current_occupancy == TLB_ENTRY_COUNT / 2);
    CHECK(tlb.stats.invalidations == removed);
    CHECK(tlb.stats.peak_occupancy == TLB_ENTRY_COUNT);
    CHECK(tlb.stats.refills == before.stats.refills && tlb.stats.hits == 1);
    CHECK(tlb.allowed_page_orders == TLB_PAGE_ORDERS_X86_64);
    CHECK(tlb.invalidation_generation != token);
    for (unsigned i = 0; i < TLB_ENTRY_COUNT; ++i) {
        CHECK(tlb.entries[i].valid == (i % 2 != 0));
        if (i % 2 != 0) {
            CHECK(memcmp(&tlb.entries[i], &before.entries[i], sizeof(tlb_entry_t)) == 0);
            CHECK(tlb.last_used[i] == before.last_used[i]);
        }
    }
    tlb_entry_t e = entry(0);
    pa = 99;
    CHECK(tlb_complete_walk(&tlb, token, &e, 0, 32, 0,
                           TLB_ACCESS_READ, &pa) == TLB_RESULT_STALE_REFILL);
    CHECK(pa == 99);
    token = tlb.invalidation_generation;
    CHECK(tlb_flush_asid(&tlb, UINT32_MAX, &removed) == TLB_INVALIDATION_NO_MATCH);
    CHECK(removed == 0 && tlb.invalidation_generation != token);
    CHECK(tlb_complete_walk(&tlb, token, &e, 0, 32, 0,
                           TLB_ACCESS_READ, &pa) == TLB_RESULT_STALE_REFILL);
    size_t remaining = tlb.stats.current_occupancy;
    CHECK(tlb_flush_all(&tlb, &removed) ==
          (remaining ? TLB_INVALIDATION_REMOVED : TLB_INVALIDATION_NO_MATCH));
    CHECK(removed == remaining && tlb.stats.current_occupancy == 0);
    CHECK(tlb.stats.invalidations == TLB_ENTRY_COUNT);
    CHECK(tlb.stats.peak_occupancy == TLB_ENTRY_COUNT);
    CHECK(tlb.stats.evictions == 0 && tlb.stats.hits == 1);
    for (unsigned i = 0; i < TLB_ENTRY_COUNT; ++i) CHECK(!tlb.entries[i].valid);
    token = tlb.invalidation_generation;
    CHECK(tlb_flush_all(&tlb, &removed) == TLB_INVALIDATION_NO_MATCH);
    CHECK(removed == 0 && tlb.invalidation_generation != token);
    CHECK(tlb_complete_walk(&tlb, token, &e, 0, 32, 0,
                           TLB_ACCESS_READ, &pa) == TLB_RESULT_STALE_REFILL);
    CHECK(tlb_complete_walk(&tlb, tlb.invalidation_generation, &e, 0, 32, 0,
                           TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    CHECK(tlb.stats.evictions == 0 && tlb.stats.current_occupancy == 1);
    memcpy(&before, &tlb, sizeof(tlb));
    removed = 99;
    CHECK(tlb_flush_all(NULL, &removed) == TLB_INVALIDATION_INVALID_ARGUMENT);
    CHECK(tlb_flush_asid(NULL, 0, &removed) == TLB_INVALIDATION_INVALID_ARGUMENT);
    CHECK(removed == 99);
    CHECK(tlb_flush_all(&tlb, NULL) == TLB_INVALIDATION_INVALID_ARGUMENT);
    CHECK(tlb_flush_asid(&tlb, 0, NULL) == TLB_INVALIDATION_INVALID_ARGUMENT);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0);
    CHECK(tlb_flush_all(&tlb, &removed) == TLB_INVALIDATION_REMOVED);
    e = entry(UINT32_MAX);
    CHECK(tlb_refill(&tlb, &e, 0, 32, UINT32_MAX,
                    TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    CHECK(tlb_flush_asid(&tlb, 0, &removed) == TLB_INVALIDATION_NO_MATCH);
    CHECK(tlb.stats.current_occupancy == 1);
    CHECK(tlb_flush_asid(&tlb, UINT32_MAX, &removed) == TLB_INVALIDATION_REMOVED);
    CHECK(removed == 1 && tlb.stats.current_occupancy == 0);
    CHECK(tlb_refill(&tlb, &e, 0, 32, UINT32_MAX,
                    TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    tlb.invalidation_generation = UINT64_MAX - 1;
    CHECK(tlb_flush_all(&tlb, &removed) == TLB_INVALIDATION_REMOVED);
    CHECK(tlb_flush_asid(&tlb, 0, &removed) == TLB_INVALIDATION_NO_MATCH);
    CHECK(tlb.invalidation_generation == UINT64_MAX);
    CHECK(tlb_complete_walk(&tlb, 0, &e, 0, 32, 0,
                           TLB_ACCESS_READ, &pa) == TLB_RESULT_STALE_REFILL);
    return failures ? 1 : 0;
}
