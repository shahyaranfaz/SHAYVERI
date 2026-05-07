# SHAYVERI v1.0 Results

This file records the final classical baseline validation snapshots before NNUE work.

## Tuning Logs

The historical SPSA notes and selected candidates are archived in:

- `tuning/spsa/pass1/pass1.txt`
- `tuning/spsa/pass2/pass2.txt`
- `tuning/spsa/pass3/pass3.txt`
- `tuning/spsa/pass4/pass4.txt`
- `tuning/spsa/passfinal/batch1_att1/`
- `tuning/spsa/passfinal/batch1_att2/`
- `tuning/texel/pass1`

## Direct Matches

`shaybot_texel0` vs `shaybot_texel1`, early stop at 22 games:

```text
Score of shaybot_texel0 vs shaybot_texel1: 1 - 20 - 1 [0.068] 22
```

`shaybot_texel1` vs `shaybot_pass4`, early stop at 10 games:

```text
Score of shaybot_texel1 vs shaybot_pass4: 0 - 9 - 1 [0.050] 10
```

`shaybot_pass4v2` vs `shaybot_pass4`, early bad build sample:

```text
Score of shaybot_pass4v2 vs shaybot_pass4: 1 - 23 - 0 [0.042] 24
```

`shaybot_pass4v2` vs `shaybot_pass4`, corrected parity sample:

```text
Score of shaybot_pass4v2 vs shaybot_pass4: 20 - 17 - 28 [0.523] 65
```

`shaybot_texel4` vs `shaybot_texel3`, 800 games at 10+0.1:

```text
White: 160 - 95 - 145
Black: 70 - 163 - 167
Elo: -12 +/- 19
```

`shaybot_texel4` vs `shaybot_texel1`, 800 games at 10+0.1:

```text
White: 173 - 92 - 135
Black: 96 - 167 - 137
Elo: +4 +/- 20
```

`shaybot_texel4` vs `shaybot_texel5.1`, early sample:

```text
Score of shaybot_texel4 vs shaybot_texel5.1: 24 - 36 - 39 [0.439] 99
```

`shaybot_texel5.2` vs `shaybot_texel5.3`, 200-game sample:

```text
Score of shaybot_texel5.2 vs shaybot_texel5.3: 65 - 66 - 69 [0.497] 200
```

`shaybot_texel4` vs `shaybot_texel5.3`, early sample:

```text
Score of shaybot_texel4 vs shaybot_texel5.3: 73 - 72 - 74 [0.502] 219
```

`shaybot_spsa_f2` vs `shaybot0`, 2500 games:

```text
Score of shaybot_spsa_f2 vs shaybot0: 2037 - 185 - 278 [0.870] 2500
Elo difference: +330.8 +/- 17.6
```

`shaybot_spsa_f` vs `shaybot_spsa_f2`, 2500 games:

```text
Score of shaybot_spsa_f vs shaybot_spsa_f2: 732 - 806 - 962 [0.485] 2500
Elo difference: -10.3 +/- 10.7
```

`shaybot_spsa_f3` vs `shaybot_spsa_f2`, 10+0.1:

```text
Score of shaybot_spsa_f3 vs shaybot_spsa_f2: 181 - 101 - 187 [0.585] 469
```

## Stockfish Reference

`shaybot_spsa_f2` vs `sf2850`, earlier 720-game snapshot:

```text
Score of shaybot vs sf2850: 237 - 379 - 105 [0.402] 721
```

`shaybot_spsa_f3` vs `sf2850`, early 297-game snapshot:

```text
Score of shaybot vs sf2850: 98 - 147 - 52 [0.418] 297
```

`shaybot_spsa_f3` vs `sf2850`, later 750-game snapshot:

```text
Score rate: about 0.433 after roughly 750 games.
Approximate local strength: about 2800 Elo in this setup.
```

These numbers are match-environment dependent and should be treated as release validation snapshots, not formal ratings.
