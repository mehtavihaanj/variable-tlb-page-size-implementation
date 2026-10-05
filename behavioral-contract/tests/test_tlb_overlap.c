#include "tlb_contract.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static tlb_entry_t mapping(uint64_t base, uint64_t mask)
{
    return (tlb_entry_t){base, base, mask, 7, TLB_PERMISSION_READ, true};
}
static tlb_result_t fill(tlb_t *tlb, tlb_entry_t entry, uint64_t *pa)
{
    return tlb_refill(tlb, &entry, entry.virtual_base, 32, entry.asid,
                      TLB_ACCESS_READ, pa);
}
static bool walk(void *context, uint64_t va, unsigned bits, uint32_t asid,
                 tlb_access_t access, tlb_entry_t *entry)
{
    (void)va; (void)bits; (void)asid; (void)access;
    *entry = *(tlb_entry_t *)context;
    return true;
}
int main(void)
{
    tlb_t tlb, before;
    uint64_t pa = 99;
    tlb_entry_t small = mapping(0x1000, 0xFFFFF000);
    tlb_entry_t large = mapping(0, 0xFFFF0000);
    tlb_init(&tlb);
    CHECK(fill(&tlb, small, &pa) == TLB_RESULT_HIT);
    uint64_t recency = tlb.last_used[0];
    CHECK(fill(&tlb, small, &pa) == TLB_RESULT_HIT);
    CHECK(tlb.stats.current_occupancy == 1 && tlb.stats.refills == 2);
    CHECK(tlb.stats.evictions == 0 && tlb.last_used[0] > recency);
    for (unsigned i = 1; i < TLB_ENTRY_COUNT; ++i) {
        CHECK(fill(&tlb, mapping((i + 1) * 0x1000, 0xFFFFF000), &pa)
              == TLB_RESULT_HIT);
    }
    CHECK(fill(&tlb, small, &pa) == TLB_RESULT_HIT);
    CHECK(tlb.stats.evictions == 0 && tlb.stats.current_occupancy == TLB_ENTRY_COUNT);
    pa = 99;
    CHECK(tlb_refill(&tlb, &small, small.virtual_base, 32, 7,
                    TLB_ACCESS_WRITE, &pa) == TLB_RESULT_PERMISSION_FAULT);
    CHECK(pa == 99 && tlb.stats.permission_faults == 1);
    CHECK(tlb.stats.evictions == 0 && tlb.stats.current_occupancy == TLB_ENTRY_COUNT);
    /* Each conflict preserves the entire TLB, including counters and recency. */
    for (unsigned kind = 0; kind < 4; ++kind) {
        tlb_init(&tlb);
        CHECK(fill(&tlb, kind == 1 ? large : small, &pa) == TLB_RESULT_HIT);
        tlb_entry_t incoming = kind == 0 ? large : small;
        if (kind == 2) incoming.physical_base = 0x8000;
        if (kind == 3) incoming.permissions |= TLB_PERMISSION_WRITE;
        memcpy(&before, &tlb, sizeof(tlb));
        pa = 99;
        CHECK(fill(&tlb, incoming, &pa) == TLB_RESULT_REFILL_CONFLICT);
        CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    }
    tlb_init(&tlb);
    CHECK(fill(&tlb, small, &pa) == TLB_RESULT_HIT);
    tlb_entry_t other = small;
    other.asid = 8;
    CHECK(fill(&tlb, other, &pa) == TLB_RESULT_HIT);
    CHECK(fill(&tlb, mapping(0x2000, 0xFFFFF000), &pa) == TLB_RESULT_HIT);
    /* Scan past an identical entry: a later conflicting overlap must win. */
    if (TLB_ENTRY_COUNT > 1) {
        tlb_init(&tlb);
        tlb.entries[0] = small;
        tlb.entries[1] = large;
        memcpy(&before, &tlb, sizeof(tlb));
        pa = 99;
        CHECK(fill(&tlb, small, &pa) == TLB_RESULT_REFILL_CONFLICT);
        CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    }
    tlb_init(&tlb);
    CHECK(fill(&tlb, small, &pa) == TLB_RESULT_HIT);
    pa = 99;
    CHECK(tlb_access(&tlb, 0x2000, 32, 7, TLB_ACCESS_READ, walk, &large, &pa)
          == TLB_RESULT_REFILL_CONFLICT);
    CHECK(pa == 99 && tlb.stats.unresolved_misses == 1);
    CHECK(tlb.stats.refills == 1 && tlb.stats.evictions == 0);
    size_t removed;
    CHECK(tlb_invalidate_overlap(&tlb, 0, 0xFFFF0000, 32, 7, &removed)
          == TLB_INVALIDATION_REMOVED);
    CHECK(fill(&tlb, large, &pa) == TLB_RESULT_HIT);
    return failures ? 1 : 0;
}
