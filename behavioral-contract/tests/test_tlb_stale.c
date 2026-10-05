#include "tlb_contract.h"
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); ++failures; } } while (0)
static const tlb_entry_t mapping = {0x1000, 0x8000, 0xFFFFF000,
                                    7, TLB_PERMISSION_READ, true};
static tlb_result_t complete(tlb_t *tlb, uint64_t token, uint64_t *pa)
{
    return tlb_complete_walk(tlb, token, &mapping, 0x102A, 32, 7,
                             TLB_ACCESS_READ, pa);
}
static bool invalidating_walk(void *context, uint64_t va, unsigned bits,
                              uint32_t asid, tlb_access_t access,
                              tlb_entry_t *entry)
{
    (void)va; (void)access;
    size_t removed;
    CHECK(tlb_invalidate_overlap(context, 0x1000, 0xFFFFF000, bits, asid,
                                 &removed) == TLB_INVALIDATION_NO_MATCH);
    *entry = mapping;
    return true;
}
int main(void)
{
    tlb_t tlb, before;
    uint64_t pa = 99;
    size_t removed;
    tlb_init(&tlb);
    uint64_t token = tlb.invalidation_generation;
    CHECK(tlb_probe(&tlb, 0x102A, 32, 7, TLB_ACCESS_READ, &pa) == TLB_RESULT_MISS);
    CHECK(tlb.invalidation_generation == token);
    tlb_entry_t invalid = mapping;
    invalid.valid = false;
    memcpy(&before, &tlb, sizeof(tlb));
    CHECK(tlb_complete_walk(&tlb, token, &invalid, 0x102A, 32, 7,
          TLB_ACCESS_READ, &pa) == TLB_RESULT_INVALID_TRANSLATION);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    CHECK(complete(&tlb, token, &pa) == TLB_RESULT_HIT && pa == 0x802A);
    CHECK(tlb_invalidate_overlap(&tlb, 0x1000, 0xFFFFF000, 32, 7, &removed)
          == TLB_INVALIDATION_REMOVED);
    CHECK(removed == 1 && tlb.invalidation_generation != token);
    memcpy(&before, &tlb, sizeof(tlb));
    pa = 99;
    CHECK(complete(&tlb, token, &pa) == TLB_RESULT_STALE_REFILL);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    CHECK(complete(&tlb, tlb.invalidation_generation, &pa) == TLB_RESULT_HIT);
    /* No-match and unrelated-ASID invalidations conservatively stale walks. */
    for (unsigned asid = 7; asid <= 8; ++asid) {
        token = tlb.invalidation_generation;
        CHECK(tlb_invalidate_overlap(&tlb, 0x9000, 0xFFFFF000, 32, asid,
                                     &removed) == TLB_INVALIDATION_NO_MATCH);
        CHECK(removed == 0 && tlb.invalidation_generation != token);
        memcpy(&before, &tlb, sizeof(tlb));
        pa = 99;
        CHECK(complete(&tlb, token, &pa) == TLB_RESULT_STALE_REFILL);
        CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    }
    token = tlb.invalidation_generation;
    CHECK(tlb_invalidate_overlap(&tlb, 0x1001, 0xFFFFF000, 32, 7, &removed)
          == TLB_INVALIDATION_INVALID_ARGUMENT);
    CHECK(tlb.invalidation_generation == token);
    CHECK(complete(&tlb, token, &pa) == TLB_RESULT_HIT);
    /* A new mapping survives a late completion for the old generation. */
    CHECK(tlb_invalidate_overlap(&tlb, 0x1000, 0xFFFFF000, 32, 7, &removed)
          == TLB_INVALIDATION_REMOVED);
    tlb_entry_t newer = mapping;
    newer.physical_base = 0xA000;
    CHECK(tlb_complete_walk(&tlb, tlb.invalidation_generation, &newer,
          0x102A, 32, 7, TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    memcpy(&before, &tlb, sizeof(tlb));
    pa = 99;
    CHECK(complete(&tlb, token, &pa) == TLB_RESULT_STALE_REFILL);
    CHECK(memcmp(&before, &tlb, sizeof(tlb)) == 0 && pa == 99);
    CHECK(tlb_probe(&tlb, 0x102A, 32, 7, TLB_ACCESS_READ, &pa) == TLB_RESULT_HIT);
    CHECK(pa == 0xA02A);
    /* The synchronous wrapper also captures the token before its callback. */
    tlb_init(&tlb);
    pa = 99;
    CHECK(tlb_access(&tlb, 0x102A, 32, 7, TLB_ACCESS_READ,
                    invalidating_walk, &tlb, &pa) == TLB_RESULT_STALE_REFILL);
    CHECK(pa == 99 && tlb.stats.current_occupancy == 0);
    CHECK(tlb.stats.accesses == 1 && tlb.stats.misses == 1);
    CHECK(tlb.stats.page_walks == 1 && tlb.stats.unresolved_misses == 1);
    CHECK(tlb.stats.refills == 0);
    /* Exhaustion must never wrap around and validate an ancient token. */
    tlb.invalidation_generation = UINT64_MAX - 1;
    CHECK(tlb_invalidate_overlap(&tlb, 0, 0xFFFFF000, 32, 7, &removed)
          == TLB_INVALIDATION_NO_MATCH);
    CHECK(tlb.invalidation_generation == UINT64_MAX);
    CHECK(complete(&tlb, UINT64_MAX, &pa) == TLB_RESULT_STALE_REFILL);
    CHECK(complete(&tlb, 0, &pa) == TLB_RESULT_STALE_REFILL);
    CHECK(tlb_invalidate_overlap(&tlb, 0, 0xFFFFF000, 32, 7, &removed)
          == TLB_INVALIDATION_NO_MATCH);
    CHECK(tlb.invalidation_generation == UINT64_MAX);
    CHECK(complete(NULL, 0, &pa) == TLB_RESULT_INVALID_ARGUMENT);
    return failures ? 1 : 0;
}
