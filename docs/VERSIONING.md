# SHAYVERI Versioning

SHAYVERI uses three-component source-release versions:

```text
SHAYVERI vX.Y.Z
```

All three components are required in tags, changelog headings, result labels,
release archives, and user-facing documentation.

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
