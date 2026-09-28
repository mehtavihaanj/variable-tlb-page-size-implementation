#include "tlb_contract.h"

#include <string.h>

static uint64_t address_width_mask(unsigned address_bits)
{
    if (address_bits == 64)
    {
        return UINT64_MAX;
    }
    return (UINT64_C(1) << address_bits) - 1;
}

static bool address_fits_width(uint64_t address, unsigned address_bits)
{
    if (address_bits == 0 || address_bits > 64)
    {
        return false;
    }
    return (address & ~address_width_mask(address_bits)) == 0;
}

bool tlb_mask_is_valid(uint64_t address_mask, unsigned address_bits)
{
    if (address_bits == 0 || address_bits > 64 || address_mask == 0)
    {
        return false;
    }

    uint64_t width_mask = address_width_mask(address_bits);
    if ((address_mask & ~width_mask) != 0)
    {
        return false;
    }

    uint64_t offset_mask = ~address_mask & width_mask;
    return (offset_mask & (offset_mask + 1)) == 0;
}

tlb_result_t tlb_lookup(const tlb_entry_t *entries, size_t entry_count,
                        uint64_t virtual_address, unsigned address_bits,
                        uint32_t asid, tlb_access_t access,
                        uint64_t *physical_address)
{
    uint8_t required_permission;
    switch (access)
    {
    case TLB_ACCESS_READ:
        required_permission = TLB_PERMISSION_READ;
        break;
    case TLB_ACCESS_WRITE:
        required_permission = TLB_PERMISSION_WRITE;
        break;
    case TLB_ACCESS_EXECUTE:
        required_permission = TLB_PERMISSION_EXECUTE;
        break;
    default:
        return TLB_RESULT_INVALID_ARGUMENT;
    }

    if (physical_address == NULL || entry_count > TLB_ENTRY_COUNT ||
        (entries == NULL && entry_count != 0) ||
        !address_fits_width(virtual_address, address_bits))
    {
        return TLB_RESULT_INVALID_ARGUMENT;
    }

    const uint8_t supported_permissions = TLB_PERMISSION_READ |
                                          TLB_PERMISSION_WRITE |
                                          TLB_PERMISSION_EXECUTE;
    const tlb_entry_t *match = NULL;
    for (size_t index = 0; index < entry_count; ++index)
    {
        const tlb_entry_t *entry = &entries[index];
        if (!entry->valid)
        {
            continue;
        }

        if (!tlb_mask_is_valid(entry->address_mask, address_bits) ||
            !address_fits_width(entry->virtual_base, address_bits) ||
            !address_fits_width(entry->physical_base, address_bits) ||
            (entry->virtual_base & ~entry->address_mask) != 0 ||
            (entry->physical_base & ~entry->address_mask) != 0 ||
            (entry->permissions & (uint8_t)~supported_permissions) != 0)
        {
            return TLB_RESULT_INVALID_ARGUMENT;
        }

        if (entry->asid == asid &&
            (virtual_address & entry->address_mask) == entry->virtual_base)
        {
            if (match != NULL)
            {
                return TLB_RESULT_AMBIGUOUS;
            }
            match = entry;
        }
    }

    if (match == NULL)
    {
        return TLB_RESULT_MISS;
    }
    if ((match->permissions & required_permission) == 0)
    {
        return TLB_RESULT_PERMISSION_FAULT;
    }

    *physical_address = match->physical_base |
                        (virtual_address & ~match->address_mask);
    return TLB_RESULT_HIT;
}

void tlb_init(tlb_t *tlb)
{
    if (tlb != NULL)
    {
        memset(tlb, 0, sizeof(*tlb));
    }
}

tlb_result_t tlb_access(tlb_t *tlb, uint64_t virtual_address,
                        unsigned address_bits, uint32_t asid,
                        tlb_access_t access, tlb_page_walker_t page_walker,
                        void *walker_context, uint64_t *physical_address)
{
    if (tlb == NULL || physical_address == NULL)
    {
        return TLB_RESULT_INVALID_ARGUMENT;
    }

    tlb_result_t result = tlb_lookup(tlb->entries, TLB_ENTRY_COUNT,
                                     virtual_address, address_bits, asid,
                                     access, physical_address);
    if (result != TLB_RESULT_MISS)
    {
        return result;
    }
    if (page_walker == NULL)
    {
        return TLB_RESULT_INVALID_ARGUMENT;
    }

    size_t free_slot = TLB_ENTRY_COUNT;
    for (size_t index = 0; index < TLB_ENTRY_COUNT; ++index)
    {
        if (!tlb->entries[index].valid)
        {
            free_slot = index;
            break;
        }
    }
    if (free_slot == TLB_ENTRY_COUNT)
    {
        return TLB_RESULT_FULL;
    }

    tlb_entry_t translation = {0};
    if (!page_walker(walker_context, virtual_address, address_bits, asid,
                     access, &translation))
    {
        return TLB_RESULT_WALK_FAILED;
    }

    result = tlb_lookup(&translation, 1, virtual_address, address_bits,
                        asid, access, physical_address);
    if (result != TLB_RESULT_HIT &&
        result != TLB_RESULT_PERMISSION_FAULT)
    {
        return TLB_RESULT_INVALID_TRANSLATION;
    }

    tlb->entries[free_slot] = translation;
    return result;
}