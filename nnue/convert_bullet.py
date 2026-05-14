#!/usr/bin/env python3
import argparse
import os
import struct


CHESS768_INPUT_SIZE = 768
MAX_KING_BUCKETS = 8
HIDDEN_SIZE = 256
NNUE_MAGIC = 0x4E4E5545
NNUE_VERSION_CLASSIC = 2
NNUE_VERSION_KB = 3

def read_exact(data, offset, size, name):
    end = offset + size
    if end > len(data):
        raise ValueError(f"{name} truncated: need {size} bytes at offset {offset}")
    return data[offset:end], end


def main():
    parser = argparse.ArgumentParser(
        description="Convert Bullet quantised.bin saved by shayveri_kb8.rs to SHAYVERI .nnue"
    )
    parser.add_argument("input_quantised")
    parser.add_argument("output_nnue")
    parser.add_argument("--king-buckets", type=int, default=MAX_KING_BUCKETS)
    args = parser.parse_args()

    if args.king_buckets not in (1, MAX_KING_BUCKETS):
        raise ValueError(f"king-buckets must be 1 or {MAX_KING_BUCKETS}, got {args.king_buckets}")

    input_size = CHESS768_INPUT_SIZE * args.king_buckets
    l0w_count = input_size * HIDDEN_SIZE
    l0b_count = HIDDEN_SIZE
    l1w_count = HIDDEN_SIZE * 2
    l1b_size = 4

    # SavedFormat order in nnue/shayveri_kb8.rs:
    #   l0w i16, merged with factoriser, flat input-major [input][hidden]
    #   l0b i16
    #   l1w i16, transposed to flat [stm hidden][nstm hidden]
    #   l1b i32
    payload_size = (
        l0w_count * 2
        + l0b_count * 2
        + l1w_count * 2
        + l1b_size
    )

    with open(args.input_quantised, "rb") as f:
        data = f.read()

    if len(data) < payload_size:
        raise ValueError(
            f"{args.input_quantised} too small: {len(data)} bytes, expected at least {payload_size}"
        )

    offset = 0
    feature_weights, offset = read_exact(data, offset, l0w_count * 2, "l0w")
    feature_bias, offset = read_exact(data, offset, l0b_count * 2, "l0b")
    output_weights, offset = read_exact(data, offset, l1w_count * 2, "l1w")
    output_bias_raw, offset = read_exact(data, offset, l1b_size, "l1b")
    output_bias = output_bias_raw

    padding = data[offset:]
    if output_bias_raw[2:4] == b"bu" or padding.startswith(b"llet"):
        raise ValueError(
            f"{args.input_quantised} looks like the old legacy64 Bullet format "
            "(2-byte l1b followed by Bullet padding). Retrain with nnue/shayveri_kb8.rs "
            "saving l1w at scale 255 and l1b as i32 at scale 255*255."
        )
    if padding:
        non_zero = sum(1 for b in padding if b)
        if non_zero and not padding.startswith(b"bullet"):
            raise ValueError(
                f"{args.input_quantised} has {len(padding)} unexpected trailing bytes "
                f"({non_zero} non-zero)"
            )
        print(f"warning: ignoring {len(padding)} Bullet padding bytes in {args.input_quantised}")

    version = NNUE_VERSION_KB if args.king_buckets > 1 else NNUE_VERSION_CLASSIC
    os.makedirs(os.path.dirname(os.path.abspath(args.output_nnue)), exist_ok=True)

    with open(args.output_nnue, "wb") as f:
        f.write(struct.pack("<II", NNUE_MAGIC, version))
        if version == NNUE_VERSION_KB:
            f.write(struct.pack("<I", args.king_buckets))
        f.write(feature_weights)
        f.write(feature_bias)
        f.write(output_weights)
        f.write(output_bias)

    written = os.path.getsize(args.output_nnue)
    print(
        f"wrote {args.output_nnue}: input={input_size} "
        f"hidden={HIDDEN_SIZE} king_buckets={args.king_buckets} bytes={written}"
    )


if __name__ == "__main__":
    main()
