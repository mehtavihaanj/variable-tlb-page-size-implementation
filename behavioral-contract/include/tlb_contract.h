#ifndef TLB_CONTRACT_H
#define TLB_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TLB_ENTRY_COUNT 8
#define TLB_PERMISSION_READ UINT8_C(0x01)
#define TLB_PERMISSION_WRITE UINT8_C(0x02)
#define TLB_PERMISSION_EXECUTE UINT8_C(0x04)

typedef enum {
    TLB_ACCESS_READ,
    TLB_ACCESS_WRITE,
    TLB_ACCESS_EXECUTE
} tlb_access_t;

typedef enum {
    TLB_RESULT_MISS,
    TLB_RESULT_HIT,
    TLB_RESULT_PERMISSION_FAULT,
    TLB_RESULT_AMBIGUOUS,
    TLB_RESULT_WALK_FAILED,
    TLB_RESULT_FULL,
    TLB_RESULT_INVALID_TRANSLATION,
    TLB_RESULT_INVALID_ARGUMENT
} tlb_result_t;

typedef struct {
    uint64_t virtual_base;
    uint64_t physical_base;
    uint64_t address_mask;
    uint32_t asid;
    uint8_t permissions;
    bool valid;
} tlb_entry_t;

typedef bool (*tlb_page_walker_t)(void *context, uint64_t virtual_address,
                                  unsigned address_bits, uint32_t asid,
                                  tlb_access_t access, tlb_entry_t *translation);

typedef struct {
    tlb_entry_t entries[TLB_ENTRY_COUNT];
} tlb_t;

bool tlb_mask_is_valid(uint64_t address_mask, unsigned address_bits);

tlb_result_t tlb_lookup(const tlb_entry_t *entries, size_t entry_count,
                        uint64_t virtual_address, unsigned address_bits,
                        uint32_t asid, tlb_access_t access,
                        uint64_t *physical_address);

void tlb_init(tlb_t *tlb);

tlb_result_t tlb_access(tlb_t *tlb, uint64_t virtual_address,
                        unsigned address_bits, uint32_t asid,
                        tlb_access_t access, tlb_page_walker_t page_walker,
                        void *walker_context, uint64_t *physical_address);

#endif