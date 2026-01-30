#!/bin/bash
# End-to-end test script for RunawayGuard
# Tests daemon startup, socket communication, and basic functionality

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
DAEMON_BIN="${PROJECT_DIR}/target/release/runaway-daemon"
SOCKET_PATH="/run/user/$(id -u)/runaway-guard.sock"
TIMEOUT=5

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

cleanup() {
    log_info "Cleaning up..."
    if [ -n "$DAEMON_PID" ]; then
        kill "$DAEMON_PID" 2>/dev/null || true
        wait "$DAEMON_PID" 2>/dev/null || true
    fi
    rm -f "$SOCKET_PATH"
}

trap cleanup EXIT

# Check if daemon binary exists
if [ ! -f "$DAEMON_BIN" ]; then
    log_warn "Daemon binary not found at $DAEMON_BIN"
    log_info "Building daemon in release mode..."
    cd "$PROJECT_DIR/daemon"
    cargo build --release
fi

# Start the daemon
log_info "Starting daemon..."
"$DAEMON_BIN" &
DAEMON_PID=$!
sleep 1

# Check if daemon is running
if ! kill -0 "$DAEMON_PID" 2>/dev/null; then
    log_error "Daemon failed to start"
    exit 1
fi
log_info "Daemon started with PID $DAEMON_PID"

# Wait for socket to be created
log_info "Waiting for socket..."
for i in $(seq 1 $TIMEOUT); do
    if [ -S "$SOCKET_PATH" ]; then
        log_info "Socket created at $SOCKET_PATH"
        break
    fi
    sleep 1
done

if [ ! -S "$SOCKET_PATH" ]; then
    log_error "Socket not created within ${TIMEOUT}s"
    exit 1
fi

# Test 1: Ping
log_info "Test 1: Ping command..."
RESPONSE=$(echo '{"cmd":"ping"}' | socat - UNIX-CONNECT:"$SOCKET_PATH" || echo "FAILED")
if echo "$RESPONSE" | grep -q '"type":"pong"'; then
    log_info "Ping test PASSED"
else
    log_error "Ping test FAILED: $RESPONSE"
    exit 1
fi

# Test 2: List processes
log_info "Test 2: List processes..."
RESPONSE=$(echo '{"cmd":"list_processes"}' | socat - UNIX-CONNECT:"$SOCKET_PATH" || echo "FAILED")
if echo "$RESPONSE" | grep -q '"type":"response"'; then
    PROC_COUNT=$(echo "$RESPONSE" | grep -o '"pid"' | wc -l)
    log_info "List processes test PASSED (found $PROC_COUNT processes)"
else
    log_error "List processes test FAILED: $RESPONSE"
    exit 1
fi

# Test 3: Get alerts (should be empty initially)
log_info "Test 3: Get alerts..."
RESPONSE=$(echo '{"cmd":"get_alerts","params":{"limit":10}}' | socat - UNIX-CONNECT:"$SOCKET_PATH" || echo "FAILED")
if echo "$RESPONSE" | grep -q '"type":"response"'; then
    log_info "Get alerts test PASSED"
else
    log_error "Get alerts test FAILED: $RESPONSE"
    exit 1
fi

# Test 4: Add whitelist
log_info "Test 4: Add whitelist..."
RESPONSE=$(echo '{"cmd":"add_whitelist","params":{"pattern":"test_process","match_type":"name"}}' | socat - UNIX-CONNECT:"$SOCKET_PATH" || echo "FAILED")
if echo "$RESPONSE" | grep -q '"success":true'; then
    log_info "Add whitelist test PASSED"
else
    log_error "Add whitelist test FAILED: $RESPONSE"
    exit 1
fi

# Test 5: Kill process (send SIGCONT to self - harmless)
log_info "Test 5: Kill process (SIGCONT to self)..."
RESPONSE=$(echo "{\"cmd\":\"kill_process\",\"params\":{\"pid\":$$,\"signal\":\"SIGCONT\"}}" | socat - UNIX-CONNECT:"$SOCKET_PATH" || echo "FAILED")
if echo "$RESPONSE" | grep -q '"success":true'; then
    log_info "Kill process test PASSED"
else
    log_error "Kill process test FAILED: $RESPONSE"
    exit 1
fi

log_info "All E2E tests PASSED!"
exit 0
