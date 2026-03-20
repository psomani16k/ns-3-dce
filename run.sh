#!/bin/bash

./waf --run "mptcp --sch=default"
./backup.sh "default_$1"
./waf --run "mptcp --sch=weighted_delay"
./backup.sh "weighted_delay_$1"
./waf --run "mptcp --sch=rr"
./backup.sh "rr_$1"
