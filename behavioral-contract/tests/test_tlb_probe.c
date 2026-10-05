#include "tlb_contract.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static tlb_result_t probe(tlb_t *tlb, uint64_t va, tlb_access_t access,
                          uint64_t *pa)
{
    return tlb_probe(tlb, va, 32, 7, access, pa);
}

int main(void)
{
    tlb_t tlb, before;
    tlb_init(&tlb);
    uint64_t pa = 99;
    CHECK(probe(&tlb, 0x102A, TLB_ACCESS_READ, &pa) == TLB_RESULT_MISS);
    CHECK(pa == 99 && tlb.stats.accesses == 1 && tlb.stats.misses == 1);
    CHECK(tlb.stats.page_walks == 0 && tlb.stats.unresolved_misses == 0);
    CHECK(tlb.stats.refills == 0 && tlb.use_sequence == 0);
    for (unsigned page = 1; page <= TLB_ENTRY_COUNT; ++page) {
        tlb_entry_t entry = {(uint64_t)page << 12, (uint64_t)page << 12,
            UINT64_C(0xFFFFF000), 7, TLB_PERMISSION_READ, true};
        CHECK(tlb_refill(&tlb, &entry, entry.virtual_base, 32, 7,
                        TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    }
    memcpy(&before, &tlb, sizeof(tlb));
    CHECK(tlb_lookup(tlb.entries, TLB_ENTRY_COUNT, 0x102A, 32, 7,
                     TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0);
    CHECK(probe(&tlb, 0x102A, TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    CHECK(pa == 0x102A && tlb.stats.hits == 1);
    CHECK(tlb.last_used[0] > before.last_used[0]);
    uint64_t recency = tlb.last_used[0];
    pa = 99;
    CHECK(probe(&tlb, 0x102A, TLB_ACCESS_WRITE, &pa)
          == TLB_RESULT_PERMISSION_FAULT);
    CHECK(pa == 99 && tlb.last_used[0] > recency);
    CHECK(tlb.stats.hits == 2 && tlb.stats.permission_faults == 1);
    CHECK(tlb.stats.hits_by_page_order[12] == 2 && tlb.stats.accesses == 3);
    memcpy(&before, &tlb, sizeof(tlb));
    CHECK(probe(&tlb, 0, TLB_ACCESS_READ, &pa) == TLB_RESULT_MISS);
    CHECK(memcmp(before.entries, tlb.entries, sizeof(tlb.entries)) == 0);
    CHECK(memcmp(before.last_used, tlb.last_used, sizeof(tlb.last_used)) == 0);
    CHECK(pa == 99 && tlb.stats.misses == 2 && tlb.stats.page_walks == 0);
    memcpy(&before, &tlb, sizeof(tlb));
    CHECK(probe(&tlb, UINT64_MAX, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_INVALID_ARGUMENT);
    CHECK(probe(&tlb, 0, (tlb_access_t)99, &pa) == TLB_RESULT_INVALID_ARGUMENT);
    CHECK(probe(&tlb, 0, TLB_ACCESS_READ, NULL) == TLB_RESULT_INVALID_ARGUMENT);
    CHECK(probe(NULL, 0, TLB_ACCESS_READ, &pa) == TLB_RESULT_INVALID_ARGUMENT);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    /* Refill must use recency established by the independent probe. */
    tlb_entry_t next = {(uint64_t)(TLB_ENTRY_COUNT + 1) << 12, 0,
        UINT64_C(0xFFFFF000), 7, TLB_PERMISSION_READ, true};
    CHECK(tlb_refill(&tlb, &next, next.virtual_base, 32, 7,
                    TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    CHECK(tlb.stats.evictions == 1);
    CHECK(probe(&tlb, TLB_ENTRY_COUNT > 1 ? 0x2000 : 0x1000,
                TLB_ACCESS_READ, &pa) == TLB_RESULT_MISS);
    /* Reuse an invalid slot even when valid slots have older timestamps. */
    tlb.entries[0].valid = false;
    uint64_t evictions = tlb.stats.evictions;
    next.virtual_base += 0x1000;
    CHECK(tlb_refill(&tlb, &next, next.virtual_base, 32, 7,
                    TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    CHECK(tlb.stats.evictions == evictions && tlb.entries[0].valid);
    if (TLB_ENTRY_COUNT > 1) {
        tlb.entries[1] = tlb.entries[0];
        memcpy(&before, &tlb, sizeof(tlb));
        CHECK(probe(&tlb, next.virtual_base, TLB_ACCESS_READ, &pa)
              == TLB_RESULT_AMBIGUOUS);
        CHECK(tlb.stats.ambiguous_lookups == 1);
        CHECK(tlb.stats.accesses == before.stats.accesses + 1);
        CHECK(tlb.stats.hits == before.stats.hits);
        CHECK(tlb.stats.misses == before.stats.misses);
        CHECK(tlb.use_sequence == before.use_sequence);
    }
    return failures ? 1 : 0;
}
