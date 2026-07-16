#!/usr/bin/env python3
import argparse
import json
import struct


CHESS768_INPUT_SIZE = 768
MAX_KING_BUCKETS = 8
HIDDEN_SIZE = 256
NNUE_MAGIC = 0x4E4E5545
NNUE_VERSION_CLASSIC = 2
NNUE_VERSION_KB = 3
L1_SCALE = 255
OUTPUT_SCALE = 400


def clamp_i16(value):
    return max(-32768, min(32767, value))


def quant_i16(value, scale):
    return clamp_i16(round(float(value) * scale))


def quant_i32(value, scale):
    return round(float(value) * scale)


def shape(value):
    if isinstance(value, list):
        return [len(value)] + (shape(value[0]) if value else [])
    return []


def require_shape(name, value, expected):
    actual = shape(value)
    if actual != expected:
        raise ValueError(f"{name} has shape {actual}, expected {expected}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input_json")
    parser.add_argument("output_nnue")
    parser.add_argument("--king-buckets", type=int, default=None)
    parser.add_argument("--feature-scale", type=float, default=L1_SCALE)
    parser.add_argument("--output-scale", type=float, default=L1_SCALE)
    parser.add_argument("--feature-bias-scale", type=float, default=L1_SCALE)
    parser.add_argument("--output-bias-scale", type=float, default=L1_SCALE * L1_SCALE)
    args = parser.parse_args()

    with open(args.input_json, "r", encoding="utf-8") as f:
        net = json.load(f)

    ft_weight = net["ft.weight"]
    ft_bias = net["ft.bias"]
    out_weight = net["out.weight"]
    out_bias = net["out.bias"]

    if not ft_weight or not ft_weight[0]:
        raise ValueError("ft.weight must be a non-empty 2D matrix")
    if len(ft_weight) == HIDDEN_SIZE:
        require_shape("ft.weight", ft_weight, [HIDDEN_SIZE, len(ft_weight[0])])
    elif len(ft_weight[0]) == HIDDEN_SIZE:
        input_size = len(ft_weight)
        require_shape("ft.weight", ft_weight, [input_size, HIDDEN_SIZE])
        ft_weight = [
            [ft_weight[input_idx][hidden_idx] for input_idx in range(input_size)]
            for hidden_idx in range(HIDDEN_SIZE)
        ]
    else:
        raise ValueError(
            f"ft.weight must be [hidden][input] or [input][hidden] with hidden={HIDDEN_SIZE}, "
            f"got {shape(ft_weight)}"
        )
    require_shape("ft.bias", ft_bias, [HIDDEN_SIZE])
    require_shape("out.weight", out_weight, [1, HIDDEN_SIZE * 2])
    require_shape("out.bias", out_bias, [1])

    input_size = len(ft_weight[0])
    if args.king_buckets is None:
        if input_size % CHESS768_INPUT_SIZE != 0:
            raise ValueError(
                f"ft.weight input size {input_size} is not divisible by {CHESS768_INPUT_SIZE}"
            )
        king_buckets = input_size // CHESS768_INPUT_SIZE
    else:
        king_buckets = args.king_buckets

    if king_buckets not in (1, MAX_KING_BUCKETS):
        raise ValueError(f"king-buckets must be 1 or {MAX_KING_BUCKETS}, got {king_buckets}")
    expected_input = CHESS768_INPUT_SIZE * king_buckets
    if input_size != expected_input:
        raise ValueError(
            f"ft.weight has input size {input_size}, expected {expected_input} for king-buckets={king_buckets}"
        )

    version = NNUE_VERSION_KB if king_buckets > 1 else NNUE_VERSION_CLASSIC

    with open(args.output_nnue, "wb") as f:
        f.write(struct.pack("<II", NNUE_MAGIC, version))
        if version == NNUE_VERSION_KB:
            f.write(struct.pack("<I", king_buckets))

        # Marlinflow stores ft.weight as [hidden][input], while SHAYVERI reads [input][hidden].
        for input_idx in range(input_size):
            for hidden_idx in range(HIDDEN_SIZE):
                f.write(struct.pack("<h", quant_i16(ft_weight[hidden_idx][input_idx],
                                                    args.feature_scale)))

        for hidden_idx in range(HIDDEN_SIZE):
            f.write(struct.pack("<h", quant_i16(ft_bias[hidden_idx],
                                                args.feature_bias_scale)))

        for weight in out_weight[0]:
            f.write(struct.pack("<h", quant_i16(weight, args.output_scale)))

        f.write(struct.pack("<i", quant_i32(out_bias[0], args.output_bias_scale)))


if __name__ == "__main__":
    main()
