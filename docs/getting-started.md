# Getting started with Lorenzo2D 1.0

Lorenzo2D installs as a CMake package and ships four starter projects for the
official desktop development paths:

- `templates/platformer`;
- `templates/top-down`;
- `templates/point-and-click`;
- `templates/isometric`.

All four templates consume only installed public API and are configured in CI.

## Minimal consumer project

Create a CMake project:

```cmake
cmake_minimum_required(VERSION 3.28)
project(MyGame LANGUAGES CXX)

find_package(Lorenzo2D 1.0 CONFIG REQUIRED)

add_executable(MyGame main.cpp)
target_compile_features(MyGame PRIVATE cxx_std_17)
target_link_libraries(MyGame PRIVATE Lorenzo2D::Lorenzo2D)
```

Then derive from `l2d::Application` and implement the fixed-step/presentation
callbacks needed by the game.

## Runtime order

A typical game keeps these responsibilities separate:

1. `onFrameStart`: sample frame-scoped input and UI;
2. `onFixedPreSimulation`: controllers, navigation commands, scene fixed update;
3. `onFixedSimulation`: physics world step;
4. `onFixedPostSimulation`: consume contact-driven gameplay;
5. `onUpdate`: camera, audio voice cleanup, other presentation state;
6. `onRender`: world, debug drawing, then screen-space UI.

Save-game writes should be triggered by game/application state, not from the
physics solver.

## Choosing a starter

Use platformer for side-view movement and one-way/slope behavior; top-down for
orthogonal free/grid movement; point-and-click for navigation-driven movement;
and isometric for Cartesian gameplay presented through diamond projection,
picking, placement, and projected-Y ordering.

Each template is intentionally small enough to copy into a new game and expand.

## Package dependencies

The installed Lorenzo2D 1.x package requires SFML 3.1 Graphics and Audio. If a
parent project already defines SFML targets it must provide both
`SFML::Graphics` and `SFML::Audio`.

## Next documentation

- [Input](input.md)
- [Physics contract](physics-contract.md)
- [Character motor](character-motor.md)
- [Top-down movement](top-down-movement.md)
- [Platformer movement](platformer-movement.md)
- [Navigation](navigation.md)
- [Isometric](isometric.md)
- [UI](ui.md)
- [Audio](audio.md)
- [Saves](saves.md)
- [Versioning](versioning.md)
