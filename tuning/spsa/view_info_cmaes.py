import sys
from pathlib import Path
import numpy as np
import nevergrad as ng

ROOT = Path(sys.argv[1])

import pathlib

pathlib.PosixPath = pathlib.WindowsPath  # cross-OS pickle fix


def inspect_dat(dat_file: Path):
    try:
        opt = ng.optimizers.ParametrizedCMA().load(str(dat_file))
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

    # ========================
    # RECOMMENDATION (FINAL PARAMS)
    # ========================
    try:
        rec = opt.provide_recommendation()
        print("\n--- recommendation ---")
        for k, v in rec.kwargs.items():
            print(f"{k}: {v}")
    except Exception as e:
        print(f"[!] recommendation failed: {e}")

    # ========================
    # BEST TRACKING (NG WRAPPER)
    # ========================
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
