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

typedef struct {
    tlb_entry_t entry;
    unsigned calls;
    bool succeeds;
} fake_walker_t;

static bool fake_walk(void *context, uint64_t virtual_address,
                     unsigned address_bits, uint32_t asid,
                     tlb_access_t access, tlb_entry_t *translation)
{
    fake_walker_t *walker = context;
    (void)virtual_address, (void)address_bits, (void)asid, (void)access;
    ++walker->calls;
    if (!walker->succeeds)
    {
        return false;
    }
    *translation = walker->entry;
    return true;
}

static tlb_entry_t make_entry(uint64_t virtual_base, uint64_t physical_base,
                              uint64_t address_mask, uint32_t asid,
                              uint8_t permissions)
{
    return (tlb_entry_t){virtual_base, physical_base, address_mask, asid,
                         permissions, true};
}

static void test_refill_then_hit(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    fake_walker_t walker = {make_entry(0x1200, 0x8000, 0xFF00, 2,
                                      TLB_PERMISSION_READ), 0, true};
    uint64_t physical_address = 0;

    CHECK(tlb_access(&tlb, 0x12FF, 16, 2, TLB_ACCESS_READ, fake_walk, &walker,
                     &physical_address) == TLB_RESULT_HIT);
    CHECK(physical_address == 0x80FF);
    CHECK(walker.calls == 1);
    CHECK(tlb_access(&tlb, 0x1234, 16, 2, TLB_ACCESS_READ, fake_walk, &walker,
                     &physical_address) == TLB_RESULT_HIT);
    CHECK(physical_address == 0x8034);
    CHECK(walker.calls == 1);
}

static void test_walk_failure_and_invalid_translation(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    fake_walker_t walker = {make_entry(0x1200, 0x8000, 0xFF00, 2,
                                      TLB_PERMISSION_READ), 0, false};
    uint64_t physical_address = 0;

    CHECK(tlb_access(&tlb, 0x1234, 16, 2, TLB_ACCESS_READ, fake_walk, &walker,
                     &physical_address) == TLB_RESULT_WALK_FAILED);
    walker.succeeds = true;
    walker.entry.address_mask = 0xFFFA;
    CHECK(tlb_access(&tlb, 0x1234, 16, 2, TLB_ACCESS_READ, fake_walk, &walker,
                     &physical_address) ==
          TLB_RESULT_INVALID_TRANSLATION);
    CHECK(walker.calls == 2);
}

static void test_permission_fault_is_cached(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    fake_walker_t walker = {make_entry(0x1230, 0xA000, 0xFFF0, 3,
                                      TLB_PERMISSION_READ), 0, true};
    uint64_t physical_address = 0;

    CHECK(tlb_access(&tlb, 0x1234, 16, 3, TLB_ACCESS_WRITE, fake_walk, &walker,
                     &physical_address) ==
          TLB_RESULT_PERMISSION_FAULT);
    CHECK(tlb_access(&tlb, 0x1234, 16, 3, TLB_ACCESS_READ, fake_walk, &walker,
                     &physical_address) == TLB_RESULT_HIT);
    CHECK(physical_address == 0xA004);
    CHECK(walker.calls == 1);
}

int main(void)
{
    test_refill_then_hit();
    test_walk_failure_and_invalid_translation();
    test_permission_fault_is_cached();

    if (failures != 0)
    {
        fprintf(stderr, "%u TLB state check(s) failed\n", failures);
        return 1;
    }
    puts("All TLB state checks passed");
    return 0;
}