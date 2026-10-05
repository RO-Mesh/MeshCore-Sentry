# skill-git-fork-sync

Remote topology:

```bash
git remote add upstream-core https://github.com/meshcore-dev/MeshCore.git
git remote add fork-jhuebert https://github.com/jhuebert/MeshCore.git
```

Routine sync:

```bash
git fetch upstream-core dev main
git fetch fork-jhuebert repeater-filter-stable
git switch ro-mesh-sentry
git rebase fork-jhuebert/repeater-filter-stable
```

Rules:

- Keep Sentry work on `ro-mesh-sentry`.
- Do not edit MeshCore core sources unless no example-level hook can do the job.
- Resolve conflicts by preserving upstream behavior first, then reapplying the
  guarded Sentry hook.
