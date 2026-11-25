#!/usr/bin/env python3
"""Wayfinder operations on this repo's GitHub Issues (see docs/issue-tracker.md).

Token: $GITHUB_TOKEN, else ~/.config/gte-wayfinder/github_token (never committed, never printed).

  wayfinder_gh.py frontier [MAP] [--all]       open sub-issues of MAP (default 1) that are unblocked and unclaimed
  wayfinder_gh.py claim N [LOGIN]              assign ticket N (default login: M0WUT)
  wayfinder_gh.py resolve N FILE               post FILE as the resolution comment, then close N as completed
  wayfinder_gh.py create TITLE TYPE FILE [--map MAP] [--blocked-by N,N]
                                               new ticket (TYPE = research|prototype|grilling|task) with body FILE,
                                               added as a sub-issue of MAP and wired to its blockers
  wayfinder_gh.py block N BLOCKER              mark N as blocked by BLOCKER
"""
import json
import os
import sys
import urllib.error
import urllib.request
from pathlib import Path

REPO = 'M0WUT/p0004-sw-002_claude-firmware'
API = f'https://api.github.com/repos/{REPO}'


def token():
    t = os.environ.get('GITHUB_TOKEN')
    if not t:
        p = Path('~/.config/gte-wayfinder/github_token').expanduser()
        if not p.exists():
            sys.exit(f'no token: set GITHUB_TOKEN or create {p} (chmod 600)')
        t = p.read_text().strip()
    return t


def call(method, path, data=None):
    req = urllib.request.Request(API + path, data=json.dumps(data).encode() if data is not None else None, method=method,
                                 headers={'Authorization': f'Bearer {token()}', 'Accept': 'application/vnd.github+json',
                                          'X-GitHub-Api-Version': '2022-11-28'})
    try:
        with urllib.request.urlopen(req) as r:
            body = r.read()
            return json.loads(body) if body else None
    except urllib.error.HTTPError as e:
        sys.exit(f'{method} {path} -> {e.code}: {e.read().decode(errors="replace")[:300]}')


def issue(n):
    return call('GET', f'/issues/{n}')


def frontier(map_n=1, show_all=False):
    for t in sorted(call('GET', f'/issues/{map_n}/sub_issues?per_page=100'), key=lambda x: x['number']):
        if t['state'] != 'open':
            continue
        blockers = [b for b in call('GET', f"/issues/{t['number']}/dependencies/blocked_by?per_page=100") if b['state'] == 'open']
        labels = ','.join(l['name'] for l in t['labels'])
        reason = ''
        if blockers:
            reason = 'blocked by ' + ', '.join(f"{b['title']} (#{b['number']})" for b in blockers)
        elif t.get('assignees'):
            reason = 'claimed by ' + ', '.join(a['login'] for a in t['assignees'])
        if reason and not show_all:
            continue
        print(f"#{t['number']:<4} {labels:<22} {t['title']}" + (f'  [{reason}]' if reason else ''))


def main(argv):
    if not argv:
        sys.exit(__doc__)
    cmd, args = argv[0], argv[1:]
    if cmd == 'frontier':
        nums = [a for a in args if a.isdigit()]
        frontier(int(nums[0]) if nums else 1, '--all' in args)
    elif cmd == 'claim':
        login = args[1] if len(args) > 1 else 'M0WUT'
        call('POST', f'/issues/{args[0]}/assignees', {'assignees': [login]})
        print(f'#{args[0]} claimed by {login}')
    elif cmd == 'resolve':
        call('POST', f'/issues/{args[0]}/comments', {'body': Path(args[1]).read_text()})
        call('PATCH', f'/issues/{args[0]}', {'state': 'closed', 'state_reason': 'completed'})
        print(f'#{args[0]} resolved and closed')
    elif cmd == 'create':
        title, typ, body_file = args[0], args[1], args[2]
        map_n = int(args[args.index('--map') + 1]) if '--map' in args else 1
        blocked = [int(x) for x in args[args.index('--blocked-by') + 1].split(',')] if '--blocked-by' in args else []
        new = call('POST', '/issues', {'title': title, 'body': Path(body_file).read_text(), 'labels': [f'wayfinder:{typ}']})
        call('POST', f'/issues/{map_n}/sub_issues', {'sub_issue_id': new['id']})
        for b in blocked:
            call('POST', f"/issues/{new['number']}/dependencies/blocked_by", {'issue_id': issue(b)['id']})
        print(f"created #{new['number']}: {title}")
    elif cmd == 'block':
        call('POST', f'/issues/{args[0]}/dependencies/blocked_by', {'issue_id': issue(args[1])['id']})
        print(f'#{args[0]} is now blocked by #{args[1]}')
    else:
        sys.exit(__doc__)


if __name__ == '__main__':
    main(sys.argv[1:])
