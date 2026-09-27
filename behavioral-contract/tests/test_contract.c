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

static void test_masks(void)
{
    CHECK(tlb_mask_is_valid(UINT64_C(0xFFFF), 16));
    CHECK(tlb_mask_is_valid(UINT64_C(0xFFF0), 16));
    CHECK(tlb_mask_is_valid(UINT64_C(0xFF00), 16));
    CHECK(!tlb_mask_is_valid(UINT64_C(0xFFFA), 16));
    CHECK(!tlb_mask_is_valid(UINT64_C(0), 16));
    CHECK(!tlb_mask_is_valid(UINT64_C(0x10000), 16));
    CHECK(!tlb_mask_is_valid(UINT64_MAX, 0));
    CHECK(!tlb_mask_is_valid(UINT64_MAX, 65));
    CHECK(tlb_mask_is_valid(UINT64_MAX, 64));
}

static void test_translation_and_boundaries(void)
{
    const tlb_entry_t entries[] = {
        { .virtual_base = 0x1200, .physical_base = 0x8000,
          .address_mask = 0xFF00, .asid = 1,
          .permissions = TLB_PERMISSION_READ, .valid = true },
        { .virtual_base = 0x2230, .physical_base = 0xA000,
          .address_mask = 0xFFF0, .asid = 1,
          .permissions = TLB_PERMISSION_READ, .valid = true }
    };
    uint64_t physical_address = 0;

    CHECK(tlb_lookup(entries, 2, 0x12FF, 16, 1, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_HIT);
    CHECK(physical_address == 0x80FF);
    CHECK(tlb_lookup(entries, 2, 0x1300, 16, 1, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_MISS);
    CHECK(tlb_lookup(entries, 2, 0x2234, 16, 1, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_HIT);
    CHECK(physical_address == 0xA004);
    CHECK(tlb_lookup(entries, 2, 0x2230, 16, 2, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_MISS);
}

static void test_validity_and_permissions(void)
{
    tlb_entry_t entry = {
        .virtual_base = 0x1200, .physical_base = 0xA000,
        .address_mask = 0xFFF0, .asid = 7,
        .permissions = TLB_PERMISSION_READ | TLB_PERMISSION_EXECUTE,
        .valid = true
    };
    uint64_t physical_address = 0;

    entry.valid = false;
    CHECK(tlb_lookup(&entry, 1, 0x1234, 16, 7, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_MISS);
    entry.valid = true;
    CHECK(tlb_lookup(&entry, 1, 0x1234, 16, 7, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_HIT);
    CHECK(tlb_lookup(&entry, 1, 0x1234, 16, 7, TLB_ACCESS_WRITE,
                     &physical_address) == TLB_RESULT_PERMISSION_FAULT);
    CHECK(tlb_lookup(&entry, 1, 0x1234, 16, 7, TLB_ACCESS_EXECUTE,
                     &physical_address) == TLB_RESULT_HIT);
}

static void test_ambiguous_match_and_arguments(void)
{
    const tlb_entry_t entries[] = {
        { .virtual_base = 0x1200, .physical_base = 0x8000,
          .address_mask = 0xFF00, .asid = 4,
          .permissions = TLB_PERMISSION_READ, .valid = true },
        { .virtual_base = 0x1230, .physical_base = 0xA000,
          .address_mask = 0xFFF0, .asid = 4,
          .permissions = TLB_PERMISSION_READ, .valid = true }
    };
    uint64_t physical_address = 0;

    CHECK(tlb_lookup(entries, 2, 0x1234, 16, 4, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_AMBIGUOUS);
    CHECK(tlb_lookup(NULL, 1, 0, 16, 0, TLB_ACCESS_READ,
                     &physical_address) == TLB_RESULT_INVALID_ARGUMENT);
    CHECK(tlb_lookup(entries, TLB_ENTRY_COUNT + 1, 0, 16, 0,
                     TLB_ACCESS_READ, &physical_address) ==
          TLB_RESULT_INVALID_ARGUMENT);
    CHECK(tlb_lookup(entries, 2, 0, 16, 0, TLB_ACCESS_READ, NULL) ==
          TLB_RESULT_INVALID_ARGUMENT);
}

int main(void)
{
    test_masks();
    test_translation_and_boundaries();
    test_validity_and_permissions();
    test_ambiguous_match_and_arguments();

    if (failures != 0) {
        fprintf(stderr, "%u contract check(s) failed\n", failures);
        return 1;
    }
    puts("All behavioral-contract checks passed");
    return 0;
}