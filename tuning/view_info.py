import sys
from pathlib import Path
import numpy as np
import nevergrad as ng

ROOT = Path(sys.argv[1])

import pathlib
pathlib.PosixPath = pathlib.WindowsPath  # cross-OS pickle fix


def dump_obj(title, obj):
    print(f"\n--- {title} ---")

    # attributes
    for k in sorted(dir(obj)):
        if k.startswith("__"):
            continue
        try:
            v = getattr(obj, k)
            # avoid flooding huge callables
            if callable(v):
                continue
            print(f"{k}: {v}")
        except:
            pass


def inspect_dat(dat_file: Path):
    try:
        opt = ng.optimizers.ParametrizedCMA().load(str(dat_file))
    except Exception as e:
        print(f"\n[!] Failed: {dat_file} -> {e}")
        return

    print("\n" + "#" * 80)
    print(f"FILE: {dat_file}")
    print("#" * 80)

    # ES engine (actual CMA-ES state)
    es = getattr(opt, "_es", None)
    if es is not None:
        dump_obj("CMA (_es)", es)

        # covariance spectrum (very informative)
        if hasattr(es, "C"):
            try:
                eigvals = np.linalg.eigvals(es.C)
                print("\n--- covariance eigenvalues (sorted) ---")
                print(np.sort(eigvals)[::-1])
            except:
                pass

    # parametrization (actual tuned params)
    try:
        rec = opt.provide_recommendation()
        print("\n--- recommendation ---")
        for k, v in rec.kwargs.items():
            print(f"{k}: {v}")
    except Exception as e:
        print(f"[!] recommendation failed: {e}")

    # best observed if available
    if hasattr(opt, "current_bests"):
        print("\n--- current bests ---")
        print(opt.current_bests)


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