
#!/bin/sh
# Integration tests for the AegisOS executable.

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
APP="$ROOT/build/aegisos"
TMP_DIR=$(mktemp -d) || exit 1

cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT HUP INT TERM

failures=0

# Test --help.
if "$APP" --help >"$TMP_DIR/help.out" 2>&1 &&
   grep -q 'Usage:' "$TMP_DIR/help.out"; then
    echo "PASS: --help prints usage information"
else
    echo "FAIL: --help prints usage information"
    cat "$TMP_DIR/help.out"
    failures=$((failures + 1))
fi

# Test --version.
if "$APP" --version >"$TMP_DIR/version.out" 2>&1 &&
   grep -q 'AegisOS 0.1.0' "$TMP_DIR/version.out"; then
    echo "PASS: --version prints the application version"
else
    echo "FAIL: --version prints the application version"
    cat "$TMP_DIR/version.out"
    failures=$((failures + 1))
fi

# Invalid command must return exit code 2.
"$APP" --not-a-command >"$TMP_DIR/invalid.out" 2>&1
rc=$?
if [ "$rc" -eq 2 ]; then
    echo "PASS: invalid command is rejected"
else
    echo "FAIL: invalid command is rejected (exit $rc)"
    failures=$((failures + 1))
fi

# Invalid PID must return exit code 2.
"$APP" --monitor invalid 1 >"$TMP_DIR/pid.out" 2>&1
rc=$?
if [ "$rc" -eq 2 ] &&
   grep -q 'invalid PID' "$TMP_DIR/pid.out"; then
    echo "PASS: invalid PID is rejected"
else
    echo "FAIL: invalid PID is rejected"
    failures=$((failures + 1))
fi

# Zero sample count must return exit code 2.
"$APP" --monitor "$$" 0 >"$TMP_DIR/count.out" 2>&1
rc=$?
if [ "$rc" -eq 2 ]; then
    echo "PASS: zero sample count is rejected"
else
    echo "FAIL: zero sample count is rejected (exit $rc)"
    failures=$((failures + 1))
fi

# Monitor this shell process for one sample.
if "$APP" --monitor "$$" 1 >"$TMP_DIR/monitor.out" 2>&1 &&
   grep -q 'PID=' "$TMP_DIR/monitor.out"; then
    echo "PASS: monitor command samples a running process"
else
    echo "FAIL: monitor command samples a running process"
    cat "$TMP_DIR/monitor.out"
    failures=$((failures + 1))
fi

# A successful child should exit successfully under supervision.
if "$APP" --supervise /bin/sh -c 'exit 0' \
    >"$TMP_DIR/supervisor.out" 2>&1; then
    echo "PASS: supervisor runs a successful child"
else
    echo "FAIL: supervisor runs a successful child"
    cat "$TMP_DIR/supervisor.out"
    failures=$((failures + 1))
fi

echo
echo "Integration tests: $failures failure(s)"
[ "$failures" -eq 0 ]
