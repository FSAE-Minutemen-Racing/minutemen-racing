# Contributing — Minutemen Racing Telemetry

This document defines how the team collaborates on this repository: the branching
model, branch naming conventions, commit style, and the pull-request workflow.
Following it keeps `main` always-flashable and makes it easy to see who is working
on what.

---

## Branching model

We use a lightweight two-trunk model:

| Branch      | Purpose                                                                 | Who merges into it            |
| ----------- | ---------------------------------------------------------------------- | ----------------------------- |
| `main`      | **Stable / flashable.** Only code that has been tested on the car.      | Release PRs from `develop`, hotfixes |
| `develop`   | **Integration.** Where day-to-day work lands and is tested together.    | Feature / bug / chore PRs     |
| short-lived | One branch per issue (see naming below). Deleted after merge.           | —                             |

**Golden rules**

- Never commit directly to `main` or `develop` — always open a pull request.
- Every working branch starts from the latest `develop` (except `hotfix/*`, which
  starts from `main`).
- One branch = one issue = one PR. Keep changes focused and reviewable.
- Delete your branch after it merges.

```
main ─────●───────────────────────●──────────────▶  (stable, on-car)
           \                      /  ▲
develop ────●────●────●────●────●─┘  │ release PR
                  \    \    \        │
       feature/…   ●    \    \       │
       bug/…            ●    \       │
       chore/…               ●───────┘  (PRs into develop)

hotfix/… branches off main, merges to main AND develop
```

---

## Branch naming conventions

Format:

```
<type>/<issue-number>-<short-kebab-description>
```

Examples:

```
feature/2-gps-forwarding-to-uno
bug/3-http-handler-timeout
bug/5-rpm-divide-by-zero
hotfix/3-server-hang
chore/12-remove-kline-dead-code
docs/16-system-architecture
```

### Types

| Prefix        | Use for                                                             | Branch from | Merge into        |
| ------------- | ------------------------------------------------------------------ | ----------- | ----------------- |
| `feature/`    | New functionality or capability                                    | `develop`   | `develop`         |
| `bug/`        | Fixing incorrect behavior                                          | `develop`   | `develop`         |
| `hotfix/`     | Urgent fix that must reach the car **now** (bypasses `develop`)     | `main`      | `main` + `develop`|
| `chore/`      | Tooling, deps, cleanup, dead-code removal, refactors (no behavior) | `develop`   | `develop`         |
| `docs/`       | Documentation only                                                 | `develop`   | `develop`         |
| `experiment/` | Spikes / throwaway prototypes (not expected to merge as-is)         | `develop`   | usually discarded |

**Guidelines**

- Always include the issue number so the branch, PR, and issue link up.
- Use lowercase kebab-case for the description; keep it under ~5 words.
- If there is genuinely no issue (rare), use a descriptive slug without a number
  and open the issue anyway when you can.

---

## Commit messages

- Write in the imperative mood: *"add HTTP read timeout"*, not *"added"* / *"adds"*.
- Keep the subject line ≤ 72 characters; add a body if the *why* isn't obvious.
- Reference issues in the body or PR (e.g. `Refs #3`, `Closes #3`).

Example:

```
Add per-connection timeout to runServer()

The request loop had no deadline, so a silent client could stall the
main loop and freeze the dash. Bound each connection to 250 ms.

Closes #3
```

---

## Pull-request workflow

1. **Create a branch** from `develop` (or `main` for a hotfix) using the naming
   convention above.
2. **Make focused commits.** Rebase/clean up before requesting review if needed.
3. **Push and open a PR** into `develop` (or `main` for a hotfix). Fill out the PR
   template.
4. **Link the issue** with `Closes #<n>` so it auto-closes on merge.
5. **Get at least one review.** For anything that runs on the car, note how it was
   tested (bench, simulated serial, or on-vehicle).
6. **Squash-merge** into `develop` to keep history readable, then delete the branch.
7. **Releases:** when `develop` is validated on the car, open a `develop → main` PR.

### Definition of done

- [ ] Builds for the affected target(s) (`telemetry` = UNO R4, `dashboard` = ESP32).
- [ ] Behavior verified (bench or on-car) and described in the PR.
- [ ] No new dead/unreferenced code left behind.
- [ ] Linked issue closed by the PR.

---

## Projects & targets quick reference

| Directory    | Board             | Toolchain          |
| ------------ | ----------------- | ------------------ |
| `telemetry/` | Arduino UNO R4 WiFi | PlatformIO         |
| `dashboard/` | ESP32 (LVGL)      | Arduino IDE / PIO  |
| `web_site/`  | Browser           | static HTML/JS/CSS |
