import sys
import os
import argparse
from pathlib import Path
import numpy as np
import nevergrad as ng

parser = argparse.ArgumentParser(
    description="Inspect binary Nevergrad CMA checkpoints for convergence state."
)
parser.add_argument("path", help="A .dat file or directory containing .dat files.")
parser.add_argument(
    "--show-best",
    action="store_true",
    help="Also print Nevergrad's noisy best-single recommendation/current_bests.",
)
args = parser.parse_args()

ROOT = Path(args.path)

import pathlib

if os.name == "nt":
    pathlib.PosixPath = pathlib.WindowsPath
else:
    pathlib.WindowsPath = pathlib.PosixPath


def inspect_dat(dat_file: Path):
    try:
        opt = ng.optimizers.base.Optimizer.load(str(dat_file))
    except Exception as e:
        print(f"\n[!] Failed: {dat_file} -> {e}")
        return

    print("\n" + "#" * 80)
    print(f"FILE: {dat_file}")
    print("#" * 80)

    es = getattr(opt, "_es", None)
    if es is None:
        print("[!] No CMA state found")
        return

    # ========================
    # CORE RUN INFO
    # ========================
    print("\n--- run ---")
    print(f"iterations: {getattr(es, 'countiter', 'n/a')}")
    print(f"evaluations: {getattr(es, 'countevals', 'n/a')}")
    print(f"popsize: {getattr(es, 'popsize', 'n/a')}")

    # ========================
    # FITNESS
    # ========================
    try:
        res = es.result
        print("\n--- fitness ---")
        print(f"best f: {res.fbest}")
        print(f"evals @ best: {res.evals_best}")
    except:
        pass

    # ========================
    # STEP SIZE (CONVERGENCE)
    # ========================
    sigma = getattr(es, "sigma", None)
    sigma0 = getattr(es, "sigma0", None)

    print("\n--- step size ---")
    print(f"sigma: {sigma}")
    print(f"sigma0: {sigma0}")

    # ========================
    # CONDITIONING (LANDSCAPE SHAPE)
    # ========================
    cond = None
    if hasattr(es, "D"):
        try:
            D = np.array(es.D)
            cond = float(np.max(D) / np.min(D))

            print("\n--- covariance spectrum ---")
            print("D:", D)
            print("condition number:", cond)
        except:
            pass

    # ========================
    # SEARCH STATE
    # ========================
    print("\n--- search state ---")

    if hasattr(es, "mean"):
        print("mean:", es.mean)

    if hasattr(es, "stds"):
        print("stds:", es.stds)

    # evolution path (directional momentum)
    if hasattr(es, "pc"):
        try:
            print("||pc||:", np.linalg.norm(es.pc))
        except:
            print("pc:", es.pc)

    # ========================
    # DIAGNOSTICS
    # ========================
    print("\n--- diagnostics ---")

    if sigma is not None:
        if sigma < 1e-3:
            print("status: converged (very small sigma)")
        elif sigma < 1e-2:
            print("status: near convergence")
        else:
            print("status: still exploring")

    if cond is not None:
        if cond > 1e3:
            print("conditioning: very high (ridge / unstable)")
        elif cond > 50:
            print("conditioning: high")
        elif cond > 10:
            print("conditioning: moderate")
        else:
            print("conditioning: good")

    print()
    print("note: checkpoint recommendations are best-single/noisy.")
    print("      use view_cmaes_log.py top-percent robust candidates for params.")

    if args.show_best:
        # ========================
        # NOISY BEST TRACKING
        # ========================
        try:
            rec = opt.provide_recommendation()
            print("\n--- noisy recommendation ---")
            for k, v in rec.kwargs.items():
                print(f"{k}: {v}")
        except Exception as e:
            print(f"[!] recommendation failed: {e}")

        if hasattr(opt, "current_bests"):
            print("\n--- current bests ---")
            print(opt.current_bests)


def dat_files(root: Path):
    if root.is_file():
        if root.suffix == ".dat":
            return [root]
        return []

    if list(root.glob("*.dat")):
        return sorted(root.glob("*.dat"))

    dats = []
    for d in sorted(root.iterdir()):
        if d.is_dir():
            dats.extend(sorted(d.rglob("*.dat")))
    return dats


dats = dat_files(ROOT)
if not dats:
    print(f"[!] No .dat files found under {ROOT}")

for f in dats:
    inspect_dat(f)
