# Correctness Strategy

- Unit-test individual components.
- Test malformed SQL and invalid input.
- Verify operator semantics with small deterministic datasets.
- Compare supported query results with DuckDB or PostgreSQL.
- Add regression tests for every fixed defect.
- Run sanitizers for memory and undefined-behavior defects.
- Introduce fuzzing when parser and execution interfaces are stable.

Correctness is a release requirement, not an optional benchmark feature.