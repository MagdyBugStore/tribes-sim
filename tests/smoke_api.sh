#!/usr/bin/env bash
# Smoke test for the live HTTP API. Usage: tests/smoke_api.sh [PORT]
# Needs a running `./tribes --serve PORT`, curl and python3 (no other packages).
# Fails if any endpoint returns an error status or invalid JSON.
set -u
PORT="${1:-8080}"
BASE="http://127.0.0.1:${PORT}"
fail=0

check_json() {   # path
    local path="$1" body status
    body="$(mktemp)"
    status="$(curl -s -o "$body" -w '%{http_code}' "${BASE}${path}")"
    if [ "$status" != "200" ]; then
        echo "FAIL  ${path}: HTTP ${status}"; fail=1
    elif ! python3 -m json.tool < "$body" > /dev/null 2>&1; then
        echo "FAIL  ${path}: invalid JSON"; fail=1
    else
        echo "ok    ${path} ($(wc -c < "$body") bytes)"
    fi
    rm -f "$body"
}

for p in /api/state /api/people "/api/people?since=0" /api/person/0 /api/person/1 \
         /api/market /api/herds /api/caravans /api/map /api/schedules /api/dash; do
    check_json "$p"
done

# static files
status="$(curl -s -o /dev/null -w '%{http_code}' "${BASE}/")"
if [ "$status" = "200" ]; then echo "ok    / (viewer page)"; else echo "FAIL  /: HTTP ${status}"; fail=1; fi
for f in viewer.js dashboards.js style.css; do
    status="$(curl -s -o /dev/null -w '%{http_code}' "${BASE}/${f}")"
    if [ "$status" = "200" ]; then echo "ok    /${f}"; else echo "FAIL  /${f}: HTTP ${status}"; fail=1; fi
done

# bad requests must fail cleanly, with a JSON error
status="$(curl -s -o /dev/null -w '%{http_code}' "${BASE}/api/person/99999999")"
if [ "$status" = "404" ]; then echo "ok    unknown person -> 404"; else echo "FAIL  unknown person: HTTP ${status}"; fail=1; fi
status="$(curl -s -o /dev/null -w '%{http_code}' "${BASE}/../etc/passwd")"
if [ "$status" = "403" ] || [ "$status" = "404" ] || [ "$status" = "400" ]; then echo "ok    path escape refused (${status})"; else echo "FAIL  path escape: HTTP ${status}"; fail=1; fi

if [ "$fail" -ne 0 ]; then echo "SMOKE TEST FAILED"; exit 1; fi
echo "SMOKE TEST PASSED"
