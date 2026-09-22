# Contribution Policy — Soldier0x0/FinceptTerminal (personal fork)

This file is the **contribution policy for this fork** — not for upstream
[Fincept-Corporation/FinceptTerminal](https://github.com/Fincept-Corporation/FinceptTerminal).

This repository is a **personal fork** focused on a free, local, India-oriented
investment terminal (local-only mode, Ollama default, no Fincept cloud login).
Changes here are **not** intended for upstream merge unless you open a separate
PR against the original project and follow *their* policy.

Looking for **how to build, where code lives, code conventions?** →
[`docs/CONTRIBUTING.md`](../docs/CONTRIBUTING.md).

---

## TL;DR (this fork)

1. **No upstream issue-label gate.** You do not need `good-first-issue`,
   `help-wanted`, or `scope:approved` from Fincept's repo to work here.
2. **One logical change per PR** is still appreciated — keeps review and CI fast.
3. **Build before you push.** `build-cpp.yml` / PR Build must pass.
4. **Use a topic branch** (e.g. `cursor/local-only-mode-spec-5a93`), not `main`.

---

## What is different from upstream?

| Upstream rule | This fork |
|---|---|
| Must link a labeled, scope-approved issue | **Not required** — open issues/PRs freely for fork goals |
| PRs closed for docs-only edits without maintainer ask | **Allowed** when they support local-only / fork docs |
| Hacktoberfest exclusion | **Not enforced** — this is personal maintenance, not a public contribution funnel |
| Contributor list / credits curation | **Not applicable** — no upstream credits chase |
| `support@fincept.in` / Fincept Discord as primary help | Use this repo's **Issues** for fork-specific questions |

Upstream policy (for reference only):
[Fincept-Corporation/FinceptTerminal `.github/CONTRIBUTING.md`](https://github.com/Fincept-Corporation/FinceptTerminal/blob/main/.github/CONTRIBUTING.md)

---

## Still expected

### Build and test

The change must compile on your target platform before submission. CI must pass.
If you could not build locally, say so in the PR body.

### Minimum scope

One feature or one bug fix per PR when possible. Do not bundle unrelated changes.

### No auto-formatter churn

Do not run formatters over files you did not meaningfully change. Match surrounding
style. Large reformat-only diffs may be rejected.

### Describe the change

The PR description should explain the user-visible effect and reasoning.
"Small improvements" / "fix bug" alone is not enough.

### Topic branches

Never open a PR from this fork's `main` if you plan to keep iterating — use a
named branch and rebase as needed.

---

## Fork focus areas

Work that fits this fork (not an exhaustive list):

- **Local-only mode** — gate cloud/upsell features, keep upstream code mergeable
- **India defaults** — MFs, gold, bonds/FDs, NSE/BSE data paths
- **Local LLM** — Ollama-first, optional Groq; no Fincept LLM dependency
- **Docs** — `docs/LOCAL_ONLY_MODE.md`, fork README notice, this file

If you want a change in **upstream** Fincept Terminal, contribute there separately
after reading their policy.

---

## Questions?

- [GitHub Issues](https://github.com/Soldier0x0/FinceptTerminal/issues) on this fork
- Local-only design: [`docs/LOCAL_ONLY_MODE.md`](../docs/LOCAL_ONLY_MODE.md)
