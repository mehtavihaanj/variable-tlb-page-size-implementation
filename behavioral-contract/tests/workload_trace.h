#ifndef WORKLOAD_TRACE_H
#define WORKLOAD_TRACE_H
#include <stdint.h>

static const uint64_t region_base[4] = {
    0, UINT64_C(0x01000000), UINT64_C(0x10000000), UINT64_C(0x40000000)};
static const uint64_t region_size[4] = {
    0, UINT64_C(8192), UINT64_C(65536), UINT64_C(4194304)};
static const uint64_t page_stride[4] = {0, 5, 7, 13};

/* Trace v1: addresses depend on iteration and ASID, never mapping size.
 * ASIDs must be 1..3. Each stream permutes its fixed 4-KiB blocks, with a
 * rotating cache-line offset. One iteration visits ASIDs 1, 2, 3 in order.
 */
static uint64_t workload_address(uint64_t iteration, uint32_t asid)
{
    uint64_t blocks = region_size[asid] / 4096;
    uint64_t block = (iteration % blocks) * page_stride[asid] % blocks;
    uint64_t offset = (iteration % 64) * 64 + asid * 16;
    return region_base[asid] + block * 4096 + offset;
}

/* Diagnostic fingerprint; exact address/coverage tests are the oracle. */
static uint64_t workload_hash(uint64_t hash, uint64_t address, uint32_t asid)
{
    return (hash ^ address ^ asid) * UINT64_C(1099511628211);
}
#define WORKLOAD_HASH_SEED UINT64_C(14695981039346656037)
#endif
