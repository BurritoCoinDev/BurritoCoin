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

# In a shallow clone `git log` stops at the shallow boundary, so regenerating
# would silently drop most entries (a depth-50 clone kept 64 of 141). Refuse.
if [ "$(git rev-parse --is-shallow-repository)" = "true" ]; then
    echo "update-changelog.sh: refusing to run in a shallow clone." >&2
    echo "  git log stops at the shallow boundary here, so most entries would be" >&2
    echo "  silently dropped. Run 'git fetch --unshallow' first." >&2
    exit 1
fi

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

# Preserve archived pre-rewrite history. Commits that earlier history
# rewrites made unreachable survive only in this file, so regeneration must
# never drop them: everything from the archive heading onward is carried
# through verbatim.
ARCHIVE_HEADING = '# Archived history (pre-rewrite)'
HASH_NOTE = (
    "> **Note on commit hashes.** On 2026-08-28 this repository's history was\n"
    "> rewritten to drop 44 superseded copies of the prebuilt Windows wallet,\n"
    "> which renumbered every commit. Hashes in the *Archived history* section\n"
    "> below refer to pre-rewrite commits and will not resolve with `git show`.\n"
    "> The commit messages themselves are unchanged and remain the authoritative\n"
    "> record."
)

archive = ''
if os.path.exists('CHANGELOG.md'):
    with open('CHANGELOG.md') as f:
        existing = f.read()
    idx = existing.find(ARCHIVE_HEADING)
    if idx != -1:
        archive = existing[idx:].rstrip('\n')

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
if archive:
    lines.append(HASH_NOTE)
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

out = '\n'.join(lines) + '\n'
if archive:
    out += '\n---\n\n' + archive + '\n'

with open('CHANGELOG.md', 'w') as f:
    f.write(out)

size = os.path.getsize('CHANGELOG.md')
note = ' + archived pre-rewrite history' if archive else ''
print(f"Wrote CHANGELOG.md ({len(commits)} commits{note}, {size:,} bytes)")
PYEOF
