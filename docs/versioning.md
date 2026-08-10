# Versioning and compatibility

Lorenzo2D uses a semantic-version-shaped `MAJOR.MINOR.PATCH` version. The public version is the
version passed to CMake's `project()` command and is available to consumers through
`<Lorenzo2D/Core/Version.hpp>`.

## Version meaning before 1.0

While the major version is zero, Lorenzo2D is still establishing its public gameplay and tooling
contracts:

- `PATCH` releases contain compatible fixes, validation hardening, documentation, and internal
  performance work.
- `MINOR` releases may add features and may contain announced public API changes.
- A public API scheduled for removal is deprecated for at least one minor release whenever a safe
  compatibility adapter is practical.
- File formats remain readable for the versions explicitly listed by their serializer. Removing a
  readable format version requires an announced migration path.

Code using deprecated APIs should still build with the repository's warnings-as-errors policy.
Deprecation diagnostics are therefore enabled only when the project can provide a clean migration
window without breaking its own supported consumers.

## Version meaning from 1.0

After 1.0:

- `PATCH` releases are backward-compatible fixes.
- `MINOR` releases add backward-compatible functionality.
- `MAJOR` releases may change public source, binary, package, or persisted-data compatibility.

Installed CMake package compatibility continues to use `SameMinorVersion` before 1.0. The policy
will be reviewed as part of 1.0 hardening rather than silently changed.

## Public compatibility surfaces

The following are compatibility surfaces:

- installed headers under `include/Lorenzo2D`;
- the `Lorenzo2D::Lorenzo2D` CMake target and its documented options;
- versioned level, map, prefab, and save formats;
- documented fixed-step callback and event ordering;
- asset lookup and package installation behavior.

Private headers under `src`, test helpers, sandbox implementation details, and benchmark scenario
internals are not public API.

## Making a breaking change

Every breaking change must include:

1. a rationale and affected compatibility surface;
2. a replacement API or an explicit explanation why none exists;
3. migration of the sandbox, examples, tests, and package consumers;
4. a migration note in the release documentation;
5. retained readers or conversion tooling for persisted data where practical.

## Generated version API

Consumers can perform compile-time checks without duplicating the package version:

```cpp
#include <Lorenzo2D/Core/Version.hpp>

static_assert(l2d::VersionMajor == 0u);
static_assert(l2d::VersionMinor >= 4u);
```

The header also exposes `LORENZO2D_VERSION_MAJOR`, `LORENZO2D_VERSION_MINOR`,
`LORENZO2D_VERSION_PATCH`, `LORENZO2D_VERSION_STRING`, and the matching `l2d::Version*`
constants.
