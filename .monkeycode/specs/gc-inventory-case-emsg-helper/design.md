# gc-inventory-case-emsg-helper design

Status: In Progress

## Scope

This slice changes only Python audit helper code and tests. It does not change Markdown truth tables or production C++ routing.

## Approach

- Add `CASE_TOKEN_TO_EMSG` for known named Dota case labels used by routing audits.
- Add `extract_case_emsgs(body, token_to_emsg=CASE_TOKEN_TO_EMSG)` near existing inventory helpers.
- Replace local template replay `case` parsing with the shared helper.
- Add helper-level tests for numeric and named labels.

## Safety

- Existing expected emsg sets stay unchanged.
- Existing issue strings stay unchanged.
- Full GC verification remains the completion gate.
