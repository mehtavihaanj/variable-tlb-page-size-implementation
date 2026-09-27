#include "tlb_contract.h"

bool tlb_mask_is_valid(uint64_t address_mask, unsigned address_bits)
{
    (void)address_mask;
    (void)address_bits;
    return false;
}

tlb_result_t tlb_lookup(const tlb_entry_t *entries, size_t entry_count,
                        uint64_t virtual_address, unsigned address_bits,
                        uint32_t asid, tlb_access_t access,
                        uint64_t *physical_address)
{
    (void)entries;
    (void)entry_count;
    (void)virtual_address;
    (void)address_bits;
    (void)asid;
    (void)access;
    (void)physical_address;
    return TLB_RESULT_INVALID_ARGUMENT;
}