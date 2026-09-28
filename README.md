# Variable-Page-Size TLB

This project will implement and test a small C model of the first design in the accompanying report: an eight-entry, fully associative TLB that uses a bitmask to match translations for different page sizes.

The implementation is developed test-first, one component at a time. Mask-aware lookup, refill, overlap invalidation, LRU replacement, and event metrics are implemented. A deterministic process-mix harness compares fixed and mixed page-size mappings.

## Design Scope

- Eight fully associative translation entries, searched in parallel by the model.
- Variable power-of-two page sizes represented by an address mask.
- Address-space identifiers and read, write, and execute permissions.
- Page-walk/refill boundary, overlap invalidation, and LRU replacement.
- Counters and reproducible workloads for TLB behavior and performance analysis.

The initial implementation models the first bitmask design only. It does not implement the report's separate skewed-associative design.

## Test-Driven Sequence

1. **Behavioral contract:** Define and test mask semantics, matching, ASID, validity, permissions, and ambiguity. See [behavioral-contract/README.md](behavioral-contract/README.md).
2. **State and refill:** Add fixed-capacity TLB state and a page-walker boundary.
3. **Invalidation:** Remove translations that overlap a reallocated or split mapping.
4. **Replacement:** Implement and test true LRU.
5. **Integration and measurement:** Exercise multi-process traces, then benchmark fixed workloads and report counters and memory use.

Each step starts with tests for its observable behavior. A component is integrated only after its focused tests pass. Integration tests then verify the combined path without replacing the component tests.

## Measurement Plan

The simulator records accesses, hits, misses, permission faults, ambiguous lookups, page walks/refills, unresolved misses, evictions, invalidations, and current/peak occupancy. Hit and resolved-miss counts are bucketed by page-size order; failed walks remain unresolved because their mapped size is unknown.

The workload harness interleaves three ASID-tagged streams in a deterministic round-robin schedule and compares all-4-KiB mappings with a fixed 4-KiB/64-KiB/2-MiB mapping mix. It reports aggregate and per-process counters, modeled storage, and host CPU time. CTest checks expected deterministic counts and miss reduction, never elapsed time; this is a policy harness, not an OS scheduler. Timing is diagnostic because host scheduling affects it. Memory reporting distinguishes nominal 416 entry bits from actual C model storage.

A C model cannot directly measure hardware TLB energy. It can report event counts and, if approved, calculate an explicitly labeled energy estimate using configurable costs per event. That estimate must not be presented as measured joules.

## Design Checks to Preserve

- Define one mask convention and use it consistently for matching and offset extraction.
- Require valid entry, matching ASID, and matching virtual address for a translation hit; check permissions as a separate result.
- Detect multiple matching entries as an invariant violation instead of selecting an arbitrary translation.
- Invalidate overlapping translations after mapping changes.
- Report the nominal storage correctly: 52 bits per entry times eight entries is 416 bits. Actual C storage will generally be larger.
