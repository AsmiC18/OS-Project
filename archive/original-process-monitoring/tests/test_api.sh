
#!/bin/sh
# HTTP API tests for AegisOS.
set -u

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SERVER="$ROOT/build/tests/run_api_server"
PORT=18080
TMP_DIR=$(mktemp -d) || exit 1
SERVER_PID=

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT HUP INT TERM

fail() {
    echo "FAIL: $1"
    exit 1
}

if [ ! -x "$SERVER" ]; then
    fail "API server not found. Run: make build/tests/run_api_server"
fi

test_route() {
    method=$1
    path=$2
    expected_status=$3
    expected_body=$4
    response="$TMP_DIR/response"
    status=
    attempt=0

    "$SERVER" >"$TMP_DIR/server.log" 2>&1 &
    SERVER_PID=$!

    # Wait until the server has started without sending a request.
    while [ "$attempt" -lt 30 ]; do
        if ! kill -0 "$SERVER_PID" 2>/dev/null; then
            wait "$SERVER_PID" 2>/dev/null || true
            SERVER_PID=
            cat "$TMP_DIR/server.log"
            fail "Server exited before $method $path"
        fi

        if grep -q "API server listening" "$TMP_DIR/server.log"; then
            break
        fi

        sleep 0.1
        attempt=$((attempt + 1))
    done

    status=$(curl --silent --show-error --max-time 3 \
        --request "$method" \
        --output "$response" \
        --write-out '%{http_code}' \
        "http://127.0.0.1:$PORT$path") || {
            wait "$SERVER_PID" 2>/dev/null || true
            SERVER_PID=
            cat "$TMP_DIR/server.log"
            fail "Request failed: $method $path"
        }

    if ! wait "$SERVER_PID"; then
        SERVER_PID=
        cat "$TMP_DIR/server.log"
        fail "Server failed handling $method $path"
    fi
    SERVER_PID=

    if [ "$status" != "$expected_status" ]; then
        cat "$response"
        fail "$method $path returned HTTP $status; expected $expected_status"
    fi

    if ! grep -Fq "$expected_body" "$response"; then
        cat "$response"
        fail "Unexpected response body for $method $path"
    fi

    echo "PASS: $method $path returns HTTP $expected_status"
}

test_route GET /health 200 '"status":"ok"'
test_route GET /unknown 404 '"error":"route not found"'
test_route POST /health 405 '"error":"method not allowed"'

echo "API tests: 0 failures"
