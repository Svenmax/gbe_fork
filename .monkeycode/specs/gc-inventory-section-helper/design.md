# gc-inventory-section-helper design

Status: Completed

## Scope

This slice refactors inventory section slicing inside `tools/_audit_gc_refactor.py`. It keeps audit semantics and production C++ unchanged.

## Approach

- Add `inventory_section(inventory_text, heading)` near the file read helper.
- Return `None` when the heading is absent.
- Return text from the heading through the next level-2 heading boundary.
- Replace duplicated section slicing in routing and Local/shared merge inventory audits.

## Safety

- Existing consumer-specific missing section messages stay at call sites.
- Existing regression tests continue exercising each consumer boundary.
- Full GC verification remains the completion gate.
