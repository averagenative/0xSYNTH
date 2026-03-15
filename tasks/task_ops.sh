#!/bin/bash
#
# 0xSYNTH Task Operations
# File-based task coordination for autonomous workers.
#
# Usage:
#   ./tasks/task_ops.sh list [agent]         — show pending tasks
#   ./tasks/task_ops.sh next [agent]         — get next unclaimed task
#   ./tasks/task_ops.sh claim <task-id>      — claim a task (mark in-progress)
#   ./tasks/task_ops.sh complete <task-id>   — mark task done, archive it
#   ./tasks/task_ops.sh add <agent> <title>  — add a new task manually
#   ./tasks/task_ops.sh status               — summary across all agents
#   ./tasks/task_ops.sh sync                 — sync task files with openspec tasks.md

set -euo pipefail
TASKS_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$TASKS_DIR")"

# Find which task file contains a given task ID
find_task_file() {
    local task_id="$1"
    grep -rl "\"$task_id\"" "$TASKS_DIR"/*_tasks.json 2>/dev/null | head -1
}

case "${1:-help}" in
    list)
        agent="${2:-}"
        if [ -n "$agent" ]; then
            files=("$TASKS_DIR/${agent}_tasks.json")
        else
            files=("$TASKS_DIR"/*_tasks.json)
        fi
        for f in "${files[@]}"; do
            [ -f "$f" ] || continue
            agent_name=$(python3 -c "import json; print(json.load(open('$f'))['agent'])")
            echo "=== $agent_name ==="
            python3 -c "
import json
with open('$f') as fh:
    data = json.load(fh)
for t in data['tasks']:
    status = t['status']
    icon = '✓' if status == 'done' else '→' if status == 'in-progress' else ' '
    print(f'  [{icon}] {t[\"id\"]} [{t[\"priority\"]}] {t[\"title\"]}')
"
            echo
        done
        ;;

    next)
        agent="${2:-}"
        if [ -n "$agent" ]; then
            files=("$TASKS_DIR/${agent}_tasks.json")
        else
            files=("$TASKS_DIR"/*_tasks.json)
        fi
        for f in "${files[@]}"; do
            [ -f "$f" ] || continue
            result=$(python3 -c "
import json
with open('$f') as fh:
    data = json.load(fh)
for t in data['tasks']:
    if t['status'] == 'pending':
        print(json.dumps(t, indent=2))
        break
" 2>/dev/null)
            if [ -n "$result" ]; then
                echo "$result"
                exit 0
            fi
        done
        echo "No pending tasks found."
        ;;

    claim)
        task_id="${2:?Usage: task_ops.sh claim <task-id>}"
        file=$(find_task_file "$task_id")
        [ -z "$file" ] && echo "Task $task_id not found" && exit 1
        python3 -c "
import json, datetime
with open('$file') as fh:
    data = json.load(fh)
for t in data['tasks']:
    if t['id'] == '$task_id':
        t['status'] = 'in-progress'
        t['claimed_at'] = datetime.datetime.now().isoformat()
        t['claimed_by'] = 'claude'
        break
with open('$file', 'w') as fh:
    json.dump(data, fh, indent=2)
"
        echo "Claimed: $task_id"
        ;;

    complete)
        task_id="${2:?Usage: task_ops.sh complete <task-id>}"
        file=$(find_task_file "$task_id")
        [ -z "$file" ] && echo "Task $task_id not found" && exit 1
        python3 -c "
import json, datetime
with open('$file') as fh:
    data = json.load(fh)
for t in data['tasks']:
    if t['id'] == '$task_id':
        t['status'] = 'done'
        t['completed_at'] = datetime.datetime.now().isoformat()
        break
with open('$file', 'w') as fh:
    json.dump(data, fh, indent=2)
"
        echo "Completed: $task_id"
        ;;

    add)
        agent="${2:?Usage: task_ops.sh add <agent> <title>}"
        title="${3:?Usage: task_ops.sh add <agent> <title>}"
        file="$TASKS_DIR/${agent}_tasks.json"
        [ ! -f "$file" ] && echo "No task file for agent: $agent" && exit 1
        python3 -c "
import json
with open('$file') as fh:
    data = json.load(fh)
# Generate next ID
prefix = data['agent'].upper()[:3]
existing = [t['id'] for t in data['tasks']]
n = 1
while f'{prefix}-{n:03d}' in existing:
    n += 1
new_id = f'{prefix}-{n:03d}'
data['tasks'].append({
    'id': new_id,
    'openspec_ref': None,
    'title': '$title',
    'status': 'pending',
    'priority': 'P1',
    'phase': 0,
    'files': [],
    'tests': [],
    'claimed_by': None,
    'claimed_at': None,
    'completed_at': None
})
with open('$file', 'w') as fh:
    json.dump(data, fh, indent=2)
print(f'Added: {new_id} — $title')
"
        ;;

    status)
        echo "0xSYNTH Task Status"
        echo "==================="
        total=0; done=0; progress=0; pending=0
        for f in "$TASKS_DIR"/*_tasks.json; do
            [ -f "$f" ] || continue
            python3 -c "
import json
with open('$f') as fh:
    data = json.load(fh)
t = len(data['tasks'])
d = sum(1 for x in data['tasks'] if x['status'] == 'done')
p = sum(1 for x in data['tasks'] if x['status'] == 'in-progress')
q = sum(1 for x in data['tasks'] if x['status'] == 'pending')
print(f'  {data[\"agent\"]:12s}  {d}/{t} done, {p} in-progress, {q} pending')
print(f'TOTALS:{t}:{d}:{p}:{q}')
" | while read line; do
                if [[ "$line" == TOTALS:* ]]; then
                    IFS=: read _ t d p q <<< "$line"
                    total=$((total + t))
                    done=$((done + d))
                    progress=$((progress + p))
                    pending=$((pending + q))
                else
                    echo "$line"
                fi
            done
        done
        echo
        echo "OpenSpec: $(cd "$PROJECT_DIR" && openspec list 2>/dev/null | grep -c 'synth' || echo '?') active changes"
        ;;

    help|*)
        echo "Usage: task_ops.sh <command> [args]"
        echo ""
        echo "Commands:"
        echo "  list [agent]        — show tasks (all or for one agent)"
        echo "  next [agent]        — get next unclaimed task"
        echo "  claim <task-id>     — mark task in-progress"
        echo "  complete <task-id>  — mark task done"
        echo "  add <agent> <title> — add a new task"
        echo "  status              — summary across all agents"
        ;;
esac
