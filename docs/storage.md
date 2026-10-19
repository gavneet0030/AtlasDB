# Storage Design

The target storage layer uses typed column vectors organized into chunks.

Planned metadata includes row counts, null counts and min/max values where
those statistics can be maintained correctly.

The initial implementation should favor simple, testable in-memory columns.
Persistent storage, compression and Parquet support should be added only
after the basic data model and correctness tests are stable.