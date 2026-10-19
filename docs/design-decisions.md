# Design Decisions

## ADR-001: C++20

Status: Accepted for initial implementation.

Reason: C++20 offers standard concurrency primitives, RAII, strong type
support, and control over data layout and allocation.

## ADR-002: Linux-first development

Status: Accepted.

Reason: Linux offers relevant tooling for profiling, system calls,
threading, memory analysis and reproducible systems experiments.

## ADR-003: Build with CMake and Ninja

Status: Accepted.

Reason: CMake supports portable build configuration and Ninja provides
fast incremental builds.

## ADR-004: Implement incrementally

Status: Accepted.

Reason: Directory structure alone does not demonstrate database behavior.
Each subsystem must be tested before advanced optimization is added.