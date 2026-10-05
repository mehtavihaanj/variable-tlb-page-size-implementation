#include "tlb_contract.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static tlb_entry_t mapping(unsigned order)
{
    return (tlb_entry_t){0, 0, UINT64_MAX << order,
                         7, TLB_PERMISSION_READ, true};
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
    /* Every representable page order remains available in abstract mode. */
    for (unsigned order = 0; order < 64; ++order) {
        tlb_entry_t entry = mapping(order);
        uint64_t last_byte = ~entry.address_mask;
        tlb_init(&tlb);
        CHECK(tlb_refill(&tlb, &entry, last_byte, 64, 7,
                        TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
        CHECK(pa == last_byte && tlb.stats.misses_by_page_order[order] == 1);
        CHECK(tlb_init_config(&tlb, TLB_PAGE_ORDERS_X86_64));
        memcpy(&before, &tlb, sizeof(tlb));
        pa = 99;
        tlb_result_t result = tlb_complete_walk(&tlb, tlb.invalidation_generation,
            &entry, last_byte, 64, 7, TLB_ACCESS_READ, &pa);
        if (order == 12 || order == 21 || order == 30) {
            CHECK(result == TLB_RESULT_HIT && pa == last_byte);
            CHECK(tlb_probe(&tlb, last_byte, 64, 7, TLB_ACCESS_READ, &pa)
                  == TLB_RESULT_HIT);
        } else {
            CHECK(result == TLB_RESULT_UNSUPPORTED_PAGE_SIZE);
            CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
        }
    }
    CHECK(tlb_init_config(&tlb, UINT64_C(1) << 12));
    tlb_entry_t small = mapping(12), large = mapping(21);
    CHECK(tlb_refill(&tlb, &small, 42, 64, 7, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_HIT);
    memcpy(&before, &tlb, sizeof(tlb));
    pa = 99;
    CHECK(tlb_refill(&tlb, &large, 42, 64, 7, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_UNSUPPORTED_PAGE_SIZE);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    CHECK(!tlb_init_config(&tlb, 0));
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0);
    CHECK(!tlb_init_config(NULL, TLB_PAGE_ORDERS_X86_64));
    /* A 64-KiB result must fail through the synchronous driver too. */
    CHECK(tlb_init_config(&tlb, TLB_PAGE_ORDERS_X86_64));
    tlb_entry_t medium = mapping(16);
    CHECK(tlb_access(&tlb, 42, 64, 7, TLB_ACCESS_READ, walk, &medium, &pa)
          == TLB_RESULT_UNSUPPORTED_PAGE_SIZE);
    CHECK(pa == 99 && tlb.stats.refills == 0 && tlb.stats.current_occupancy == 0);
    CHECK(tlb.stats.misses == 1 && tlb.stats.unresolved_misses == 1);
    CHECK(tlb.stats.page_walks == 1);
    /* Even manually populated unsupported entries cannot yield stateful hits. */
    tlb.entries[0] = medium;
    memcpy(&before, &tlb, sizeof(tlb));
    CHECK(tlb_probe(&tlb, 42, 64, 7, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_UNSUPPORTED_PAGE_SIZE);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    CHECK(tlb_lookup(tlb.entries, TLB_ENTRY_COUNT, 42, 64, 7,
                     TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    /* Invalidation may cover ranges larger than configured leaf sizes. */
    CHECK(tlb_init_config(&tlb, UINT64_C(1) << 12));
    CHECK(tlb_refill(&tlb, &small, 42, 64, 7, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_HIT);
    size_t removed;
    CHECK(tlb_invalidate_overlap(&tlb, 0, large.address_mask, 64, 7, &removed)
          == TLB_INVALIDATION_REMOVED);
    CHECK(removed == 1);
    /* Size policy does not replace width and mask validation. */
    small.address_mask = 0xFFFFF000;
    CHECK(tlb_refill(&tlb, &small, 42, 32, 7, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_HIT);
    memcpy(&before, &tlb, sizeof(tlb));
    pa = 99;
    CHECK(tlb_refill(&tlb, &small, 42, 24, 7, TLB_ACCESS_READ, &pa)
          == TLB_RESULT_INVALID_TRANSLATION);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    return failures ? 1 : 0;
}
