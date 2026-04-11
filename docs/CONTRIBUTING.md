# Contributing

## Development Setup

1. Install system packages:

   ```bash
   ./scripts/setup_ubuntu_deps.sh
   ```

2. Install Qt 6.11 and export `QT_ROOT`.
3. Build local MapLibre Native Qt:

   ```bash
   ./scripts/setup_maplibre_qt6.sh
   ```

4. Create your local environment file:

   ```bash
   cp env.example env
   ```

5. Configure, build, and run:

   ```bash
   ./scripts/configure.sh
   ./scripts/build.sh
   ./scripts/run.sh
   ```

Before spending time on a larger change, make sure you can reproduce a working local build and a clean application launch.

## Repository Map

- `src/` contains first-party C++ code
- `qml/` contains the QML application layer
- `third_party/` contains vendored dependencies
- `legacy/` contains archived code kept out of the primary build
- `resources/` contains QRC manifests

See [docs/PROJECT_LAYOUT.md](/home/boots/work/phd/GCSNetworkPlanner/docs/PROJECT_LAYOUT.md) for a fuller breakdown.

## Coding Expectations

- Keep new code within the current domain-oriented layout.
- Avoid adding new root-level source files.
- Preserve environment-driven configuration instead of hardcoding personal tokens or machine-specific paths.
- Keep vendor code isolated under `third_party/` unless you are intentionally patching a vendored dependency.
- Prefer targeted, reviewable changes over broad rewrites of stable runtime behavior.
- Keep documentation in sync when you add new runtime configuration or setup requirements.
- Do not commit local `env`, build artifacts, or machine-specific generated files.

## Documentation Expectations

Contributor-facing changes should update the relevant documentation:

- build/setup changes -> `docs/BUILD.md`
- runtime configuration changes -> `docs/CONFIGURATION.md`
- architectural changes -> `docs/ARCHITECTURE.md` or `docs/PROJECT_LAYOUT.md`
- support/security process changes -> `SUPPORT.md` or `SECURITY.md`

## Pull Requests

Before opening a PR:

1. run `./scripts/configure.sh`
2. run `./scripts/build.sh`
3. run `./scripts/run.sh`
4. verify the QML UI loads and the map stack initializes

Include in the PR description:

- what changed
- how it was verified
- any environment or dependency assumptions

If the change affects users or contributors, include the documentation update in the same PR.

## Community Standards

By contributing, you agree to follow the project’s [CODE_OF_CONDUCT.md](/home/boots/work/phd/GCSNetworkPlanner/CODE_OF_CONDUCT.md).
