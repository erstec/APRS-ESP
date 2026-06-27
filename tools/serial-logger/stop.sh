#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
PID_FILE=".pid"

if [ ! -f "$PID_FILE" ]; then
    echo "Not running (no .pid file)"
    exit 1
fi

PID=$(cat "$PID_FILE")
if kill -0 "$PID" 2>/dev/null; then
    kill -INT "$PID"
    rm "$PID_FILE"
    echo "Stopped (PID $PID)"
else
    echo "Not running (stale .pid)"
    rm "$PID_FILE"
fi
