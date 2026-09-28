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

static bool entry_is_well_formed(const tlb_entry_t *entry,
                                 unsigned address_bits)
{
    const uint8_t supported_permissions = TLB_PERMISSION_READ |
                                          TLB_PERMISSION_WRITE |
                                          TLB_PERMISSION_EXECUTE;
    return tlb_mask_is_valid(entry->address_mask, address_bits) &&
           address_fits_width(entry->virtual_base, address_bits) &&
           address_fits_width(entry->physical_base, address_bits) &&
           (entry->virtual_base & ~entry->address_mask) == 0 &&
           (entry->physical_base & ~entry->address_mask) == 0 &&
           (entry->permissions & (uint8_t)~supported_permissions) == 0;
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

    const tlb_entry_t *match = NULL;
    for (size_t index = 0; index < entry_count; ++index)
    {
        const tlb_entry_t *entry = &entries[index];
        if (!entry->valid)
        {
            continue;
        }

        if (!entry_is_well_formed(entry, address_bits))
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

static size_t find_matching_slot(const tlb_t *tlb, uint64_t virtual_address,
                                 uint32_t asid)
{
    for (size_t index = 0; index < TLB_ENTRY_COUNT; ++index)
    {
        const tlb_entry_t *entry = &tlb->entries[index];
        if (entry->valid && entry->asid == asid &&
            (virtual_address & entry->address_mask) == entry->virtual_base)
        {
            return index;
        }
    }
    return TLB_ENTRY_COUNT;
}

static void mark_recently_used(tlb_t *tlb, size_t slot)
{
    ++tlb->use_sequence;
    tlb->last_used[slot] = tlb->use_sequence;
}

static unsigned page_order(uint64_t address_mask, unsigned address_bits)
{
    uint64_t offset_mask = ~address_mask & address_width_mask(address_bits);
    unsigned order = 0;
    while (offset_mask != 0)
    {
        ++order;
        offset_mask >>= 1;
    }
    return order;
}

static void update_occupancy(tlb_t *tlb)
{
    size_t current = 0;
    for (size_t index = 0; index < TLB_ENTRY_COUNT; ++index)
    {
        current += tlb->entries[index].valid ? 1 : 0;
    }
    tlb->stats.current_occupancy = current;
    if (current > tlb->stats.peak_occupancy)
    {
        tlb->stats.peak_occupancy = current;
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
    if (result == TLB_RESULT_INVALID_ARGUMENT)
    {
        return result;
    }

    ++tlb->stats.accesses;
    update_occupancy(tlb);
    if (result == TLB_RESULT_AMBIGUOUS)
    {
        ++tlb->stats.ambiguous_lookups;
        return result;
    }
    if (result == TLB_RESULT_HIT || result == TLB_RESULT_PERMISSION_FAULT)
    {
        size_t slot = find_matching_slot(tlb, virtual_address, asid);
        if (slot < TLB_ENTRY_COUNT)
        {
            ++tlb->stats.hits;
            ++tlb->stats.hits_by_page_order[
                page_order(tlb->entries[slot].address_mask, address_bits)];
            mark_recently_used(tlb, slot);
        }
        if (result == TLB_RESULT_PERMISSION_FAULT)
        {
            ++tlb->stats.permission_faults;
        }
        return result;
    }
    ++tlb->stats.misses;
    if (page_walker == NULL)
    {
        ++tlb->stats.unresolved_misses;
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
    tlb_entry_t translation = {0};
    ++tlb->stats.page_walks;
    if (!page_walker(walker_context, virtual_address, address_bits, asid,
                     access, &translation))
    {
        ++tlb->stats.unresolved_misses;
        return TLB_RESULT_WALK_FAILED;
    }

    result = tlb_lookup(&translation, 1, virtual_address, address_bits,
                        asid, access, physical_address);
    if (result != TLB_RESULT_HIT &&
        result != TLB_RESULT_PERMISSION_FAULT)
    {
        ++tlb->stats.unresolved_misses;
        return TLB_RESULT_INVALID_TRANSLATION;
    }

    unsigned resolved_order = page_order(translation.address_mask,
                                         address_bits);
    ++tlb->stats.refills;
    ++tlb->stats.misses_by_page_order[resolved_order];
    if (result == TLB_RESULT_PERMISSION_FAULT)
    {
        ++tlb->stats.permission_faults;
    }
    if (free_slot == TLB_ENTRY_COUNT)
    {
        ++tlb->stats.evictions;
        free_slot = 0;
        for (size_t index = 1; index < TLB_ENTRY_COUNT; ++index)
        {
            if (tlb->last_used[index] < tlb->last_used[free_slot])
            {
                free_slot = index;
            }
        }
    }

    tlb->entries[free_slot] = translation;
    mark_recently_used(tlb, free_slot);
    update_occupancy(tlb);
    return result;
}

tlb_invalidation_result_t tlb_invalidate_overlap(
    tlb_t *tlb, uint64_t virtual_base, uint64_t address_mask,
    unsigned address_bits, uint32_t asid, size_t *invalidated_count)
{
    if (tlb == NULL || invalidated_count == NULL ||
        !tlb_mask_is_valid(address_mask, address_bits) ||
        !address_fits_width(virtual_base, address_bits) ||
        (virtual_base & ~address_mask) != 0)
    {
        return TLB_INVALIDATION_INVALID_ARGUMENT;
    }

    for (size_t index = 0; index < TLB_ENTRY_COUNT; ++index)
    {
        const tlb_entry_t *entry = &tlb->entries[index];
        if (entry->valid && !entry_is_well_formed(entry, address_bits))
        {
            return TLB_INVALIDATION_INVALID_ARGUMENT;
        }
    }

    uint64_t width_mask = address_width_mask(address_bits);
    uint64_t request_end = virtual_base | (~address_mask & width_mask);
    *invalidated_count = 0;
    for (size_t index = 0; index < TLB_ENTRY_COUNT; ++index)
    {
        tlb_entry_t *entry = &tlb->entries[index];
        if (!entry->valid || entry->asid != asid)
        {
            continue;
        }

        uint64_t entry_end = entry->virtual_base |
                             (~entry->address_mask & width_mask);
        if (entry->virtual_base <= request_end && virtual_base <= entry_end)
        {
            entry->valid = false;
            ++*invalidated_count;
        }
    }

    tlb->stats.invalidations += *invalidated_count;
    update_occupancy(tlb);
    return *invalidated_count == 0 ? TLB_INVALIDATION_NO_MATCH :
                                    TLB_INVALIDATION_REMOVED;
}

size_t tlb_storage_bytes(void)
{
    return sizeof(tlb_t);
}