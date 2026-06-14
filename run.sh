#!/bin/bash
# Run all scheduler conditions for comparison and back up results.
# Usage: ./run.sh <experiment_label>
# Results saved to results_backup/<scheduler>_<label>_<timestamp>/

LABEL=${1:-"run"}

echo "=== 1/4: default (minRTT) ==="
./waf --run "mptcp --sch=default"
./backup.sh "default_${LABEL}"

echo "=== 2/4: weighted_delay (forward-delay) ==="
./waf --run "mptcp --sch=weighted_delay"
./backup.sh "weighted_delay_${LABEL}"

# echo "=== 3/4: rbp (RBP + minRTT) ==="
# ./waf --run "mptcp --sch=rbp"
# ./backup.sh "rbp_${LABEL}"
#
# echo "=== 4/4: rbp_fwd (RBP + forward-delay) ==="
# ./waf --run "mptcp --sch=rbp_fwd"
# ./backup.sh "rbp_fwd_${LABEL}"
#
echo "=== Done. All results backed up under results_backup/ ==="
