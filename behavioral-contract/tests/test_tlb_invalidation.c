#include "tlb_contract.h"

#include <stdio.h>

static unsigned failures;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expression); \
            ++failures; \
        } \
    } while (0)

static tlb_entry_t make_entry(uint64_t virtual_base, uint64_t address_mask,
                              uint32_t asid)
{
    return (tlb_entry_t){virtual_base, virtual_base, address_mask, asid,
                         TLB_PERMISSION_READ, true};
}

static void test_split_invalidates_overlapping_mapping_only(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    tlb.entries[0] = make_entry(0x1200, 0xFF00, 4);
    tlb.entries[1] = make_entry(0x1300, 0xFF00, 4);
    size_t invalidated = 0;

    CHECK(tlb_invalidate_overlap(&tlb, 0x1230, 0xFFF0, 16, 4,
                                 &invalidated) == TLB_INVALIDATION_REMOVED);
    CHECK(invalidated == 1);
    CHECK(!tlb.entries[0].valid);
    CHECK(tlb.entries[1].valid);
}

static void test_asid_and_nonoverlap(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    tlb.entries[0] = make_entry(0x1200, 0xFF00, 1);
    tlb.entries[1] = make_entry(0x1200, 0xFF00, 2);
    size_t invalidated = 0;

    CHECK(tlb_invalidate_overlap(&tlb, 0x1230, 0xFFF0, 16, 1,
                                 &invalidated) == TLB_INVALIDATION_REMOVED);
    CHECK(invalidated == 1);
    CHECK(!tlb.entries[0].valid && tlb.entries[1].valid);
    CHECK(tlb_invalidate_overlap(&tlb, 0x1300, 0xFF00, 16, 2,
                                 &invalidated) == TLB_INVALIDATION_NO_MATCH);
    CHECK(invalidated == 0 && tlb.entries[1].valid);
}

static void test_invalid_requests_do_not_mutate(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    tlb.entries[0] = make_entry(0x1200, 0xFF00, 1);
    size_t invalidated = 99;

    CHECK(tlb_invalidate_overlap(&tlb, 0x1231, 0xFFF0, 16, 1,
                                 &invalidated) ==
          TLB_INVALIDATION_INVALID_ARGUMENT);
    CHECK(invalidated == 99 && tlb.entries[0].valid);
    CHECK(tlb_invalidate_overlap(&tlb, 0x1230, 0xFFFA, 16, 1,
                                 &invalidated) ==
          TLB_INVALIDATION_INVALID_ARGUMENT);
    CHECK(tlb_invalidate_overlap(&tlb, 0x1230, 0xFFF0, 16, 1, NULL) ==
          TLB_INVALIDATION_INVALID_ARGUMENT);
}

int main(void)
{
    test_split_invalidates_overlapping_mapping_only();
    test_asid_and_nonoverlap();
    test_invalid_requests_do_not_mutate();

    if (failures != 0)
    {
        fprintf(stderr, "%u invalidation check(s) failed\n", failures);
        return 1;
    }
    puts("All TLB invalidation checks passed");
    return 0;
}