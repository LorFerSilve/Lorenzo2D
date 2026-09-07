# Lorenzo2D platformer starter

This standalone Lorenzo2D 0.13 project demonstrates the public platformer API using an installed
package. It includes running, buffered variable-height jumping, a one-way platform, step-up, and a
moving platform.

After installing Lorenzo2D, configure this directory with its install prefix:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/lorenzo2d-install
cmake --build build
```

Controls: A/D to run, Space to jump, and S to drop through the purple platform.
