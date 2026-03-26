# Contributing to Aria Build System

aria-make is the official build system for Aria projects, featuring incremental builds, parallel compilation, and first-class FFI support.

## Building from Source

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Repository Structure

- **src/** — Core build system (C++)
- **include/** — Headers
- **docs/** — Configuration reference

## Build Configuration

Projects use `build.abc` files in ABC format:

```abc
[project]
name = "my-project"
version = "1.0.0"

[sources]
aria = "src/*.aria"

[ffi]
c_sources = "src/ffi/*.c"
```

## Contribution Areas

- Incremental build improvements (SHA-256 content hashing)
- Dependency resolution and cycle detection
- FFI compilation pipeline (C/C++ integration)
- Glob pattern matching
- Documentation and examples

## Commit Messages

```
type: description

feat: add C++ source compilation support
fix: handle circular dependencies in glob expansion
perf: parallelize hash computation
```

## License

By contributing, you agree that your contributions will be licensed under the same terms as this project (see LICENSE).
