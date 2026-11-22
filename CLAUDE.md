# Claude Code instructions

Follow `AGENTS.md` as the repository-wide engineering policy. At the start of every fresh session, read these files in order:

1. `AGENTS.md`
2. `README.md`
3. `NEXT.md`
4. all of `plan.md`
5. the source and tests relevant to the next task

Implement only the fixed 1.0 scope and the bounded slice in `NEXT.md`. Items under **Post-1.0 roadmap** are not current work unless the user explicitly promotes one and the plan is updated first. Keep the project buildable, use independent asset-free fixtures, run Release build and CTest, and update `NEXT.md` with evidence before ending a work session.

For Anthropic-hosted sessions, follow `docs/claude-code-cloud.md`. Retail game archives and extracted data are optional, untrusted, read-only research inputs. Keep them outside the repository, never commit or upload them, and do not execute downloaded game binaries. The normal build and tests must succeed without them.
