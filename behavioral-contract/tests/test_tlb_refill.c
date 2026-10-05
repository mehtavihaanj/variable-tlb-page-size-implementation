#include "tlb_contract.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)

static tlb_entry_t mapping(unsigned page)
{
    return (tlb_entry_t){(uint64_t)page << 12, (uint64_t)page << 12,
        UINT64_C(0xFFFFF000), 7, TLB_PERMISSION_READ, true};
}

static tlb_result_t refill(tlb_t *tlb, const tlb_entry_t *entry, uint64_t *pa)
{
    return tlb_refill(tlb, entry, entry->virtual_base + 42, 32, 7,
                      TLB_ACCESS_READ, pa);
}

int main(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    uint64_t pa = 99;
    tlb_entry_t entry = mapping(1);
    CHECK(refill(&tlb, &entry, &pa) == TLB_RESULT_HIT);
    CHECK(pa == 0x102A);
    CHECK(tlb.stats.refills == 1 && tlb.stats.current_occupancy == 1);
    CHECK(tlb.stats.misses_by_page_order[12] == 1);
    CHECK(tlb.stats.accesses == 0 && tlb.stats.misses == 0);
    CHECK(tlb.stats.page_walks == 0 && tlb.stats.hits == 0);
    for (unsigned page = 2; page <= TLB_ENTRY_COUNT; ++page) {
        entry = mapping(page);
        CHECK(refill(&tlb, &entry, &pa) == TLB_RESULT_HIT);
    }
    /* Invalid completed walks must not evict or update any state/output. */
    tlb_t before;
    memcpy(&before, &tlb, sizeof(tlb));
    for (unsigned bad = 0; bad < 5; ++bad) {
        entry = mapping(TLB_ENTRY_COUNT + 1);
        if (bad == 0) entry.address_mask = 0xFFFFFFFA;
        if (bad == 1) entry.physical_base++;
        if (bad == 2) entry.asid++;
        if (bad == 3) entry.valid = false;
        pa = 99;
        CHECK(tlb_refill(&tlb, &entry, bad == 4 ? 0 : entry.virtual_base,
              32, 7, TLB_ACCESS_READ, &pa) == TLB_RESULT_INVALID_TRANSLATION);
        CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    }
    CHECK(tlb_refill(&tlb, NULL, 0, 32, 7, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_INVALID_ARGUMENT);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0);
    /* A hit after the miss began changes the victim chosen at completion. */
    CHECK(tlb_access(&tlb, 0x1000, 32, 7, TLB_ACCESS_READ, NULL, NULL, &pa)
          == TLB_RESULT_HIT);
    entry = mapping(TLB_ENTRY_COUNT + 1);
    CHECK(refill(&tlb, &entry, &pa) == TLB_RESULT_HIT);
    CHECK(tlb.stats.evictions == 1);
    CHECK(tlb.stats.current_occupancy == TLB_ENTRY_COUNT);
    CHECK(tlb_lookup(tlb.entries, TLB_ENTRY_COUNT,
          TLB_ENTRY_COUNT > 1 ? 0x2000 : 0x1000, 32, 7,
          TLB_ACCESS_READ, &pa) == TLB_RESULT_MISS);
    /* A permission fault still caches the mapping, preserving output. */
    tlb_init(&tlb);
    entry = mapping(1);
    pa = 99;
    CHECK(tlb_refill(&tlb, &entry, 0x102A, 32, 7, TLB_ACCESS_WRITE, &pa)
          == TLB_RESULT_PERMISSION_FAULT);
    CHECK(pa == 99 && tlb.stats.permission_faults == 1);
    CHECK(tlb.stats.refills == 1 && tlb.stats.evictions == 0);
    CHECK(tlb_access(&tlb, 0x102A, 32, 7, TLB_ACCESS_READ, NULL, NULL, &pa)
          == TLB_RESULT_HIT);
    CHECK(pa == 0x102A && tlb.stats.page_walks == 0);
    return failures ? 1 : 0;
}
