#!/bin/bash
set -e
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT/backend"
make
./heritage serve data/ndli_sample.csv 5000 &
BACKEND_PID=$!
cd "$ROOT/frontend"
python3 -m http.server 8000 &
FRONTEND_PID=$!
cleanup(){ kill "$BACKEND_PID" "$FRONTEND_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM
sleep 2
open "http://localhost:8000" 2>/dev/null || true
echo "Reading Room is running. Press Ctrl+C to stop both servers."
wait
