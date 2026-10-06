# gc-inventory-request-emsg-helper design

Status: In Progress

## Scope

This slice changes only Python audit helper code and tests. It does not change Markdown truth tables or production C++ routing.

## Approach

- Add `REQUEST_EMSG_TOKEN_TO_EMSG` for named request tokens used by direct fallback audit.
- Add `extract_request_emsg_comparisons(body, token_to_emsg=REQUEST_EMSG_TOKEN_TO_EMSG)` near existing inventory helpers.
- Replace local direct conditional fallback extraction with the shared helper.
- Add helper-level tests for numeric literals, unsigned suffixes, and named tokens.

## Safety

- Existing expected emsg sets stay unchanged.
- Existing issue strings stay unchanged.
- Full GC verification remains the completion gate.
