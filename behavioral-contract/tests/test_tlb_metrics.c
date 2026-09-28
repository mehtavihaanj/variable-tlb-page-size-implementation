#include "tlb_contract.h"

#include <stdio.h>

static unsigned failures;

#define CHECK(expression) \
    do { if (!(expression)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
        ++failures; \
    } } while (0)

typedef struct {
    unsigned calls;
    bool succeeds;
} fake_walker_t;

static bool fake_walk(void *context, uint64_t virtual_address,
                     unsigned address_bits, uint32_t asid,
                     tlb_access_t access, tlb_entry_t *translation)
{
    fake_walker_t *walker = context;
    ++walker->calls;
    if (!walker->succeeds || address_bits != 16) return false;

    if ((virtual_address & 0xFF00) == 0x1200)
    {
        *translation = (tlb_entry_t){0x1200, 0x8000, 0xFF00, asid,
                                     TLB_PERMISSION_READ, true};
    }
    else
    {
        uint64_t virtual_base = virtual_address & UINT64_C(0xFFF0);
        *translation = (tlb_entry_t){
            virtual_base, 0xA000 | (virtual_base & 0x0FF0), 0xFFF0, asid,
            TLB_PERMISSION_READ, true};
    }
    (void)access;
    return true;
}

static void test_event_and_page_size_statistics(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    fake_walker_t walker = {0, true};
    uint64_t physical_address = 0;
    size_t invalidated = 0;

    CHECK(tlb_access(&tlb, 0x12AB, 16, 1, TLB_ACCESS_READ, fake_walk,
                     &walker, &physical_address) == TLB_RESULT_HIT);
    CHECK(tlb_access(&tlb, 0x12BC, 16, 1, TLB_ACCESS_READ, fake_walk,
                     &walker, &physical_address) == TLB_RESULT_HIT);
    CHECK(tlb_access(&tlb, 0x12BD, 16, 1, TLB_ACCESS_WRITE, fake_walk,
                     &walker, &physical_address) ==
          TLB_RESULT_PERMISSION_FAULT);
    CHECK(tlb_access(&tlb, 0x2234, 16, 1, TLB_ACCESS_READ, fake_walk,
                     &walker, &physical_address) == TLB_RESULT_HIT);
    CHECK(tlb_invalidate_overlap(&tlb, 0x1230, 0xFFF0, 16, 1,
                                 &invalidated) == TLB_INVALIDATION_REMOVED);
    walker.succeeds = false;
    CHECK(tlb_access(&tlb, 0x3300, 16, 1, TLB_ACCESS_READ, fake_walk,
                     &walker, &physical_address) == TLB_RESULT_WALK_FAILED);

        CHECK(tlb.stats.accesses == 5 && tlb.stats.hits == 2 &&
            tlb.stats.misses == 3);
        CHECK(tlb.stats.permission_faults == 1 &&
            tlb.stats.unresolved_misses == 1);
    CHECK(tlb.stats.page_walks == 3 && tlb.stats.refills == 2);
    CHECK(tlb.stats.evictions == 0 && tlb.stats.invalidations == 1);
        CHECK(tlb.stats.hits_by_page_order[8] == 2 &&
            tlb.stats.misses_by_page_order[8] == 1 &&
            tlb.stats.misses_by_page_order[4] == 1);
            CHECK(tlb.stats.current_occupancy == 1 && tlb.stats.peak_occupancy == 2);
            CHECK(tlb_storage_bytes() == sizeof(tlb) &&
                TLB_NOMINAL_ENTRY_BITS * TLB_ENTRY_COUNT == 416);
}

int main(void)
{
    test_event_and_page_size_statistics();

    if (failures != 0)
    {
        fprintf(stderr, "%u metrics check(s) failed\n", failures);
        return 1;
    }
    puts("All TLB metrics checks passed");
    return 0;
}