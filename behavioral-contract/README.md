# Behavioral Contract

This folder will hold the first independently tested C component: the TLB's externally observable rules and the smallest matching implementation. Its purpose is to settle how an access is interpreted before adding page walks, replacement, or integration behavior.

## Proposed Contract

- Use `uint64_t` for virtual and physical addresses, with the supported address width and page sizes defined by configuration.
- Support power-of-two page sizes. A page's address mask has `1` bits for address bits that identify the mapping and `0` bits for page-offset bits. The low offset bits must be contiguous zeros in the mask.
- Store aligned virtual and physical page bases, the mask, a valid bit, an address-space identifier (ASID), and read/write/execute permissions in each entry.
- A virtual match requires the entry to be valid, the ASID to match, and all address bits selected by the mask to match. Bits masked off are within the page and do not distinguish translations.
- Physical-address construction preserves the page offset while taking the page base from the entry. Matching and offset extraction must use the same mask convention.
- A translation hit is distinct from permission approval: a matching entry with insufficient permission reports a permission fault, not a miss.
- No matching entry reports a miss. More than one matching entry reports an ambiguous-match invariant violation; lookup must not silently choose one.

These rules are the proposed starting contract. Later components will enforce uniqueness through refill and overlap invalidation.

## Test-First Coverage

Tests will cover valid and invalid entries, mask validity, matching and nonmatching addresses, page boundaries, ASID isolation, each permission, permission faults, and ambiguous matches. Small illustrative address widths will make boundary cases easy to inspect; realistic page-size configurations will be checked as well.

The first addition introduces the API contract, minimal test harness, and contract tests. Placeholder functions make the test executable report the expected failures before production behavior is implemented. The following addition will replace those placeholders with the contract behavior needed to pass the tests. Each addition is estimated at 160–180 lines, for roughly 320–360 lines total, excluding documentation and build configuration. These are estimates, not line-count targets.

## Planned Files

- A public C header for the entry, configuration, result, and contract API.
- A C source file for the minimal contract behavior.
- A focused test source file and the smallest project test/build configuration needed to run it.

The first addition contains the test scaffold and placeholder functions only; the matching and translation rules remain unimplemented. The tests for this component will be run before proceeding to the next component.
