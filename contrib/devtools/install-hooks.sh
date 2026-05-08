#!/usr/bin/env bash
# Install local git hooks for this checkout.
#
# Currently installs:
#   - post-commit hook that regenerates CHANGELOG.md so each new commit
#     keeps the rolling commit-log file in sync with reality. The hook
#     uses --no-verify on the follow-up commit so it doesn't recurse.
#
# Run once per fresh clone:
#   ./contrib/devtools/install-hooks.sh
#
# Skip the auto-update by setting SKIP_CHANGELOG=1 before any git commit.

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel)"
HOOK="${REPO_ROOT}/.git/hooks/post-commit"

cat > "$HOOK" <<'EOF'
#!/usr/bin/env bash
# Auto-regenerate CHANGELOG.md after every commit.
# Skip with SKIP_CHANGELOG=1 git commit ...
[ "${SKIP_CHANGELOG:-0}" = "1" ] && exit 0

# Avoid recursion: if HEAD's commit subject is the auto-update one, skip.
if git log -1 --pretty=%s | grep -q '^Auto-update CHANGELOG\.md$'; then
    exit 0
fi

REPO_ROOT="$(git rev-parse --show-toplevel)"
cd "$REPO_ROOT"

# Regenerate
./contrib/devtools/update-changelog.sh > /dev/null

# Stage + commit only if there's a real diff
if ! git diff --quiet CHANGELOG.md; then
    git add CHANGELOG.md
    SKIP_CHANGELOG=1 git commit -m "Auto-update CHANGELOG.md" --no-verify > /dev/null
fi
EOF

chmod +x "$HOOK"

echo "Installed: $HOOK"
echo ""
echo "From now on, every git commit will be followed by a second commit that"
echo "refreshes CHANGELOG.md. Skip with: SKIP_CHANGELOG=1 git commit ..."
