---
name: github-issues
description: Mention and link the GitHub issue in every commit that addresses one. Use when fixing, implementing, or closing a GitHub issue, or when writing a commit message or pull request for issue work.
---

# GitHub issues in commits

Read this skill before you commit work that addresses a GitHub issue.

GitHub only links a commit to an issue if the message contains the issue number in a form it recognises, such as `#6` or `Fixes #6`. A message that describes the change but omits the number leaves the issue unlinked.

## Always

1. Name the issue in the commit message with a GitHub keyword and the number: `Fixes #6`, `Closes #3`, or `Resolves #12`.
2. Put that line in the commit body, after a short subject that says what changed.
3. If the commit addresses more than one issue, mention each number.
4. Repeat the same `Fixes #N` (or `Closes` / `Resolves`) line in the pull request body so merge also closes the issue.
5. Use the issue number from the request, the URL, or `gh issue view`. Do not guess.

## Never

1. Never commit issue work with a message that only describes the change and does not mention `#N`.
2. Never invent an issue number. If the work is not for a GitHub issue, omit the keyword.
3. Never use a full GitHub URL in place of `#N`. The number is what GitHub indexes.
4. Never write `issue 6` or `issue-6` without `#`. That does not create a link.

## Commit shape

```text
Add qualification structure: ends per round and total rounds.

Fixes #6.
```

Several issues:

```text
Deduct Technical Control from the round break and add make-up ends.

Fixes #7. Fixes #5.
```

Subject-only is enough only when the number is already there:

```text
Fixes #1: add Restart session.
```

## Keywords GitHub accepts

Use one of: `Fixes`, `Closes`, `Resolves` (and the same words with `Fix`, `Close`, `Resolve`).

Follow it with `#` and the issue number. That both links the commit on the issue and closes the issue when the commit reaches `main`.
