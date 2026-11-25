# Issue tracker

This repo tracks issues in **GitHub Issues** on [M0WUT/p0004-sw-002_claude-firmware](https://github.com/M0WUT/p0004-sw-002_claude-firmware/issues). (Until 2026-11-25 it used local Markdown files in `issues/`; that history is in git.)

Agents use the REST API through [`tools/wayfinder_gh.py`](../tools/wayfinder_gh.py), since the `gh` CLI isn't installed on the dev VM. The token comes from `$GITHUB_TOKEN`, or from `~/.config/gte-wayfinder/github_token` (mode 600). **Never commit it or print it.** A fine-grained token needs Issues read/write (plus Contents read/write to push).

## Wayfinding operations

- **Map**: an issue labelled `wayfinder:map`. The current map is [#1 GTE Core firmware](https://github.com/M0WUT/p0004-sw-002_claude-firmware/issues/1).
- **Tickets**: GitHub **sub-issues** of the map, each labelled with exactly one of `wayfinder:research`, `wayfinder:prototype`, `wayfinder:grilling` or `wayfinder:task`. No other labels.
- **Blocking**: GitHub's native issue dependencies ("blocked by"). A ticket is unblocked when every issue blocking it is closed.
- **Claim**: assign the ticket to the dev driving the map before any work: `tools/wayfinder_gh.py claim N`.
- **Frontier**: open, unblocked, unassigned sub-issues of the map: `tools/wayfinder_gh.py frontier 1` (add `--all` to see blocked and claimed tickets with the reason).
- **Resolve**: post the answer as a resolution comment, then close as completed: `tools/wayfinder_gh.py resolve N answer.md`. Then edit the map's *Decisions so far* to add `- [<title>](<issue URL>): <gist>`.
- **New tickets**: `tools/wayfinder_gh.py create "<title>" <type> body.md --blocked-by N,N` creates the issue, adds it to the map and wires the blockers in one go. When creating several tickets that block each other, create them all first, then wire them with `block N BLOCKER`.
- **Research findings** go on a `research/<slug>` branch, which is never merged. The ticket's resolution links to it.
- **Map edits** (Destination, Notes, Not yet specified, Out of scope) are edits to the map issue's body.
