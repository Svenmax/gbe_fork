# Requirements: Direct Target Local Lobby Indexed Mutating Method Guard

## Goal

Guard audit 10d against target-pointer indexed Local lobby mutating method calls such as `options.client_target->GBE_local_lobby.members[0].slots.clear()`.

## Requirements

- The audit shall reject mutating method calls on indexed `GBE_local_lobby` member chains reached through `options.client_target->` or equivalent target pointer prefixes.
- The guard shall report the existing direct Local lobby field write failure text.
- The change shall not modify production GC behavior.

## Non-Goals

- No production handler refactor.
- No routing inventory changes.
- No state helper additions.
