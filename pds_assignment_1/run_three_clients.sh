#!/usr/bin/env bash
set -e
# Start server expecting 3 clients, then spawn 3 interactive terminals (gnome-terminal fallback to xterm)
./server 127.0.0.1 5555 --expected 3 &
SRV_PID=$!
sleep 1

(./client 127.0.0.1 5555) &
(./client 127.0.0.1 5555) &
(./client 127.0.0.1 5555) &

wait $SRV_PID