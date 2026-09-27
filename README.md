# Variable-Page-Size TLB

This project will implement and test a small C model of the first design in the accompanying report: an eight-entry, fully associative TLB that uses a bitmask to match translations for different page sizes.

The implementation will be developed test-first, one component at a time. Each component gets focused tests and a reviewable addition before it is integrated with the next component. The behavioral contract is currently in its test-first scaffold; translation behavior is not implemented yet.

## Design Scope

- Eight fully associative translation entries, searched in parallel by the model.
- Variable power-of-two page sizes represented by an address mask.
- Address-space identifiers and read, write, and execute permissions.
- Page-walk/refill boundary, overlap invalidation, and LRU replacement.
- Counters and reproducible workloads for TLB behavior and performance analysis.

The initial implementation models the first bitmask design only. It does not implement the report's separate skewed-associative design.

## Test-Driven Sequence

1. **Behavioral contract:** Define address and mask semantics, matching, ASID, validity, permissions, and multiple-match behavior. Write tests before implementation. See [behavioral-contract/README.md](behavioral-contract/README.md).
2. **Mask and address helpers:** Validate supported masks and implement page matching, offset extraction, and physical-address construction.
3. **Entry and TLB lookup:** Add the eight-entry fully associative lookup, including ASID and permission handling.
4. **Page-walk/refill boundary:** Introduce a deterministic fake walker for tests, then refill on misses.
5. **Invalidation:** Remove translations that overlap a reallocated or split mapping; ensure lookups cannot use stale entries.
6. **Replacement:** Implement and test true LRU. 
7. **Integration and measurement:** Exercise full access traces, then benchmark fixed workloads and report counters and memory use.

Each step starts with tests for its observable behavior. A component is integrated only after its focused tests pass. Integration tests then verify the combined path without replacing the component tests.

## Measurement Plan

The simulator should expose these core counts: accesses, hits, misses, page walks/refills, evictions, and occupancy. Per-page-size hit/miss counts and invalidation counts are recommended to make variable-size behavior visible.

Performance benchmarks will use repeatable traces and report elapsed time per access separately from correctness tests. Timing is diagnostic, not a pass/fail assertion, because host scheduling and hardware affect it. Memory reporting will distinguish the nominal packed entry bits from the actual C structure and allocated storage, which can include padding.

A C model cannot directly measure hardware TLB energy. It can report event counts and, if approved, calculate an explicitly labeled energy estimate using configurable costs per event. That estimate must not be presented as measured joules.

## Design Checks to Preserve

- Define one mask convention and use it consistently for matching and offset extraction.
- Require valid entry, matching ASID, and matching virtual address for a translation hit; check permissions as a separate result.
- Detect multiple matching entries as an invariant violation instead of selecting an arbitrary translation.
- Invalidate overlapping translations after mapping changes.
- Report the nominal storage correctly: 52 bits per entry times eight entries is 416 bits. Actual C storage will generally be larger.
