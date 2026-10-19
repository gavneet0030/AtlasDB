# Benchmark Methodology

## Required controls

- Use identical input data and query semantics.
- Record hardware, OS, compiler and build configuration.
- Separate cold-cache and warm-cache experiments.
- Warm up systems where appropriate.
- Run multiple repetitions.
- Report median and tail latency when sample size supports it.
- Record CPU time, peak memory and rows processed where possible.
- Verify result equivalence before comparing performance.

## Systems

Compare AtlasDB with DuckDB and PostgreSQL only for SQL features and
workloads that can be compared fairly.

Do not publish performance claims until experiments have been run.