# AtlasDB

**A C++20 analytical database and SQL query execution engine.**

AtlasDB is an experimental database engine built to explore database internals:
SQL processing, query operators, catalog management, table persistence, and
reproducible correctness testing.

It is a learning and research project, not a production-ready replacement for
DuckDB, PostgreSQL, or other established database systems.

## Current implementation

The repository currently includes:

- C++20 core types, values, schemas, and in-memory tables.
- Table catalog with case-insensitive table-name lookup.
- SQL statement handling for `CREATE TABLE`, `INSERT`, and `SELECT`.
- SQL parsing for supported expressions and filtering.
- Query operators for table scans, filtering, projection, aggregation, and sorting.
- `ORDER BY` support.
- Multi-table database catalogs.
- Table serialization and loading.
- A database manifest that records the tables in a persisted database.
- Command-line SQL execution and interactive shell modes.
- SQL-file and shell-file execution modes. Both modes load an existing AtlasDB table-storage file; `--sql-file` also requires SQL statements, while `--shell-file` starts an interactive shell.
- CMake-based builds and automated tests.

The engine is under active development. Implemented features should not be
interpreted as complete SQL-standard compatibility.

## Architecture

The current execution path is organized around the following concepts:

1. SQL input and statement parsing.
2. Statement handling and table-catalog lookup.
3. Query planning and execution operators.
4. In-memory table data and schema validation.
5. Table serialization and database-manifest persistence.
6. CLI output and automated correctness tests.

The longer-term research direction includes columnar storage, vectorized
execution, query optimization, parallel processing, and analytical benchmarking.
Those goals are not all implemented in the current version.

## Technology

- C++20
- CMake
- Visual Studio 2022 on Windows
- GoogleTest and CTest
- PowerShell for Windows CLI integration tests
- Linux/WSL development scripts

## Build on Windows

Open PowerShell in the repository directory.

Configure the Debug build:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -DATLASDB_BUILD_TESTS=ON
```

Compile:

```powershell
cmake --build build --config Debug --parallel 4
```

Run tests:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

Build and validate the Release configuration:

```powershell
cmake -S . -B build-release -G "Visual Studio 17 2022" -DATLASDB_BUILD_TESTS=ON
cmake --build build-release --config Release --parallel 4
ctest --test-dir build-release -C Release --output-on-failure
```

The latest verified local Release run on 18 October 2026 passed all 234 registered CTest tests. Rerun the commands above after subsequent changes to verify the working tree.

## Command-line usage

The executable is produced at:

```text
build-release/Release/atlasdb.exe
```

Check the version:

```powershell
.\build-release\Release\atlasdb.exe --version
```

Execute SQL directly:

```powershell
.\build-release\Release\atlasdb.exe --sql "CREATE TABLE products (id INT64, name STRING); INSERT INTO products VALUES (1, 'Keyboard'); SELECT * FROM products;"
```

Run the interactive SQL shell:

```powershell
.\build-release\Release\atlasdb.exe --shell
```

Run SQL against a saved table:

```powershell
.\build-release\Release\atlasdb.exe --save-demo .\users.adb
.\build-release\Release\atlasdb.exe --sql-file .\users.adb "SELECT * FROM users;"
```

Open an interactive shell using a saved table:

```powershell
.\build-release\Release\atlasdb.exe --save-demo .\users.adb
.\build-release\Release\atlasdb.exe --shell-file .\users.adb
```

The executable also retains the demo-data command:

```powershell
.\build-release\Release\atlasdb.exe --save-demo
```

Use `--help` only if help output is implemented in the current executable;
the documented, verified version command is `--version`.

## Persistent databases

AtlasDB supports a directory-backed database mode:

```powershell
.\build-release\Release\atlasdb.exe --database .\my_database --sql "CREATE TABLE products (id INT64, name STRING); INSERT INTO products VALUES (1, 'Keyboard');"
```

Run a later query against the same database directory:

```powershell
.\build-release\Release\atlasdb.exe --database .\my_database --sql "SELECT * FROM products;"
```

The database directory contains a manifest and separate serialized table files.
The manifest identifies which tables belong to the database.

Persistence is currently basic. Full transaction processing, crash recovery,
concurrent-writer coordination, and production-grade durability guarantees
are not implemented.

## Supported SQL subset

The current SQL implementation includes:

- `CREATE TABLE`
- `INSERT`, including explicit column lists
- `SELECT`
- Column projection and `SELECT *`
- `WHERE` filtering
- Comparison operators `=`, `!=`, `<>`, `<`, `<=`, `>`, and `>=`
- Logical operators `AND`, `OR`, and `NOT`
- Parenthesized filter expressions
- `ORDER BY`
- Integer, floating-point, Boolean, string, and null values, subject to the
  implemented type and expression rules

SQL compatibility is intentionally limited. The SQL parser and query planner support `INNER JOIN ... ON ...` and SQL `GROUP BY` with aggregate functions, subject to the implemented validation rules. Aggregate queries combined with JOIN are not yet supported. SQL compatibility remains intentionally limited.

Consult the implementation and tests for the exact behavior of individual
statements and data types.

## Testing

The test suite contains unit tests and CLI integration coverage.

Run the registered CTest suite with:

```powershell
ctest --test-dir build-release -C Release --output-on-failure
```

The integration tests exercise areas including:

- Version output.
- Table creation and row insertion.
- Persistence across process restarts.
- Independent tables in one database directory.
- Missing-table error handling.
- Legacy demo-command behavior.

A passing test suite establishes only the behaviors covered by those tests.
It does not establish production readiness, SQL-standard compliance, or
performance superiority over established databases.

## Repository layout

```text
include/       Public headers
src/           C++ implementation
tests/         Unit and integration tests
docs/          Architecture and design notes
examples/      Example documentation and usage
benchmarks/    Benchmark methodology and reference-engine plans
scripts/       Build and development scripts
python/        Python analysis tooling
config/        Configuration resources
docker/        Container-related resources
k8s/           Kubernetes-related resources
```

Local build directories, runtime database files, logs, and historical backups
should not be treated as source deliverables unless a specific fixture or
artifact is intentionally selected for version control.

## Development principles

1. Correctness before performance claims.
2. Regression tests for behavioral changes.
3. Reproducible benchmark methodology.
4. Explicit documentation of limitations.
5. Small, reviewable changes with backups before risky edits.
6. No invented performance numbers or unsupported compatibility claims.

## Roadmap

Potential future work includes:

- Broader SQL syntax and expression support.
- Joins and SQL aggregation syntax.
- Stronger persistence guarantees and transaction support.
- Improved storage formats and memory management.
- Vectorized execution.
- Query profiling and `EXPLAIN`.
- Parallel query execution.
- Benchmark comparisons against DuckDB and PostgreSQL.
- Additional fuzzing, sanitizer, and recovery testing.

Roadmap items are goals, not claims about current functionality.

## Project status

AtlasDB has progressed beyond its initial repository bootstrap into a working
experimental SQL engine with a catalog, CLI, basic database persistence, and
automated test coverage. It remains an evolving research project with known
SQL, storage, and reliability limitations.
