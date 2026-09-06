---
name: git-worktrees
description: Use git worktrees so more than one branch can be edited at once. New jobs always branch from main. Use when starting a job, starting parallel work, avoiding a stacked PR, using /worktree, or checking out a second branch.
---

# Git worktrees

A normal checkout can only be on one branch. A worktree is a second folder that shares the same `.git` object store, so two branches can be edited at the same time without stashing or stacking.

Read this skill before `git checkout` away from unfinished work, before starting a second feature, and when the user asks to work on more than one branch.

## Always

1. When starting a new job, always branch from `origin/main`. Fetch first. Do not start from the branch this checkout happens to be on, from another feature branch, or from an open PR.
2. Run `git worktree list` first. Each listed path is already a live checkout.
3. Put new worktrees **outside** this repo root. Never create them under `/workspace` or inside the project folder.
4. Give each worktree its own branch. Git refuses to check out the same branch in two worktrees.
5. Run installs, builds, tests, commits, and pushes **inside that worktree's path**.
6. After creating a worktree, run the setup from `.cursor/worktrees.json` if Cursor did not already (on this repo that is `npm ci`).
7. When the work is finished or abandoned, remove the worktree with `git worktree remove`, not `rm`.

## Never

1. Never `git checkout` a branch another worktree already has. Create or use a different branch instead.
2. Never symlink `node_modules/` or `.pio/` from the main checkout into a worktree. That can break both trees.
3. Never run two Playwright or demo servers on port 4173. One worktree at a time, or give the second a different port.
4. Never delete a worktree folder by hand and leave it registered. Use `git worktree remove` or `git worktree prune`.
5. Never commit a worktree checkout. The extra folder is local only.
6. Never force-push `main` from a worktree.
7. Never start a new job from another feature branch or open PR unless the user explicitly asks to stack on that work.

## Cursor UI

On the desktop app, prefer Cursor's own worktrees when they are available:

- Agents Window: start or move the agent into a worktree
- IDE chat: `/worktree` to isolate the rest of the chat
- `/apply-worktree` to bring a result back into the main checkout
- `/delete-worktree` when that isolated checkout is done
- `/best-of-n` to try the same task in several worktrees

Cursor runs `.cursor/worktrees.json` when it creates the folder. Debug setup from the editor Output panel, channel **Worktrees Setup**.

In a Cloud Agent VM the UI may not create the folder. Use the git commands below in that case.

## Commands

List what is already checked out:

```text
git worktree list
```

New job, from `main`:

```text
git fetch origin main
git worktree add -b cursor/SHORT-NAME ../ArcheryShotClock-SHORT-NAME origin/main
cd ../ArcheryShotClock-SHORT-NAME
npm ci
```

On this Cloud Agent VM, use `/tmp/worktrees/SHORT-NAME` instead of a sibling of `/workspace`:

```text
mkdir -p /tmp/worktrees
git fetch origin main
git worktree add -b cursor/SHORT-NAME /tmp/worktrees/SHORT-NAME origin/main
cd /tmp/worktrees/SHORT-NAME
npm ci
```

Stack on an existing PR only when the user asks for that:

```text
git fetch origin BRANCH
git worktree add -b cursor/SHORT-NAME /tmp/worktrees/SHORT-NAME origin/BRANCH
```

Follow the branch-name rules from the current agent run and from [github-repo](../github-repo/SKILL.md). Do not invent a second naming scheme.

Commit and push from inside the worktree, same as any other branch. Then create or update that branch's PR with `ManagePullRequest`. Set `branch_name` to the worktree's branch. For a new job, set `base_branch` to `main`.

Remove a worktree you are done with:

```text
git worktree remove ../ArcheryShotClock-SHORT-NAME
git worktree prune
```

If the folder is already gone:

```text
git worktree prune
```

## This repository

Each worktree needs its own install. Do not reuse another tree's build dirs.

| Need | Command in that worktree |
|---|---|
| Browser tests | `npm ci` then `npx playwright install chromium` if Chromium is missing |
| Firmware / native tests | `pio test -e native` (PlatformIO downloads into that tree's `.pio/`) |
| Demo server | `python3 -m http.server 4173 --directory software/web` — change the port if 4173 is taken |
| WASM rebuild | `./software/tools/build_wasm.sh` then `./software/tools/sync_pages.sh` |

`.pio/`, `node_modules/`, `playwright-report/`, `test-results/`, and `src/web_page.h` stay untracked. Same rule as [github-repo](../github-repo/SKILL.md).

Playwright's config uses `http://127.0.0.1:4173` and `reuseExistingServer`. Two worktrees running `npm test` at once will steal each other's server. Run one suite at a time, or temporarily point the second tree at another port.

## If something fails

- `fatal: 'BRANCH' is already used by worktree at ...` — that branch is checked out elsewhere. Use `git worktree list`, then create a new branch or work in the existing path.
- `npm` or `pio` missing in the new folder — you ran the command from the wrong tree, or setup did not run. `cd` into the worktree and run `npm ci`.
- Tests talking to the wrong clock — another worktree is already serving port 4173. Stop that server or pick another port.
- Cursor created a worktree but `node_modules` is empty — run the commands in `.cursor/worktrees.json` yourself, then check the **Worktrees Setup** output.
