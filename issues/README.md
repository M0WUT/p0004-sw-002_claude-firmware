# Issues (local Markdown tracker)

This repo tracks issues as Markdown files in this folder, one per issue, named `NNNN-<slug>.md`. The 4-digit number is the issue id and never gets reused.

## Issue format

Each file starts with YAML frontmatter:

```yaml
---
id: 9
title: The Service contract
labels: [wayfinder:grilling]
status: open          # open | closed
assignee:             # empty = unclaimed
parent: 1             # id of the parent issue (e.g. a wayfinder map), empty if none
blocked_by: [4, 5]    # ids of issues that must be closed first
---
```

The body follows the frontmatter. When an issue is resolved, its answer is appended as a `## Resolution` section, and `status` is set to `closed`.

## Wayfinding operations

- **Map**: the issue labelled `wayfinder:map`. Its tickets are the issues whose `parent` is the map's id.
- **Claim a ticket**: set `assignee:` to the developer's name before doing any work.
- **Blocking**: list the ids of the blocking issues in `blocked_by`. A ticket is unblocked when every issue in that list has `status: closed`.
- **Frontier**: the open, unblocked, unassigned children of the map. To list it:

  ```sh
  python3 issues/frontier.py 1
  ```

- **Resolve**: append a `## Resolution` section, set `status: closed`, and add a line to the map's *Decisions so far*.
- **Research findings** go on a `research/<slug>` branch, which is never merged. The ticket's resolution links the branch and the file path.
