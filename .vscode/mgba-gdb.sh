#!/bin/bash
set -e

MGBA_EXECUTABLE=${MGBA_EXECUTABLE:-/usr/bin/mgba-qt}
export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-xcb}

LOGFILE="/tmp/mgba-gdb-wrapper.log"
ROM_FILE="$1"

echo "Starting mGBA with ROM: $ROM_FILE" > "$LOGFILE"
echo "mGBA executable: $MGBA_EXECUTABLE" >> "$LOGFILE"

pkill -f mgba-qt || true
sleep 1

"$MGBA_EXECUTABLE" "$ROM_FILE" -g >> "$LOGFILE" 2>&1 &
MGBA_PID=$!

echo "mGBA PID: $MGBA_PID" >> "$LOGFILE"

for i in $(seq 1 30); do
    echo "Checking for port 2345... attempt $i" >> "$LOGFILE"
    
    if ss -ltn | grep -q ':2345'; then
        echo "Port 2345 is now listening!" >> "$LOGFILE"
        echo "started-mgba-server"
        wait $MGBA_PID
        exit 0
    fi

    if ! kill -0 "$MGBA_PID" 2>/dev/null; then
        echo "mGBA process exited unexpectedly!" >> "$LOGFILE"
        echo "mgba process exited unexpectedly. See $LOGFILE" >&2
        cat "$LOGFILE" >&2
        exit 1
    fi

    sleep 0.5
done

echo "Timeout waiting for mGBA gdb server on port 2345" >> "$LOGFILE"
echo "timeout waiting for mGBA gdb server on port 2345. See $LOGFILE" >&2
cat "$LOGFILE" >&2
exit 1
