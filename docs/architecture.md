# AtlasDB Architecture

## Target query pipeline

SQL text
-> Lexer and parser
-> Abstract syntax tree
-> Logical plan
-> Logical rewrites
-> Statistics and cost estimation
-> Physical plan
-> Execution operators
-> Result set

## Storage pipeline

CSV / Parquet
-> Schema validation
-> Typed columns
-> Chunks and column segments
-> Column metadata and statistics
-> Disk-backed storage

## Initial implementation order

1. Common types and error handling.
2. Schema and catalog.
3. Column vectors and data chunks.
4. CSV reader.
5. SQL lexer and AST.
6. Logical plan.
7. Scan, filter and projection.
8. Aggregation.
9. Query optimization.
10. Parallel execution.

The architecture is a target design. Components are not implemented merely
because their directories exist.