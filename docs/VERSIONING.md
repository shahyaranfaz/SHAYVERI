# SHAYVERI Versioning

SHAYVERI uses three-component source-release versions:

```text
SHAYVERI vX.Y.Z
```

All three components are required in tags, changelog headings, result labels,
release archives, and user-facing documentation.

### Note

The current versioning scheme was created with v2.5.0 and strengthened through
v2.6.0 and the development of v2.7.0. Earlier version numbers were assigned to
historical engine snapshots for consistency. Those releases remain usable
milestones. Published releases since v2.3.0 include recorded STC and LTC
strength pins. This distinction concerns versioning and release organization
and does not necessarily reflect the rigor of testing or Elo pinning performed
on the engine.

## Release Scope

Version bumps to `Y` or `Z` are triggered only by engine changes, including
search, evaluation, time management, protocol behavior, and NNUE runtime
changes. Documentation, tooling, CI, and other development-infrastructure
changes do not trigger a version bump and are not recorded in the changelog.

The changelog is a snapshot of published engine behavior, not a record of every
repository change.

## X: Development Era

`X` identifies the engine's development era. It is project-defined rather than
SemVer compatibility signaling.

- `0`: developing the first usable engine
- `1`: refining and tuning the HCE engine
- `2`: NNUE integration, networks, and NNUE-era search work
- `3`: planned future work

The major component changes only when the project moves to the next era.

## Y: Significant Release

`Y` identifies a significant release within an era.

- Start at `0` when `X` changes.
- Increase by exactly one for each significant release.
- Do not skip values.
- Use a new `Y` for a substantial engine, evaluation, search, time-management,
  opening-book, protocol, or NNUE-runtime milestone.
- Reset `Z` to `0` whenever `Y` changes.

Examples: `v0.1.0`, `v0.2.0`, `v0.3.0`, then `v1.0.0`, `v1.1.0`.

## Z: Patch Release

`Z` identifies a minor compatible change or fix to the current `X.Y` release.

- Start at `0` for every new `X.Y` release.
- Increase by exactly one for each published patch.
- Use patches for engine correctness fixes and other minor engine changes that
  do not justify a new `Y`.
- A patch must never be listed after a later `Y` release. For example,
  `v2.7.2` must precede `v2.8.0`.

## Source Releases and NNUE Networks

SHAYVERI source versions and NNUE network artifact names are independent.

- Source release: `SHAYVERI v2.6.0`
- Evaluator: `NNUE` or `HCE-classical`
- Network artifact: `SHAYVERI2_5_0.nnue`

The network artifact does not determine the source version. A complete result
label names all applicable parts:

```text
SHAYVERI v2.6.0 / NNUE SHAYVERI2_5_0
SHAYVERI v2.6.0 / HCE-classical
```
