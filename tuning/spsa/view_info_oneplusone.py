import sys
from pathlib import Path
import numpy as np
import nevergrad as ng

ROOT = Path(sys.argv[1])

import pathlib
pathlib.PosixPath = pathlib.WindowsPath  # cross-OS pickle fix


def dump_obj(title, obj):
    print(f"\n--- {title} ---")
    for k in sorted(dir(obj)):
        if k.startswith("__"):
            continue
        try:
            v = getattr(obj, k)
            if callable(v):
                continue
            print(f"{k}: {v}")
        except:
            pass


def inspect_dat(dat_file: Path):
    try:
        opt = ng.optimizers.base.Optimizer.load(str(dat_file))
    except Exception as e:
        print(f"\n[!] Failed: {dat_file} -> {e}")
        return

    print("\n" + "#" * 80)
    print(f"FILE: {dat_file}")
    print("#" * 80)

    # core optimizer state
    dump_obj("OnePlusOne (opt)", opt)

    # mutation / step-size info (key part of 1+1)
    for attr in ["sigma", "_sigma", "scale", "_scale"]:
        if hasattr(opt, attr):
            print(f"\n{attr}: {getattr(opt, attr)}")

    # recommendation (your tuned params)
    try:
        rec = opt.provide_recommendation()
        print("\n--- recommendation ---")
        for k, v in rec.kwargs.items():
            print(f"{k}: {v}")
    except Exception as e:
        print(f"[!] recommendation failed: {e}")

    # best observed
    if hasattr(opt, "current_bests"):
        print("\n--- current bests ---")
        print(opt.current_bests)

    # archive (very important for OnePlusOne)
    if hasattr(opt, "archive"):
        print("\n--- archive summary ---")
        try:
            print(f"size: {len(opt.archive)}")

            # print top few entries
            items = list(opt.archive.items())[:5]
            for i, (k, v) in enumerate(items):
                print(f"{i}: loss={v.loss}, params={k}")
        except Exception as e:
            print(f"[!] archive read failed: {e}")


for d in sorted(ROOT.iterdir()):
    if not d.is_dir():
        continue

    dats = list(d.rglob("*.dat"))
    if not dats:
        continue

    print("\n" + "=" * 80)
    print(f"DIR: {d}")
    print("=" * 80)

    for f in dats:
        inspect_dat(f)