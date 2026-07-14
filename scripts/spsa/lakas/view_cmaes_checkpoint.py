#!/usr/bin/env python3
"""Inspect binary Nevergrad CMA checkpoints without overstating convergence."""

import argparse
import os
from pathlib import Path
import pathlib

import nevergrad as ng
import numpy as np


def parse_args():
    parser = argparse.ArgumentParser(
        description="Inspect binary Nevergrad CMA checkpoints and optimizer recommendations."
    )
    parser.add_argument("path", type=Path, help="A .dat file or directory containing .dat files.")
    parser.add_argument(
        "--show-best",
        action="store_true",
        help="Print Nevergrad's noisy xbest recommendation and current_bests diagnostics.",
    )
    return parser.parse_args()


def compatible_pickle_paths():
    if os.name == "nt":
        pathlib.PosixPath = pathlib.WindowsPath
    else:
        pathlib.WindowsPath = pathlib.PosixPath


def numeric_attr(obj, name):
    value = getattr(obj, name, None)
    if callable(value):
        try:
            value = value()
        except TypeError:
            return None
    return value if isinstance(value, (int, float, np.number)) else None


def optimizer_counter(opt, *names):
    for name in names:
        value = getattr(opt, name, None)
        if isinstance(value, int):
            return value
    return "n/a"


def covariance_condition(es, spectrum):
    condition = numeric_attr(es, "condition_number")
    if condition is not None:
        return float(condition)
    if spectrum.size == 0 or np.min(spectrum) <= 0:
        return None
    # pycma D values are square roots of covariance eigenvalues.
    return float((np.max(spectrum) / np.min(spectrum)) ** 2)


def cma_stop_conditions(es):
    stop = getattr(es, "stop", None)
    if not callable(stop):
        return None
    try:
        return dict(stop())
    except Exception as exc:
        return {"inspection_error": str(exc)}


def print_parameter(candidate):
    if candidate.kwargs:
        for name, value in candidate.kwargs.items():
            print(f"{name}: {value}")
    else:
        print(candidate.value)


def print_cma_center(opt, es):
    try:
        center = opt.parametrization.spawn_child().set_standardized_data(
            np.asarray(es.mean, dtype=float),
            reference=opt.parametrization,
        )
        print("\n--- decoded CMA distribution center ---")
        print("This is the optimizer-state candidate to validate first.")
        print_parameter(center)
    except Exception as exc:
        print(f"[!] CMA center decoding failed: {exc}")


def print_recommendation(opt):
    try:
        recommendation = opt.provide_recommendation()
        print("\n--- Nevergrad xbest recommendation ---")
        print("For CMA this is the lowest observed single-match result, not the distribution center.")
        print("Treat it as a winner's-curse diagnostic and do not promote it directly.")
        print_parameter(recommendation)
    except Exception as exc:
        print(f"[!] recommendation failed: {exc}")


