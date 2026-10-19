# Tests

Use CTest to execute registered tests.

Recommended test groups:
- Unit: individual data structures and functions.
- Parser: valid and invalid SQL.
- Planner: logical and physical plan construction.
- Optimizer: semantics-preserving rewrites.
- Storage: column vectors, chunks and file readers.
- Execution: operator correctness.
- Integration: end-to-end query behavior.
- Regression: previously fixed bugs.
- Fuzz: parser robustness and supported query equivalence.

Only tests actually implemented and registered are executed by CTest.