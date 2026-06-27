#!/usr/bin/env bash
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"
PID_FILE=".pid"

if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
    echo "Already running (PID $(cat "$PID_FILE"))"
    exit 1
fi

nohup python3 log-to-file.py > /dev/null 2>&1 &
PID=$!
echo "$PID" > "$PID_FILE"
echo "Started (PID $PID) — logs: $SCRIPT_DIR/esp32_cdc.log"
