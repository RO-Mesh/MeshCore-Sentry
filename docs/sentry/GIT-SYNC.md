# Git Synchronization Guide

Current base:

- `fork-jhuebert/repeater-filter-stable`
- `upstream-core/dev`
- `upstream-core/main`

Recommended one-time setup:

```bash
git remote add upstream-core https://github.com/meshcore-dev/MeshCore.git
git remote add fork-jhuebert https://github.com/jhuebert/MeshCore.git
git switch -c ro-mesh-sentry
```

Routine update:

```bash
git fetch upstream-core dev main
git fetch fork-jhuebert repeater-filter-stable
git switch ro-mesh-sentry
git rebase fork-jhuebert/repeater-filter-stable
```

Conflict policy:

- Preserve upstream/fork code first.
- Reapply only the guarded `RO_MESH_SENTRY` hook lines.
- Keep feature logic in `examples/simple_repeater/SentryManager.*`.
- Build and run native tests before firmware builds.