def inspect_dat(dat_file, show_best):
    try:
        opt = ng.optimizers.base.Optimizer.load(str(dat_file))
    except Exception as exc:
        print(f"\n[!] Failed: {dat_file} -> {exc}")
        return False

    print("\n" + "#" * 80)
    print(f"FILE: {dat_file}")
    print("#" * 80)

    es = getattr(opt, "_es", None)
    if es is None:
        print("[!] No initialized CMA state found")
        return False

    asks = optimizer_counter(opt, "num_ask", "_num_ask")
    tells = optimizer_counter(opt, "num_tell", "_num_tell")
    tells_not_asked = optimizer_counter(
        opt,
        "num_tell_not_asked",
        "_num_tell_not_asked",
    )
    measured_tells = (
        tells - tells_not_asked
        if isinstance(tells, int) and isinstance(tells_not_asked, int)
        else "n/a"
    )
    outstanding_asks = (
        asks - measured_tells
        if isinstance(asks, int) and isinstance(measured_tells, int)
        else "n/a"
    )

    print("\n--- run counters ---")
    print(f"optimizer asks: {asks}")
    print(f"optimizer tells: {tells}")
    print(f"optimizer tells not asked: {tells_not_asked}")
    print(f"measured candidate tells: {measured_tells}")
    print(f"asked candidates not yet told: {outstanding_asks}")
    print(f"CMA iterations: {getattr(es, 'countiter', 'n/a')}")
    print(f"CMA evaluations: {getattr(es, 'countevals', 'n/a')}")
    population_size = getattr(es, "popsize", None)
    print(f"population size: {population_size if population_size is not None else 'n/a'}")
    cma_evaluations = getattr(es, "countevals", None)
    if (
        isinstance(cma_evaluations, int)
        and isinstance(measured_tells, int)
        and isinstance(population_size, int)
        and population_size > 0
    ):
        pending_population_tells = measured_tells - cma_evaluations
        if 0 <= pending_population_tells < population_size:
            print(
                "tells collected toward next CMA update: "
                f"{pending_population_tells}/{population_size}"
            )
            print(
                "CMA population state: "
                + (
                    "clean update boundary"
                    if pending_population_tells == 0
                    else "partial population"
                )
            )
        else:
            print(
                "CMA population progress: unavailable "
                f"(measured tells={measured_tells}, internal evaluations={cma_evaluations})"
            )
    if isinstance(outstanding_asks, int):
        print(
            "evaluation pipeline state: "
            + ("drained" if outstanding_asks == 0 else "jobs still outstanding")
        )

    try:
        result = es.result
        print("\n--- internal CMA fitness ---")
        print(f"lowest observed loss: {result.fbest}")
        print(f"evaluation index at lowest loss: {result.evals_best}")
        print("note: this is a noisy single match result, not a promotion candidate")
    except Exception as exc:
        print(f"[!] CMA fitness unavailable: {exc}")

    sigma = numeric_attr(es, "sigma")
    sigma0 = numeric_attr(es, "sigma0")
    print("\n--- step size ---")
    print(f"internal sigma: {sigma if sigma is not None else 'n/a'}")
    print(f"initial internal sigma: {sigma0 if sigma0 is not None else 'n/a'}")
    if sigma is not None and sigma0 not in (None, 0):
        print(f"sigma / sigma0: {sigma / sigma0}")
    print("note: sigma is in CMA's normalized coordinates, not engine parameter units")

    spectrum = np.asarray(getattr(es, "D", []), dtype=float)
    condition = covariance_condition(es, spectrum)
    print("\n--- covariance ---")
    if spectrum.size:
        print("sqrt eigenvalues D:", spectrum)
    else:
        print("sqrt eigenvalues D: n/a")
    print(f"covariance condition number: {condition if condition is not None else 'n/a'}")
    if condition is not None:
        if condition > 1e6:
            print("shape: extremely elongated")
        elif condition > 1e4:
            print("shape: strongly elongated")
        elif condition > 100:
            print("shape: moderately elongated")
        else:
            print("shape: well conditioned")

    print("\n--- internal normalized search state ---")
    if hasattr(es, "mean"):
        print("mean coordinates:", es.mean)
    if hasattr(es, "stds"):
        print("coordinate standard deviations:", es.stds)
    if hasattr(es, "pc"):
        try:
            print("evolution path norm ||pc||:", np.linalg.norm(es.pc))
        except Exception:
            print("evolution path pc:", es.pc)
    print_cma_center(opt, es)

    stop_conditions = cma_stop_conditions(es)
    print("\n--- CMA termination state ---")
    if stop_conditions is None:
        print("termination state unavailable")
    elif stop_conditions:
        for name, value in stop_conditions.items():
            print(f"{name}: {value}")
    else:
        print("no CMA stop condition has triggered")
    print("note: budget completion and statistical strength validation are separate decisions")

    if show_best:
        print_recommendation(opt)
        if hasattr(opt, "current_bests"):
            print("\n--- Nevergrad current_bests diagnostics ---")
            print(opt.current_bests)
    else:
        print("\nRun with --show-best to print the optimizer recommendation.")
    return True


def dat_files(root):
    if root.is_file():
        return [root] if root.suffix.lower() == ".dat" else []
    if not root.is_dir():
        return []
    return sorted(root.rglob("*.dat"))


def main():
    args = parse_args()
    compatible_pickle_paths()
    files = dat_files(args.path)
    if not files:
        raise SystemExit(f"no .dat files found under {args.path}")

    failures = 0
    for dat_file in files:
        if not inspect_dat(dat_file, args.show_best):
            failures += 1
    if failures:
        raise SystemExit(f"failed to inspect {failures} checkpoint(s)")


if __name__ == "__main__":
    main()
