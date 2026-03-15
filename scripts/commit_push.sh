#!/bin/bash
#
# Commit staged changes and push to GitHub.
# Pre-commit hook runs build+test automatically.
#
# Usage: ./scripts/commit_push.sh "commit message"

set -e

cd "$(dirname "$0")/.."

if [ -z "$1" ]; then
    echo "Usage: commit_push.sh \"commit message\""
    exit 1
fi

git add -A
git commit -m "$(cat <<EOF
$1

Co-Authored-By: Claude Opus 4.6 (1M context) <noreply@anthropic.com>
EOF
)"
git push
