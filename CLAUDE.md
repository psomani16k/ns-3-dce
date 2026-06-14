# CLAUDE.md — ns-3-dce MPTCP Research Project

## Project Overview

This is a **thesis research project** investigating MPTCP (Multipath TCP) subflow scheduling under asymmetric network conditions. The stack has two layers:

1. **ns-3-dce** (this repo) — the simulation environment. DCE (Direct Code Execution) lets ns-3 run real Linux kernel network stacks as shared libraries inside the simulator, so protocol implementations run unmodified.
2. **net-next-nuse-mptcp** (`../net-next-nuse-mptcp/`) — the Linux kernel MPTCP implementation compiled as a userspace library (`liblinux.so`). This is what actually runs inside DCE nodes.

The active research topic is **forward-delay-aware scheduling**: testing whether a scheduler that uses one-way (forward) delay instead of RTT makes better subflow selections when reverse paths are asymmetrically congested.

---

## Repository Layout

```
ns-3-dce/
├── myscripts/mptcp/          # ← Main simulation scripts (edit here)
│   ├── dce-fw-dly-test.cc    # ← ACTIVE simulation: 3-path asymmetric topology
│   ├── dce-iperf-mptcp.cc    # Alternative: iperf-based MPTCP test
│   ├── dce-iperf-multi.cc    # Alternative: multi-flow iperf test
│   └── wscript               # WAF build config (selects which .cc to compile)
├── example/                  # Upstream DCE examples (reference only)
├── model/                    # DCE core runtime (kernel socket emulation, fd mgmt)
├── helper/                   # ns-3 helper classes (DceManagerHelper, LinuxStackHelper)
├── test/                     # DCE unit/integration tests
├── run.sh                    # Runs default + weighted_delay schedulers, backs up results
├── backup.sh                 # Moves files-*, *.pcap, *.xml to timestamped results_backup/
└── print.sh                  # Dumps logs from all 26 DCE node directories (files-0..files-25)
```

---

## Build System

Uses **WAF** (Python-based build tool, similar to scons/cmake).

### Configure (one-time, done via bake)
```bash
./waf configure --with-ns3=/path/to/ns3 --enable-kernel-stack=/path/to/nuse
```

### Build the active simulation
```bash
./waf build
```

### Run a simulation
```bash
./waf --run "mptcp --sch=default"           # RTT-based (minRTT) scheduler
./waf --run "mptcp --sch=weighted_delay"    # Forward-delay-aware scheduler (custom)
./waf --run "mptcp --sch=roundrobin"        # Round-robin scheduler
```

### Switch which simulation file is compiled
Edit `myscripts/mptcp/wscript` — change the `source=[...]` line:
```python
source=['dce-fw-dly-test.cc'],      # active
# source=['dce-iperf-mptcp.cc'],    # iperf variant
# source=['dce-iperf-multi.cc'],    # multi-flow variant
```

---

## Active Simulation: `dce-fw-dly-test.cc`

### Topology

26 nodes total:
- **N0, N1**: Client and server (run MPTCP via DCE)
- **R0–R5**: 6 intermediate routers forming 3 parallel paths
- **Rtx01, Rrx01, Rtx10, Rrx10, Rtx23, Rrx23, Rtx32, Rrx32**: Traffic injection routers (8 total) — inject UDP cross-traffic to simulate congestion
- **Traffic generator nodes**: UDP flood sources on reverse paths

### 3-Path Network Design

The topology is specifically engineered so that RTT-based scheduling makes **wrong** choices:

| Path | Forward BW | Fwd Delay | Reverse BW  | Rev Delay | Cross-traffic | Notes |
|------|------------|-----------|-------------|-----------|---------------|-------|
| P1   | 8 Mbps     | 15 ms     | 300 Kbps    | 20 ms     | YES (UDP)     | Best forward; reverse congested → ACKs queue → RTT balloons |
| P2   | 3 Mbps     | 70 ms     | 5 Mbps      | 15 ms     | No            | Medium forward; clean reverse |
| P3   | 1.5 Mbps   | 40 ms     | 5 Mbps      | 5 ms      | No            | Worst forward BW/delay; pristine reverse → minRTT incorrectly favors this |

**Key insight**: minRTT scheduler over-uses P3 (sees low RTT due to clean reverse) and under-uses P1 (sees high RTT due to congested reverse), even though P1 has the best forward characteristics. The `weighted_delay` scheduler uses `sfw_dly_us` (forward delay) to make better decisions.

