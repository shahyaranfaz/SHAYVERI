#!/usr/bin/env bash
# Shared branch defaults for net13.
#
# Token meanings:
#   N = net5 baseline lane
#   H = HCE baseline lane
#   D = net5 deep-hard lane
#   E = HCE deep-hard lane, relabelled by net5
#
# These patterns encode the branch matrix in net13results.txt.
# Worker and master should source this file with the same BRANCH value.

case "${BRANCH:-13A}" in
  13A)
    : "${NFS_ROOT:=$HOME/nnue_v2_13/13A}"
    : "${TRAIN_ID:=shayveri_v2.13A}"
    : "${NET13_PATTERN:=NNNNHHDDDE}"
    : "${LR:=0.00001}"
    ;;
  13B)
    : "${NFS_ROOT:=$HOME/nnue_v2_13/13B}"
    : "${TRAIN_ID:=shayveri_v2.13B}"
    : "${NET13_PATTERN:=NNNNHHDDDE}"
    : "${LR:=0.00003}"
    ;;
  13C)
    : "${NFS_ROOT:=$HOME/nnue_v2_13/13C}"
    : "${TRAIN_ID:=shayveri_v2.13C}"
    : "${NET13_PATTERN:=NNNNHDDDDD}"
    : "${LR:=0.00001}"
    ;;
  13D)
    : "${NFS_ROOT:=$HOME/nnue_v2_13/13D}"
    : "${TRAIN_ID:=shayveri_v2.13D}"
    : "${NET13_PATTERN:=NNNNNNNNHHHHHHHDDEEE}"
    : "${LR:=0.00001}"
    ;;
  *)
    echo "unknown BRANCH=${BRANCH}; expected 13A, 13B, 13C, or 13D" >&2
    return 2 2>/dev/null || exit 2
    ;;
esac

export NFS_ROOT TRAIN_ID NET13_PATTERN LR
