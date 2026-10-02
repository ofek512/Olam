# Project Coding Rules

Full design/roadmap: [agent.MD](agent.MD).

1. Simulation code must never depend on rendering code.
2. World generation must always be deterministic.
3. Never use global random state.
4. Do not introduce new dependencies without approval.
5. Do not redesign unrelated systems during a task.
6. Keep changes scoped to the requested milestone.
7. Compile after significant changes.
8. Run relevant tests after changes.
9. Every new WorldGen stage should include tests.
10. Do not replace working systems simply because another architecture is fashionable.
11. Prefer simple implementations first.
12. Do not optimize without evidence from profiling.
13. Do not hard-code gameplay rules into rendering systems.
14. Do not use external images without verifying their license.
15. Document major architectural decisions.
16. Ask for architectural guidance when a requested change conflicts with documented architecture.

## Layering

`platform/` (SDL3) -> `core/` + `render/` -> world simulation -> game systems.
Lower layers never include headers from higher layers.

- `olam_core` target: pure C++20, no SDL (logging, time, input, files, math, camera). Unit-tested.
- `olam_engine` target: SDL platform, renderer, debug rendering, `Application`.

## Build & test (Windows, MSVC)

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\Olam.exe
```
