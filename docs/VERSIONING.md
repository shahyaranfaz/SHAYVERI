# SHAYVERI Versioning

SHAYVERI separates engine/source releases, evaluator generations, and NNUE
network artifacts.

## Engine Releases

Engine releases describe the source code and embedded defaults.

Format:

```text
SHAYVERI vMAJOR.MINOR[.PATCH]
```

Examples:

- `SHAYVERI v2.6`
- `SHAYVERI v2.6.1`

Engine releases cover search, UCI behavior, time management, datagen, opening
book logic, NNUE runtime support, build tooling, and the embedded default net
selection.

## Evaluator Generations

Evaluator generations describe the evaluation path, not the engine version.

Current evaluator labels:

- `HCE-classical`: handcrafted evaluation path, selected with `UseNNUE=false`.
- `NNUE`: neural evaluator path.

Use `HCE-classical` for the handcrafted evaluator. It is not an engine release.

## NNUE Networks

NNUE networks are named as artifacts and do not define the engine version.

Format:

```text
SHAYVERI<network_version>.nnue
```

Example:

- `SHAYVERI2_5_0.nnue`

The v2.6 engine embeds `SHAYVERI2_5_0.nnue` as its default network. External
network files can still be loaded through the UCI `EvalFile` option.

## Result Labels

Every benchmark or release result should identify:

- engine/source version;
- evaluator generation;
- NNUE network, if used.

Examples:

- `SHAYVERI v2.6 / NNUE SHAYVERI2_5_0`
- `SHAYVERI v2.6 / HCE-classical`
