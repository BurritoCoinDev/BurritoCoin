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

# Use ASCII control characters as delimiters. RS (0x1e) separates commit
# records and FS (0x1f) separates fields within a record. These bytes
# never appear in human-authored commit messages, so — unlike text
# markers such as "===COMMIT===" — they cannot collide with commit body
# content (which in this repo includes markdown "---" rules and code).
RS = '\x1e'
FS = '\x1f'

fmt = '%H' + FS + '%ad' + FS + '%an' + FS + '%s' + FS + '%b' + RS
log = subprocess.check_output([
    'git', 'log',
    '--pretty=format:' + fmt,
    '--date=iso',
    '--since=2026-01-01',
    '--reverse',
]).decode()

commits = []
for record in log.split(RS):
    # git joins records with a newline; strip the leading/trailing ones.
    record = record.strip('\n')
    if not record.strip():
        continue
    parts = record.split(FS)
    if len(parts) < 5:
        continue
    commits.append({
        'hash': parts[0].strip(),
        'date': parts[1].strip(),
        'author': parts[2].strip(),
        'subject': parts[3].strip(),
        # Re-join in the unlikely event a body contained a stray FS.
        'body': [FS.join(parts[4:]).strip('\n')],
    })

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