### Simulation Parameters
```cpp
#define MPTCP_RUNTIME 35        // seconds
#define TRAFFIC_BW "480Kbps"    // cross-traffic bandwidth
#define TRAFFIC_START_DLY 10    // seconds before cross-traffic starts
```

### Output Files
- `files-*/` — DCE process filesystems (logs, pcap per node)
- `*.pcap` — packet captures on each link
- `*.xml` — NetAnim animation files

---

## MPTCP Implementation (`../net-next-nuse-mptcp/net/mptcp/`)

### Schedulers (subflow selection)

| File | Scheduler | `--sch=` argument | Description |
|------|-----------|-------------------|-------------|
| `mptcp_sched.c` | Default (minRTT) | `default` | Selects subflow with lowest SRTT |
| `mptcp_wt_dly.c` | Weighted Delay | `weighted_delay` | Selects by forward delay (`sfw_dly_us`); the custom scheduler under study |
| `mptcp_rr.c` | Round Robin | `roundrobin` | Quota-based round-robin across subflows |

#### Weighted Delay Scheduler (`mptcp_wt_dly.c`)
- **Selection metric**: `wt_dly = 1*fw_dly + 0*rv_dly` (forward delay only)
- **Key field**: `tp->sfw_dly_us` — one-way forward delay estimate
- **Preference**: unused subflows first, then lowest forward delay among used ones; skips backup/low-priority subflows
- **Goal**: Avoid penalizing a subflow for having a congested reverse path (ACK path), which RTT-based schedulers incorrectly do

### Congestion Control Algorithms

| File | Algorithm | Description |
|------|-----------|-------------|
| `mptcp_coupled.c` | LIA | Linked Increase Algorithm (original MPTCP CC) |
| `mptcp_olia.c` | OLIA | Opportunistic Linked Increases (EPFL) |
| `mptcp_balia.c` | BALIA | Balanced Linked Adaptation (Caltech/Bell Labs) |
| `mptcp_wvegas.c` | Weighted Vegas | Vegas-based with coupled alpha (Tsinghua) |

### Path Managers

| File | Manager | Description |
|------|---------|-------------|
| `mptcp_fullmesh.c` | fullmesh | Creates subflows between all address pairs |
| `mptcp_ndiffports.c` | ndiffports | Multiple subflows via different source ports |
| `mptcp_binder.c` | binder | Gateway-based with loose source routing |

### Core Protocol Files
- `mptcp_ctrl.c` — Connection setup/teardown, socket operations
- `mptcp_input.c` — Receive side: packet processing, ACK handling, subflow demux
- `mptcp_output.c` — Send side: packet scheduling, subflow selection integration
- `mptcp_ofo_queue.c` — Out-of-order receive queue management
- `mptcp_pm.c` — Path manager plugin interface
- `mptcp_ipv4.c` / `mptcp_ipv6.c` — IP-version-specific operations

---

## Common Workflows

### Run both scheduler conditions and save results
```bash
./run.sh "experiment_label"
# → runs default scheduler, backs up to results_backup/default_experiment_label_<timestamp>/
# → runs weighted_delay scheduler, backs up similarly
```

### Manual run + backup
```bash
./waf --run "mptcp --sch=weighted_delay"
./backup.sh "my_label"
```

### Inspect simulation logs
```bash
./print.sh           # dumps logs from all 26 files-N/var/log/ directories
```

### Check what a node sent/received
```bash
ls files-0/          # DCE filesystem for node 0 (client)
ls files-1/          # node 1 (server)
```

---

## Key Design Decisions & Research Context

- **Why asymmetric reverse paths?** RTT = forward delay + reverse delay. If the reverse (ACK) path is congested, RTT rises even when the forward data path is excellent. RTT-based schedulers mistakenly deprioritize these subflows.
- **Forward delay estimation**: `sfw_dly_us` in the kernel's tcp_sock tracks one-way forward delay. The weighted delay scheduler uses this instead of SRTT to avoid reverse-path pollution.
- **DCE vs. emulation**: DCE runs the real kernel MPTCP code in simulation, giving reproducibility and ground truth that emulation cannot provide.
- **Bake build system**: The outer `bake/` directory manages fetching and building all components (ns-3, ns-3-dce, net-next-nuse-mptcp). Do not manually build components out of bake context.

---

## Files NOT to Edit (upstream/generated)

- `model/` — upstream DCE runtime; changes here require rebuilding the whole DCE framework
- `helper/` — upstream DCE helpers
- `example/` — upstream DCE examples (use `myscripts/` for custom simulations)
- `*.o`, `.*.cmd`, `builtin.o` — build artifacts
- `objdir/`, `build/` — WAF output directories
