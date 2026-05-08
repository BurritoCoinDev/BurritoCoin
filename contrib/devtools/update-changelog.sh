#!/usr/bin/env bash
# Regenerate CHANGELOG.md from `git log`.
#
# This is part of the project's recoverable handoff: CHANGELOG.md
# captures every commit's full message body so contributors who don't
# have a git client (or who lost their working copy) can still
# reconstruct the work history by reading the file on GitHub.
#
# Usage:
#   ./contrib/devtools/update-changelog.sh
#   git add CHANGELOG.md
#   git commit -m "Update CHANGELOG.md"
#
# Or wire as a pre-commit hook (see contrib/devtools/install-hooks.sh)
# so every commit auto-refreshes the file.

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

python3 <<'PYEOF'
import subprocess
import os

log = subprocess.check_output([
    'git', 'log',
    '--pretty=format:===COMMIT===%n%H%n%ad%n%an%n%s%n---BODY---%n%b%n---END---',
    '--date=iso',
    '--since=2026-01-01', '--since=2026-01-01', '--reverse',
]).decode()

commits = []
current = None
state = None
for line in log.split('\n'):
    if line == '===COMMIT===':
        if current:
            commits.append(current)
        current = {'hash': '', 'date': '', 'author': '', 'subject': '', 'body': []}
        state = 'hash'
    elif state == 'hash':
        current['hash'] = line.strip(); state = 'date'
    elif state == 'date':
        current['date'] = line.strip(); state = 'author'
    elif state == 'author':
        current['author'] = line.strip(); state = 'subject'
    elif state == 'subject':
        current['subject'] = line.strip(); state = 'body_start'
    elif line == '---BODY---':
        state = 'body'
    elif line == '---END---':
        state = None
    elif state == 'body':
        current['body'].append(line)

if current:
    commits.append(current)

lines = []
lines.append('# CHANGELOG')
lines.append('')
lines.append('Auto-generated from `git log`. Regenerate with `./contrib/devtools/update-changelog.sh`.')
lines.append('')
lines.append("Each entry contains the full commit message body verbatim. This file is part")
lines.append("of the project's recoverable handoff (along with `HANDOFF.md`) so any future")
lines.append("contributor or session can reconstruct the work history without access to a")
lines.append("git client. Newest commits at the top.")
lines.append('')
lines.append('---')

for c in reversed(commits):
    body = '\n'.join(c['body']).strip()
    short = c['hash'][:7]
    lines.append('')
    lines.append(f"## `{short}` — {c['subject']}")
    lines.append('')
    lines.append(f"**Date:** {c['date']}  ")
    lines.append(f"**Author:** {c['author']}  ")
    lines.append(f"**Full hash:** `{c['hash']}`")
    if body:
        lines.append('')
        lines.append(body)

with open('CHANGELOG.md', 'w') as f:
    f.write('\n'.join(lines) + '\n')

size = os.path.getsize('CHANGELOG.md')
print(f"Wrote CHANGELOG.md ({len(commits)} commits, {size:,} bytes)")
PYEOF
