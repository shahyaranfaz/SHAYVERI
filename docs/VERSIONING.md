# SHAYVERI Versioning

SHAYVERI separates source releases, evaluator generations, and NNUE
network artifacts.

## SHAYVERI Releases

SHAYVERI releases describe the source code and embedded defaults.

Format:

```text
SHAYVERI vMAJOR.MINOR[.PATCH]
```

Examples:

- `SHAYVERI v2.6`
- `SHAYVERI v2.6.1`

SHAYVERI releases cover search, UCI behavior, time management, datagen, opening
book logic, NNUE runtime support, build tooling, and the embedded default net
selection.

## Evaluator Generations

Evaluator generations describe the evaluation path, not the SHAYVERI version.

Current evaluator labels:

- `HCE-classical`: handcrafted evaluation path, selected with `UseNNUE=false`.
- `NNUE`: neural evaluator path.

Use `HCE-classical` for the handcrafted evaluator. It is not a SHAYVERI release.

## NNUE Networks

NNUE networks are named as artifacts and do not define the SHAYVERI version.

Format:

```text
SHAYVERI<network_version>.nnue
```

Example:

- `SHAYVERI2_5_0.nnue`

SHAYVERI v2.6 embeds `SHAYVERI2_5_0.nnue` as its default network. External
network files can still be loaded through the UCI `EvalFile` option.

## Result Labels

Every benchmark or release result should identify:

- SHAYVERI source version
- evaluator generation
- NNUE network, if used

Examples:

- `SHAYVERI v2.6 / NNUE SHAYVERI2_5_0`
- `SHAYVERI v2.6 / HCE-classical`
