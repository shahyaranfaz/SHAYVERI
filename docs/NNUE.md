# SHAYVERI NNUE

This document describes the NNUE architectures and binary formats implemented
by SHAYVERI, which are independent of source releases (such as v2.8.0) and
trained network artifact names (such as `SHAYVERI2_5_0`).

## SHAYVERI Architectures

There is no v1 SHAYVERI format. SHAYVERI's first runtime NNUE format was v2,
marking the change in evaluation type. Format v1 is therefore unassigned and
unsupported, not a removed historical format.

| Format |     Architecture | Inputs | Hidden |   Activation |         Output heads | Release history |  Support |
|-------:|-----------------:|-------:|-------:|-------------:|---------------------:|----------------:|---------:|
|     v2 | `Chess768x256-L` |    768 |    256 | clipped ReLU |           one linear | v2.0-v2.1, v2.3 |      YES |
|     v2 | `Chess768x512-L` |    768 |    512 | clipped ReLU |           one linear |           never |      YES |
|     v3 |      `KB8x256-L` |  6,144 |    256 |       SCReLU |           one linear |            v2.2 |      YES |
|     v3 |      `KB8x512-L` |  6,144 |    512 |       SCReLU |           one linear |           never |      YES |
|     v3 |     `KB16x256-L` | 12,288 |    256 |       SCReLU |           one linear |            v2.4 |      YES |
|     v3 |     `KB16x512-L` | 12,288 |    512 |       SCReLU |           one linear |       v2.5-v2.8 |      YES |
|     v4 |    `KB16x512-L8` | 12,288 |    512 |       SCReLU |       eight material |           never |      YES |

## Feature Families

### Chess768

`Chess768` represents each colored piece type on each square:

```text
2 colors * 6 piece types * 64 squares = 768 inputs
```

White and black perspectives use vertically mirrored squares and reversed
colors. The two perspective accumulators are updated incrementally.

### King Buckets (KB8 / KB16)

King-bucketed networks condition every Chess768 feature on the friendly king
region:

```text
Chess768 input * king bucket
```

The king mapping is horizontally symmetric. KB8 uses eight king regions and
KB16 uses sixteen. Moving a king can change the active feature block for its
perspective, requiring that perspective to be refreshed.

### Material Output Buckets

`KB16x512-L8` retains the KB16 feature transformer and selects one of eight
linear output heads:

```text
bucket = clamp((piece_count - 1) / 4, 0, 7)
```

Thus buckets cover 1-4, 5-8, ..., 29-32 pieces. Only the selected head is
evaluated at runtime.

## Evaluation

Each position has one accumulator per perspective. Evaluation orders them as
side-to-move and non-side-to-move, applies the network activation, concatenates
the results, and evaluates the selected output head.

SHAYVERI provides scalar and AVX2 evaluators. Both consume the same quantized
network and must return bit-identical integer scores. Accumulator updates are
checked against full refreshes across quiet moves, captures, promotions,
castling, en passant, and king moves.

## Binary formats

All integers are little-endian. Every format begins with:

|   Field |     Type |         Value |
|--------:|---------:|--------------:|
|   magic | `uint32` |  `0x4E4E5545` |
| version | `uint32` | format number |

### Format v2: classic Chess768

Header:

```text
magic, version=2
```

Payload:

```text
int16 feature_weights[768][hidden]
int16 feature_bias[hidden]
int16 output_weights[2 * hidden]
int32 output_bias
```

The loader infers the hidden width from the exact payload size: 394,764 bytes
selects `Chess768x256-L`, while 789,516 bytes selects `Chess768x512-L`.

Width is not stored explicitly. Any other inferred width, truncated section,
or trailing data is rejected.

### Format v3: king-bucketed single head

Header:

```text
magic, version=3, king_bucket_count
```

Payload:

```text
int16 feature_weights[768 * king_bucket_count][hidden]
int16 feature_bias[hidden]
int16 output_weights[2 * hidden]
int32 output_bias
```

The loader accepts 8 or 16 king buckets and infers a supported 256- or
512-channel hidden width from the payload. Exact sizes are 3,147,280 bytes for
`KB8x256-L`, 6,294,544 for `KB8x512-L`, 6,293,008 for `KB16x256-L`, and
12,586,000 for `KB16x512-L`.

King-bucket count is explicit, but hidden width is not. The exact file size
therefore determines which supported width is loaded.

### Format v4: explicit material-bucketed network

Header:

```text
uint32 magic
uint32 version = 4
uint32 king_bucket_count = 16
uint32 hidden_size = 512
uint32 output_bucket_count = 8
uint32 flags = 1 # SCReLU
uint32 feature_weights_bytes
uint32 feature_bias_bytes
uint32 output_weights_bytes
uint32 output_bias_bytes
```

Payload:

```text
int16 feature_weights[12288][512]
int16 feature_bias[512]
int16 output_weights[8][1024]
int32 output_bias[8]
```

The only accepted v4 architecture is `KB16x512-L8`, exactly 12,600,392 bytes.
Unlike v2/v3, its dimensions and section lengths are explicit. The loader
validates every architecture field, section size, total size, and trailing
byte. Other bucket counts, widths, flags, or layouts are rejected.

## Embedded Networks

By default, SHAYVERI embeds its `.nnue` file into its binary at build time,
using `src/tools/embed_nnue.cpp`. This tool copies the `.nnue` file into a
generated C++ byte array and is format-agnostic. Embedded and external
networks use the same runtime parser and therefore have identical
compatibility.

## Historical Compatibility

Every architecture listed in this file can be loaded through `EvalFile` or
embedded as the default network. Embedding preserves the original
bytes and uses the same parser as external loading.
