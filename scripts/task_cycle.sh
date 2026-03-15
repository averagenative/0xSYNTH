#!/bin/bash
#
# Full autonomous worker cycle:
#   1. Show next task
#   2. Claim it
#   3. Build + test (after you've implemented)
#   4. Complete it
#   5. Update openspec tasks.md checkbox
#
# Usage:
#   ./scripts/task_cycle.sh next          # show next task
#   ./scripts/task_cycle.sh claim ENG-006 # claim a task
#   ./scripts/task_cycle.sh done ENG-006  # build, test, complete, commit
#   ./scripts/task_cycle.sh status        # show progress

set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
TASKS="$PROJECT_DIR/tasks/task_ops.sh"
BUILD="$PROJECT_DIR/scripts/build_test.sh"

case "${1:-help}" in
    next)
        $TASKS next "${2:-}"
        ;;
    claim)
        $TASKS claim "${2:?need task id}"
        ;;
    done)
        task_id="${2:?need task id}"
        echo "=== Building and testing ==="
        bash "$BUILD"
        echo ""
        echo "=== Tests passed — completing $task_id ==="
        $TASKS complete "$task_id"
        echo ""
        echo "Task $task_id done. Remember to:"
        echo "  1. Update openspec tasks.md checkbox"
        echo "  2. git add -A && git commit && git push"
        ;;
    status)
        $TASKS status
        echo ""
        echo "=== OpenSpec Progress ==="
        cd "$PROJECT_DIR"
        grep -c '\- \[x\]' openspec/changes/synth-engine-extraction/tasks.md 2>/dev/null || echo "0"
        total=$(grep -c '\- \[' openspec/changes/synth-engine-extraction/tasks.md 2>/dev/null || echo "0")
        done=$(grep -c '\- \[x\]' openspec/changes/synth-engine-extraction/tasks.md 2>/dev/null || echo "0")
        echo "  OpenSpec: $done/$total tasks complete"
        ;;
    help|*)
        echo "Usage: task_cycle.sh <next|claim|done|status> [task-id]"
        ;;
esac
