#!/usr/bin/env bash
set -uo pipefail

if [[ $# -ne 1 ]]; then
    printf 'Usage: %s <atlasdb-executable>\n' "$0" >&2
    exit 2
fi

EXECUTABLE="$1"
if [[ ! -x "$EXECUTABLE" ]]; then
    printf 'AtlasDB executable not found or not executable: %s\n' "$EXECUTABLE" >&2
    exit 1
fi

test_root="$(mktemp -d "${TMPDIR:-/tmp}/atlasdb_cli_test.XXXXXX")" || exit 1
trap 'rm -rf "$test_root"' EXIT
db_path="$test_root/database"
LAST_OUTPUT=""

run_atlasdb() {
    local expected_exit_code="$1"
    shift

    LAST_OUTPUT="$("$EXECUTABLE" "$@" 2>&1)"
    local actual_exit_code=$?

    printf '%s\n' "$LAST_OUTPUT"
    if [[ "$actual_exit_code" -ne "$expected_exit_code" ]]; then
        printf 'Expected exit code %s, got %s. Arguments: %s\n' \
            "$expected_exit_code" "$actual_exit_code" "$*" >&2
        return 1
    fi
}

assert_contains() {
    local text="$1"
    local expected="$2"
    local description="$3"

    if [[ "$text" != *"$expected"* ]]; then
        printf '%s failed. Expected output to contain: %s\n' \
            "$description" "$expected" >&2
        return 1
    fi
}

printf '\n[1/8] Help command\n'
run_atlasdb 0 --help || exit 1
assert_contains "$LAST_OUTPUT" "Usage:" "Help command" || exit 1
assert_contains "$LAST_OUTPUT" "--help" "Help command" || exit 1

printf '\n[2/8] Version command\n'
run_atlasdb 0 --version || exit 1
assert_contains "$LAST_OUTPUT" "AtlasDB" "Version command" || exit 1

printf '\n[3/8] Unknown option returns exit code 2\n'
run_atlasdb 2 --invalid-option || exit 1
assert_contains "$LAST_OUTPUT" "Usage:" "Unknown option" || exit 1
run_atlasdb 0 --version || exit 1
assert_contains "$LAST_OUTPUT" "AtlasDB" "Version command" || exit 1

printf '\n[4/8] Create table and insert multiple rows\n'
sql="CREATE TABLE products (id INT64, name STRING, price DOUBLE); INSERT INTO products VALUES (1, 'Keyboard', 49.99); INSERT INTO products VALUES (2, 'Mouse', 19.5);"
run_atlasdb 0 --database "$db_path" --sql "$sql" || exit 1
assert_contains "$LAST_OUTPUT" "Table created: products" "CREATE TABLE" || exit 1
assert_contains "$LAST_OUTPUT" "Rows inserted: 1" "INSERT" || exit 1

printf '\n[5/8] Verify persistence after process restart\n'
run_atlasdb 0 --database "$db_path" --sql "SELECT * FROM products;" || exit 1
assert_contains "$LAST_OUTPUT" "Keyboard" "Persisted first product" || exit 1
assert_contains "$LAST_OUTPUT" "Mouse" "Persisted second product" || exit 1

printf '\n[6/8] Verify a second independent table persists\n'
sql="CREATE TABLE audit (id INT64, event STRING); INSERT INTO audit VALUES (1, 'startup');"
run_atlasdb 0 --database "$db_path" --sql "$sql" || exit 1
run_atlasdb 0 --database "$db_path" --sql "SELECT * FROM audit;" || exit 1
assert_contains "$LAST_OUTPUT" "startup" "Persisted audit table" || exit 1

printf '\n[7/8] Verify SQL errors return a nonzero exit code\n'
run_atlasdb 1 --database "$db_path" --sql "SELECT * FROM missing_table;" || exit 1
assert_contains "$LAST_OUTPUT" "SQL error" "Missing table error" || exit 1

printf '\n[8/8] Verify legacy demo command still works\n'
run_atlasdb 0 --sql "SELECT * FROM users;" || exit 1
assert_contains "$LAST_OUTPUT" "Aman" "Legacy users demo" || exit 1
assert_contains "$LAST_OUTPUT" "Priya" "Legacy users demo" || exit 1

printf '\nALL CLI INTEGRATION CHECKS PASSED\n'