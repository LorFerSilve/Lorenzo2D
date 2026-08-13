# Top-down game template

This is a minimal standalone Lorenzo2D 0.12 project. It uses only installed public headers and the
exported `Lorenzo2D::Lorenzo2D` CMake target.

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/path/to/lorenzo2d/install
cmake --build build --config Release
```

Run `TopDownGame` and move with WASD. Copy this directory when starting an orthogonal free-movement
game. For cell-by-cell movement, replace `TopDownController2D` with `GridStepController2D` as shown
in `examples/phase6.cpp` and the top-down movement guide.
