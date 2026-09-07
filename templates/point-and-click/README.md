# Lorenzo2D point-and-click starter

This standalone Lorenzo2D 0.13 project uses only the installed public API. Left-clicking a
walkable location computes a deterministic A* path and follows it through the shared top-down
controller and collision-aware character motor.

Build Lorenzo2D into an install prefix, then configure this folder with that prefix:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/lorenzo2d/install
cmake --build build
```

Static obstacles are baked once at startup. For changing maps, update the grid and request a new
path when `PathFollower2D::isPathStale()` becomes true. Collect nearby
`NavigationAgentSnapshot2D` values each fixed tick to enable multi-agent local avoidance.
