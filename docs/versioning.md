# Versioning and compatibility

Lorenzo2D follows semantic versioning with a public `MAJOR.MINOR.PATCH` version. The version passed
to CMake's `project()` command is available to consumers through
`<Lorenzo2D/Core/Version.hpp>`.

Lorenzo2D 1.0.0 establishes the first stable public source/package contract for the documented
desktop baseline.

## Version meaning from 1.0

- `PATCH` releases contain backward-compatible fixes and hardening.
- `MINOR` releases add backward-compatible public functionality.
- `MAJOR` releases may remove or incompatibly change public source, package, or persisted-data
  contracts.

A public API scheduled for removal should be deprecated for at least one 1.x minor release when a
safe compatibility adapter is practical. Emergency correctness or security fixes may require a
shorter path and must document the exception.

## CMake package compatibility

The installed `Lorenzo2DConfigVersion.cmake` uses CMake's `SameMajorVersion` policy from 1.0.
A consumer requesting Lorenzo2D 1.0 can therefore accept a later compatible 1.x package, while a
2.x package does not satisfy that request automatically.

```cmake
find_package(Lorenzo2D 1.0 CONFIG REQUIRED)
target_link_libraries(MyGame PRIVATE Lorenzo2D::Lorenzo2D)
```

Lorenzo2D is a static library. Source/package compatibility is the primary 1.x promise; consumers
should rebuild their executable and Lorenzo2D together when compiler, standard-library, build
configuration, or toolchain ABI changes.

## Public compatibility surfaces

The following are compatibility surfaces:

- installed headers under `include/Lorenzo2D`;
- the `Lorenzo2D::Lorenzo2D` CMake target and documented CMake options;
- required public dependencies, currently SFML 3.1 Graphics and Audio;
- versioned level and save formats;
- documented fixed-step callback and event ordering;
- asset lookup, UI interaction, audio-service, and package-installation behavior.

Private headers under `src`, test helpers, sandbox implementation details, and benchmark scenario
internals are not public API.

Phase 11.8 audited every installed header in the 1.x package. The resulting hardening uses additive
checked APIs, compile-time rejection of invalid temporary lifetimes, internal transactionality
fixes, and documentation; no public 1.x symbol was removed. See
[api-failure-audit.md](api-failure-audit.md).

## Persisted-data compatibility

Persisted engine envelopes carry their own format versions. Game save schemas carry a separate
game-defined revision.

Removing a readable persisted format after 1.0 requires an announced migration path or conversion
tool where practical. A serializer must reject unsupported future envelope versions rather than
silently interpreting them as an older contract.

## Making a breaking change

Every breaking change must include:

1. a rationale and affected compatibility surface;
2. a replacement API or an explicit explanation why none exists;
3. migration of the sandbox, examples, templates, tests, and package consumers;
4. a migration note in release documentation;
5. retained readers or conversion tooling for persisted data where practical.

## Generated version API

Consumers can perform compile-time checks without duplicating the package version:

```cpp
#include <Lorenzo2D/Core/Version.hpp>

static_assert(l2d::VersionMajor == 1u);
static_assert(l2d::VersionMinor >= 0u);
```

The header also exposes `LORENZO2D_VERSION_MAJOR`, `LORENZO2D_VERSION_MINOR`,
`LORENZO2D_VERSION_PATCH`, `LORENZO2D_VERSION_STRING`, and the matching `l2d::Version*`
constants.
