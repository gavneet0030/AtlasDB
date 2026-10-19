# Query Engine Roadmap

The planned query engine includes:

- SQL lexer, parser and AST.
- Logical and physical query plans.
- Scan, filter and projection operators.
- Aggregation and hash joins.
- Vectorized batch execution.
- Predicate pushdown and projection pruning.
- Statistics-driven optimization.
- Parallel execution.
- EXPLAIN and query profiling.

Each feature requires semantic tests and should be introduced in dependency
order rather than implemented all at once.