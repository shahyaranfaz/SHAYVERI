#!/usr/bin/env python3
import argparse
import json
import struct


INPUT_SIZE = 768
HIDDEN_SIZE = 256
NNUE_MAGIC = 0x4E4E5545
NNUE_VERSION = 2
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
    parser.add_argument("--feature-scale", type=float, default=L1_SCALE)
    parser.add_argument("--output-scale", type=float, default=L1_SCALE)
    parser.add_argument("--bias-scale", type=float, default=L1_SCALE * L1_SCALE)
    args = parser.parse_args()

    with open(args.input_json, "r", encoding="utf-8") as f:
        net = json.load(f)

    ft_weight = net["ft.weight"]
    ft_bias = net["ft.bias"]
    out_weight = net["out.weight"]
    out_bias = net["out.bias"]

    require_shape("ft.weight", ft_weight, [HIDDEN_SIZE, INPUT_SIZE])
    require_shape("ft.bias", ft_bias, [HIDDEN_SIZE])
    require_shape("out.weight", out_weight, [1, HIDDEN_SIZE * 2])
    require_shape("out.bias", out_bias, [1])

    with open(args.output_nnue, "wb") as f:
        f.write(struct.pack("<II", NNUE_MAGIC, NNUE_VERSION))

        # Marlinflow stores ft.weight as [hidden][input]; SHAYVERI reads [input][hidden].
        for input_idx in range(INPUT_SIZE):
            for hidden_idx in range(HIDDEN_SIZE):
                f.write(struct.pack("<h", quant_i16(ft_weight[hidden_idx][input_idx],
                                                    args.feature_scale)))

        for hidden_idx in range(HIDDEN_SIZE):
            f.write(struct.pack("<h", quant_i16(ft_bias[hidden_idx], args.bias_scale)))

        for weight in out_weight[0]:
            f.write(struct.pack("<h", quant_i16(weight, args.output_scale)))

        f.write(struct.pack("<i", quant_i32(out_bias[0], args.output_scale)))


if __name__ == "__main__":
    main()
