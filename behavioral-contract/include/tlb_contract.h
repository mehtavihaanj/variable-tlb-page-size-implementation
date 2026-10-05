#ifndef TLB_CONTRACT_H
#define TLB_CONTRACT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TLB_ENTRY_COUNT 8
#define TLB_PAGE_ORDER_COUNT 65
#define TLB_NOMINAL_ENTRY_BITS 52
#define TLB_PAGE_ORDERS_ABSTRACT UINT64_MAX
#define TLB_PAGE_ORDERS_X86_64 ((UINT64_C(1) << 12) | \
                                (UINT64_C(1) << 21) | (UINT64_C(1) << 30))
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
    TLB_RESULT_INVALID_TRANSLATION,
    TLB_RESULT_INVALID_ARGUMENT,
    TLB_RESULT_REFILL_CONFLICT,
    TLB_RESULT_STALE_REFILL,
    TLB_RESULT_UNSUPPORTED_PAGE_SIZE
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
    uint64_t accesses;
    uint64_t hits;
    uint64_t misses;
    uint64_t permission_faults;
    uint64_t ambiguous_lookups;
    uint64_t page_walks;
    uint64_t refills;
    uint64_t unresolved_misses;
    uint64_t evictions;
    uint64_t invalidations;
    uint64_t hits_by_page_order[TLB_PAGE_ORDER_COUNT];
    uint64_t misses_by_page_order[TLB_PAGE_ORDER_COUNT];
    size_t current_occupancy;
    size_t peak_occupancy;
} tlb_stats_t;

typedef struct {
    tlb_entry_t entries[TLB_ENTRY_COUNT];
    uint64_t last_used[TLB_ENTRY_COUNT];
    uint64_t use_sequence;
    uint64_t invalidation_generation;
    uint64_t allowed_page_orders;
    tlb_stats_t stats;
} tlb_t;

typedef enum {
    TLB_INVALIDATION_REMOVED,
    TLB_INVALIDATION_NO_MATCH,
    TLB_INVALIDATION_INVALID_ARGUMENT
} tlb_invalidation_result_t;

bool tlb_mask_is_valid(uint64_t address_mask, unsigned address_bits);

tlb_result_t tlb_lookup(const tlb_entry_t *entries, size_t entry_count,
                        uint64_t virtual_address, unsigned address_bits,
                        uint32_t asid, tlb_access_t access,
                        uint64_t *physical_address);

void tlb_init(tlb_t *tlb);
/* Bit n permits 2^n-byte pages (orders 0..63); zero is invalid.
 * Initialization clears state: cancel outstanding walks first. Configure once
 * per lifetime; do not edit allowed_page_orders on a live TLB. tlb_init uses
 * the abstract preset. X86_64 constrains sizes only, not other ISA semantics.
 * Raw tlb_lookup and invalidation ranges retain abstract mask semantics.
 */
bool tlb_init_config(tlb_t *tlb, uint64_t allowed_page_orders);
size_t tlb_storage_bytes(void);

/* Account one lookup and refresh recency on translation matches (including
 * permission faults). A miss returns immediately without starting a walk.
 * Invalid requests leave state unchanged; use tlb_lookup for pure inspection.
 */
tlb_result_t tlb_probe(tlb_t *tlb, uint64_t virtual_address,
                      unsigned address_bits, uint32_t asid,
                      tlb_access_t access, uint64_t *physical_address);

/* Complete a successful walk. Identical same-ASID mappings refresh in place;
 * conflicting overlaps return REFILL_CONFLICT without mutation or output.
 * Invalidate explicitly before changing a mapping. Scan all entries before
 * accepting duplicates, so preexisting ambiguity is never silently accepted.
 * Updates refill/eviction, resolved-page, permission-fault and occupancy stats.
 * Accepted duplicates count as refills and resolved misses, never evictions.
 * Probe owns access/miss counts; the driver owns walk/unresolved counts.
 * Invalid input leaves state and output unchanged; permission faults cache
 * the mapping without output.
 */
tlb_result_t tlb_refill(tlb_t *tlb, const tlb_entry_t *translation,
                       uint64_t virtual_address, unsigned address_bits,
                       uint32_t asid, tlb_access_t access,
                       uint64_t *physical_address);

/* Capture invalidation_generation before starting a walk and pass it here.
 * Any valid invalidation stales outstanding walks, including other ASIDs and
 * ranges with no cached entries. Rejected completions leave state/output alone.
 * UINT64_MAX is a fail-closed exhausted generation. Reinitialize only after
 * cancelling all pending walks; tokens belong to this TLB instance/lifetime.
 * tlb_refill is for immediate insertions; delayed drivers must use this API
 * and account rejected completions as unresolved before retrying a fresh walk.
 */
tlb_result_t tlb_complete_walk(tlb_t *tlb, uint64_t generation,
                              const tlb_entry_t *translation,
                              uint64_t virtual_address, unsigned address_bits,
                              uint32_t asid, tlb_access_t access,
                              uint64_t *physical_address);

tlb_result_t tlb_access(tlb_t *tlb, uint64_t virtual_address,
                        unsigned address_bits, uint32_t asid,
                        tlb_access_t access, tlb_page_walker_t page_walker,
                        void *walker_context, uint64_t *physical_address);

tlb_invalidation_result_t tlb_invalidate_overlap(
    tlb_t *tlb, uint64_t virtual_base, uint64_t address_mask,
    unsigned address_bits, uint32_t asid, size_t *invalidated_count);

#endif
