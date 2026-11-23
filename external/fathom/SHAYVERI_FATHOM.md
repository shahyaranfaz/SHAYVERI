# Vendored Fathom

This directory contains an unmodified snapshot of Fathom used for Syzygy
tablebase probing in SHAYVERI.

- Upstream: https://github.com/jdart1/Fathom
- Commit: `c9c6fef0dddc05d2e242c183acf5833149ab676d`
- License: MIT; see [`LICENSE`](LICENSE)
- Local modifications: one behavior-preserving unsigned cast in `tbprobe.c`
  to keep SHAYVERI's warning-enabled C++ build clean

The snapshot is vendored so a normal clone or source archive contains the
complete dependency. It is not updated automatically.
