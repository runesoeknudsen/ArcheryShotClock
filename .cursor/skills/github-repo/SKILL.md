---
name: github-repo
description: Use this GitHub repository and its GitHub Actions CI. Use when committing, branching, pushing, opening a pull request, changing .github/workflows, or explaining how tests run on GitHub.
---

# GitHub and Actions

Read this skill before you commit, push, or open a pull request.

## Repository

The repo is **`runesoeknudsen/ArcheryShotClock`**.

- Canonical URL: `https://github.com/runesoeknudsen/ArcheryShotClock`
- Git remote in this workspace: `origin` → `https://github.com/runesoeknudsen/ArcheryShotClock.git`
- Live demo: `https://runesoeknudsen.github.io/ArcheryShotClock/`
- Former name: `runesoeknudsen/esp_display_ws2812B` (renamed; do not treat this as the current repo)

Cursor Cloud run metadata and `GITHUB_REPO` may still show `esp_display_ws2812B`. That is stale identity, not the git remote. Trust `git remote -v`.

## What this agent can do

**Can**

1. Create branches, commit, and `git push -u origin <branch>` to **ArcheryShotClock**.
2. Read GitHub with `gh` when it is installed (PRs, CI logs, issue text).
3. Open or update a PR **only** with the `ManagePullRequest` tool, targeting ArcheryShotClock.

**Cannot**

1. Create or update a PR when `ManagePullRequest` returns `unauthenticated`. That happens when this agent's GitHub identity is still `esp_display_ws2812B` while git pushes to `ArcheryShotClock`.
2. Use `gh`, `origin`, or raw GitHub HTTP to create or edit PRs. Those are not a workaround.
3. Assume `gh` is installed. In some agent VMs it is missing.

If PR creation fails with `unauthenticated`:

1. Push the branch anyway.
2. Tell the user the compare URL, for example  
   `https://github.com/runesoeknudsen/ArcheryShotClock/compare/BASE...HEAD`
3. Do not claim a PR exists. Do not keep retrying the same unauthenticated call.

## Always

1. Work on a branch. When starting a new job, always branch from `main`. Do not commit straight to `main` unless the user says so.
2. Write a short commit message that says what changed and why.
3. Push the same branch you committed.
4. Keep `.github/workflows/ci.yml` able to run with no secrets and no extra hardware.
5. If you change how tests run, change the workflow in the same commit.

## Never

1. Never force-push to `main`.
2. Never delete the workflow to hide a failing test.
3. Never add a CI step that needs a real ESP32, Wi-Fi passwords, or a paid service.
4. Never commit these folders: `.pio/`, `node_modules/`, `playwright-report/`, `test-results/`, `src/web_page.h`.
5. Never put tokens, passwords, or private keys in the repo.
6. Never open a PR against `esp_display_ws2812B` or tell the user that is the current repo.

## Branches you will see

| Branch | What it is |
|---|---|
| `main` | Current published line |
| `v1` | World Archery clock plus Bluetooth speaker |
| `v2` | World Archery clock plus MAX98357A speaker and volume |

When starting a new job, always branch from `main`, not from `v1`, `v2`, or another feature branch. To edit more than one branch at once, use a git worktree. See [git-worktrees](../git-worktrees/SKILL.md).

## What GitHub Actions runs

The file is `.github/workflows/ci.yml`.
It starts on every `push` and every `pull_request`.

There are two jobs. They run in parallel.

### Job `firmware`

1. Check out the repo.
2. Install Python 3.11.
3. Install PlatformIO.
4. Run `pio test -e native`.
5. Run `python software/tools/logcheck.py --selftest`.
6. Build the ESP32 program with `pio run -e esp32dev`.

This job does not flash a board.

### Job `browser`

1. Check out the repo.
2. Install Node.js 22.
3. Run `npm ci`.
4. Install Chromium.
5. Run `npm test` (Playwright).
6. Upload the Playwright report if the job fails or succeeds.

This job does not need an ESP32.

## Commands to copy

Save your work:

```text
git status
git add -A
git commit -m "Short description of the change."
git push -u origin BRANCH_NAME
```

Run the same checks GitHub will run:

```text
pio test -e native -d software/firmware
python3 software/tools/logcheck.py --selftest
pio run -e esp32dev -d software/firmware
npm ci
npx playwright install chromium
npm test
```

You need a `python` or `python3` on the PATH for Playwright's local web server.

## If CI fails

1. Open the failed job log on GitHub.
2. Read the first error, not only the last line.
3. Reproduce it on your machine with the command from that job.
4. Fix the code or the test.
5. Commit and push again.

Do not mark a PR ready if `firmware` or `browser` is red.
