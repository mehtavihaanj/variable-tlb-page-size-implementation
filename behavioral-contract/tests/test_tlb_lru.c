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
    unsigned calls;
    bool succeeds;
} fake_walker_t;

static bool fake_walk(void *context, uint64_t virtual_address,
                     unsigned address_bits, uint32_t asid,
                     tlb_access_t access, tlb_entry_t *translation)
{
    fake_walker_t *walker = context;
    ++walker->calls;
    if (!walker->succeeds || address_bits != 16)
    {
        return false;
    }

    uint64_t virtual_base = virtual_address & UINT64_C(0xFFF0);
    uint8_t permissions = virtual_base == 0x1010 ? TLB_PERMISSION_READ :
        TLB_PERMISSION_READ | TLB_PERMISSION_WRITE;
    *translation = (tlb_entry_t){virtual_base,
                                 0x8000 + (virtual_base & 0x0FFF),
                                 0xFFF0, asid, permissions, true};
    (void)access;
    return true;
}

static tlb_result_t access_page(tlb_t *tlb, uint64_t virtual_address,
                                tlb_access_t access, fake_walker_t *walker,
                                uint64_t *physical_address)
{
    return tlb_access(tlb, virtual_address, 16, 1, access, fake_walk, walker,
                      physical_address);
}

static void test_lru_hit_and_permission_fault_refresh_recency(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    fake_walker_t walker = {0, true};
    uint64_t physical_address = 0;

    for (size_t index = 0; index < TLB_ENTRY_COUNT; ++index)
    {
        CHECK(access_page(&tlb, 0x1000 + index * 0x10, TLB_ACCESS_READ,
                          &walker, &physical_address) == TLB_RESULT_HIT);
    }
    CHECK(walker.calls == TLB_ENTRY_COUNT);
    CHECK(access_page(&tlb, 0x1000, TLB_ACCESS_READ, &walker,
                      &physical_address) == TLB_RESULT_HIT);
    CHECK(access_page(&tlb, 0x1010, TLB_ACCESS_WRITE, &walker,
                      &physical_address) == TLB_RESULT_PERMISSION_FAULT);

    CHECK(access_page(&tlb, 0x1080, TLB_ACCESS_READ, &walker,
                      &physical_address) == TLB_RESULT_HIT);
    CHECK(walker.calls == TLB_ENTRY_COUNT + 1);
    CHECK(access_page(&tlb, 0x1000, TLB_ACCESS_READ, &walker,
                      &physical_address) == TLB_RESULT_HIT);
    CHECK(access_page(&tlb, 0x1010, TLB_ACCESS_READ, &walker,
                      &physical_address) == TLB_RESULT_HIT);
    CHECK(walker.calls == TLB_ENTRY_COUNT + 1);
    CHECK(access_page(&tlb, 0x1020, TLB_ACCESS_READ, &walker,
                      &physical_address) == TLB_RESULT_HIT);
    CHECK(walker.calls == TLB_ENTRY_COUNT + 2);
}

static void test_failed_walk_does_not_evict(void)
{
    tlb_t tlb;
    tlb_init(&tlb);
    fake_walker_t walker = {0, true};
    uint64_t physical_address = 0;

    for (size_t index = 0; index < TLB_ENTRY_COUNT; ++index)
    {
        CHECK(access_page(&tlb, 0x1000 + index * 0x10, TLB_ACCESS_READ,
                          &walker, &physical_address) == TLB_RESULT_HIT);
    }
    walker.succeeds = false;
    CHECK(access_page(&tlb, 0x1080, TLB_ACCESS_READ, &walker,
                      &physical_address) == TLB_RESULT_WALK_FAILED);
    walker.succeeds = true;
    CHECK(access_page(&tlb, 0x1000, TLB_ACCESS_READ, &walker,
                      &physical_address) == TLB_RESULT_HIT);
    CHECK(walker.calls == TLB_ENTRY_COUNT + 1);
}

int main(void)
{
    test_lru_hit_and_permission_fault_refresh_recency();
    test_failed_walk_does_not_evict();

    if (failures != 0)
    {
        fprintf(stderr, "%u LRU check(s) failed\n", failures);
        return 1;
    }
    puts("All TLB LRU checks passed");
    return 0;
}