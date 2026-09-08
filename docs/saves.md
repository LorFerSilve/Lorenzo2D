# Save-game persistence

Lorenzo2D 1.0 adds a small versioned persistence layer for game-owned state.
It does not serialize arbitrary C++ object graphs. Games explicitly choose the
keys and values that form their save contract.

## SaveDocument

`SaveDocument` has two identity fields:

- `schema`: a non-empty game-defined schema name;
- `revision`: a positive game-defined schema revision.

Values are stored by string key and support:

- boolean;
- signed 64-bit integer;
- finite double;
- UTF-8/byte string.

The ordered key map gives deterministic serialized output for identical state.

```cpp
l2d::SaveDocument save("my-game", 1u);
save.setInteger("player.coins", 42);
save.setBool("boss.defeated", true);
save.setString("checkpoint", "harbor");
```

Typed getters return `std::optional`; requesting a key through the wrong type
returns no value rather than coercing it.

## File format

`SaveGameSerializer::FormatVersion` is the engine envelope format version.
Game schema revision is independent from that envelope.

The JSON envelope stores:

- `format_version`;
- `schema`;
- `revision`;
- typed `values`.

Readers reject unsupported envelope versions. Invalid input is transactional:
the destination `SaveDocument` is replaced only after the complete document
has validated.

## Workload limits

`SaveGameLimits` bounds entry count, key/schema length, individual string
length, and total serialized bytes. These limits apply to both read and write
paths so malformed or untrusted data cannot request unbounded work.

Non-finite floating-point values are rejected.

## File replacement

`saveToFile` writes to a sibling temporary file before replacing the target.
When an existing save is present it is first moved to a temporary backup, and a
failed final rename attempts to restore that previous file.

This prevents a serialization failure from truncating the last valid save.

## Migration policy

The engine envelope format is versioned independently from a game's schema.
Games should increment their schema revision when game-owned fields change and
perform migration in game code after loading older supported revisions.

After Lorenzo2D 1.0, removal of a readable persisted format requires an announced
migration path under the compatibility policy.

## Scope and exclusions

The 1.0 baseline does not provide cloud synchronization, encryption, compression,
multi-profile slot management, automatic ECS snapshots, schema migration code
generation, or conflict resolution.
