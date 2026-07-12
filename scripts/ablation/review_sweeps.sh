#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export OUT="${OUT:-scripts/ablation/outputs/review_sweeps_10s}"

# Defines run_sweep and moves into the arena checkout.
source "$SCRIPT_DIR/../spsa/run_sweep.sh"

require_options() {
  local uci_output option
  uci_output="$(printf 'uci\nquit\n' | ./SHAYVERI)"
  for option in "$@"; do
    if ! grep -Fq "option name $option " <<< "$uci_output"; then
      echo "missing tuning option: $option" >&2
      echo "build an experimental engine with the review options enabled" >&2
      exit 2
    fi
  done
}

require_options \
  QS_Futility_Margin \
  History_Pruning_Threshold \
  History_Pruning_Min_Depth \
  History_Pruning_Min_Moves \
  IIR_Reduction \
  Check_Extension \
  SE_Extension \
  LMR_Extra_Reduction \
  LMR_Good_History_Reduction \
  PVS_SEE_Margin

# Newly meaningful after the qsearch and signed-threshold fixes.
run_sweep qs_futility_margin QS_Futility_Margin "50 100 150 200"

export RUN_SWEEP_EACH_OPTIONS="option.History_Pruning_Min_Depth=4 option.History_Pruning_Min_Moves=6"
run_sweep history_pruning_threshold History_Pruning_Threshold "-250 -500 -800 -1100"
unset RUN_SWEEP_EACH_OPTIONS

# Small discrete amounts: the base engine supplies the current value 1.
run_sweep iir_reduction IIR_Reduction "0 2"
run_sweep check_extension Check_Extension "0 2"
run_sweep se_extension SE_Extension "0 2"
run_sweep lmr_extra_reduction LMR_Extra_Reduction "0 2"
run_sweep lmr_good_history_reduction LMR_Good_History_Reduction "0 2"

# The base engine supplies the current signed margin -125.
run_sweep pvs_see_margin PVS_SEE_Margin "0 -75 -175"
