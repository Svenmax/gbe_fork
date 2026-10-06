# Requirements: Direct Local Lobby Parenthesized Field Write Guard

## Goal

Guard audit 10d against parenthesized direct Local lobby field writes such as `(GBE_local_lobby).state = 1u`.

## Requirements

- The audit shall reject field writes through a parenthesized `GBE_local_lobby` expression.
- The guard shall report the existing direct Local lobby field write failure text.
- The change shall not modify production GC behavior.

## Non-Goals

- No production handler refactor.
- No routing inventory changes.
- No state helper additions.
