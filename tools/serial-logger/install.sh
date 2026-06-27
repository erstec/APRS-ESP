#!/usr/bin/env bash
set -e
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

pip3 install -r requirements.txt --break-system-packages

echo "Done. Edit SERIAL_PORT in log-to-file.py, then run ./start.sh"
