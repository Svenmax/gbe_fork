# Requirements: Direct Target Local Lobby Indexed Postfix Write Guard

## Goal

Guard audit 10d against target-pointer indexed postfix Local lobby mutations such as `options.client_target->GBE_local_lobby.members[0].team++`.

## Requirements

- The audit shall reject postfix `++` / `--` mutations on indexed `GBE_local_lobby` field chains reached through `options.client_target->` or equivalent target pointer prefixes.
- The guard shall report the existing direct Local lobby field write failure text.
- The change shall not modify production GC behavior.

## Non-Goals

- No production handler refactor.
- No routing inventory changes.
- No state helper additions.
