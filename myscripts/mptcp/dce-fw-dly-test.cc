#include "ns3/applications-module.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/core-module.h"
#include "ns3/dce-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include <iostream>
#include <string>

using namespace ns3;

// #define MPTCP_RUNTIME 70
//
// #define TRAFFIC_BW "480K"
// #define TRAFFIC_START_DLY 10
//
// #define PATH_1_FWBW "6Mbps"
// #define PATH_1_FWDLY "50ms"
// bool PATH_1_FW_TRAFFIC = false;
//
// #define PATH_1_RVBW "500Kbps"
// #define PATH_1_RVDLY "50ms"
// bool PATH_1_RV_TRAFFIC = false;
//
// #define PATH_2_FWBW "4Mbps"
// #define PATH_2_FWDLY "80ms"
// bool PATH_2_FW_TRAFFIC = false;
//
// #define PATH_2_RVBW "500Kbps"
// #define PATH_2_RVDLY "10ms"
// bool PATH_2_RV_TRAFFIC = false;
//
// #define PATH_3_FWBW "8Mbps"
// #define PATH_3_FWDLY "10ms"
// bool PATH_3_FW_TRAFFIC = false;
//
// #define PATH_3_RVBW "500Kbps"
// #define PATH_3_RVDLY "250ms"
// bool PATH_3_RV_TRAFFIC = false;
//
// #define INFINITE_BW "100Mbps"
// #define INFINITE_DLY "1ms"
#define MPTCP_RUNTIME 60

// Cross-traffic: 820Kbps on a 1Mbps reverse bottleneck → ~95% utilisation (including ACKs),
// M/G/1 queuing ≈ 103ms delay for ACKs → RTT(P1_congested) ≈ 113ms.
// Starts 10 s into MPTCP so subflows are fully established before congestion hits.
#define TRAFFIC_BW "820Kbps"
#define TRAFFIC_START_DLY 10

// PATH 1: BEST forward path — high BW (10Mbps), lowest one-way delay (5ms).
//         Reverse bottleneck is 1Mbps; 820Kbps cross-traffic loads it to ~95%,
//         producing ~103ms queuing → RTT(P1_congested) ≈ 113ms.
//         weighted_delay sees fwd=5ms   → correctly PREFERS P1 (highest throughput).
//         minRTT      sees RTT ≈ 113ms → incorrectly AVOIDS P1.
#define PATH_1_FWBW "10Mbps"
#define PATH_1_FWDLY "5ms"
bool PATH_1_FW_TRAFFIC = false;

#define PATH_1_RVBW "1Mbps"
#define PATH_1_RVDLY "5ms"
bool PATH_1_RV_TRAFFIC = true;

// PATH 2: MEDIUM — 4Mbps forward, 35ms delay; clean reverse (5Mbps, 15ms).
//         RTT(P2) ≈ 50ms. Clear middle ground: minRTT prefers it over P1 but not P3;
//         weighted_delay uses it as second choice after P1.
#define PATH_2_FWBW "4Mbps"
#define PATH_2_FWDLY "35ms"
bool PATH_2_FW_TRAFFIC = false;

#define PATH_2_RVBW "5Mbps"
#define PATH_2_RVDLY "15ms"
bool PATH_2_RV_TRAFFIC = false;

// PATH 3: HIGH DELAY forward — 6Mbps BW, 80ms one-way delay.
//         Pristine reverse (100Mbps, 1ms) → RTT(P3) ≈ 81ms.
//         CWND = 81ms × 6Mbps / (1500B × 8) ≈ 40 segments — large enough to absorb real traffic.
//         minRTT      sees RTT ≈ 81ms  → PREFERS P3 over P1 (P1 RTT ≈ 113ms).
//         weighted_delay sees fwd=80ms → correctly AVOIDS P3 in favour of P1 (fwd=5ms).
//         This creates the measurable divergence: minRTT uses P2+P3 (~10Mbps),
//         weighted_delay uses P1+P2 (~14Mbps).
#define PATH_3_FWBW "6Mbps"
#define PATH_3_FWDLY "80ms"
bool PATH_3_FW_TRAFFIC = false;

#define PATH_3_RVBW "100Mbps"
#define PATH_3_RVDLY "1ms"
bool PATH_3_RV_TRAFFIC = false;

#define INFINITE_BW "100Mbps"
#define INFINITE_DLY "1ms"


#define N0 nodes.Get(0)       // 0
#define N1 nodes.Get(1)       // 1
#define R0 routers.Get(0)     // 2
#define R1 routers.Get(1)     // 3
#define R2 routers.Get(2)     // 4
#define R3 routers.Get(3)     // 5
#define R4 routers.Get(4)     // 6
#define R5 routers.Get(5)     // 7
#define Rtx01 routers.Get(6)  // 8
#define Rrx01 routers.Get(7)  // 9
#define Rtx10 routers.Get(8)  // 10
#define Rrx10 routers.Get(9)  // 11
#define Rtx23 routers.Get(10) // 12
#define Rrx23 routers.Get(11) // 13
#define Rtx32 routers.Get(12) // 14
#define Rrx32 routers.Get(13) // 15
#define Rtx45 routers.Get(14) // 16
#define Rrx45 routers.Get(15) // 17
#define Rtx54 routers.Get(16) // 18
#define Rrx54 routers.Get(17) // 19
#define T0 traffic.Get(0)     // 20
#define T1 traffic.Get(1)     // 21
#define T2 traffic.Get(2)     // 22
#define T3 traffic.Get(3)     // 23
#define T4 traffic.Get(4)     // 24
#define T5 traffic.Get(5)     // 25

void printTcpFlags(std::string key, std::string value) {
  std::cout << key << " = " << value << std::endl;
}

void setPos(Ptr<Node> n, int x, int y, int z) {
  Ptr<ConstantPositionMobilityModel> loc =
      CreateObject<ConstantPositionMobilityModel>();
  n->AggregateObject(loc);
  Vector locVec2(x, y + 200, z);
  loc->SetPosition(locVec2);
}

int main(int argc, char *argv[]) {
  // choosing congestion control algorithm
  std::string congestionControl = "lia";
  std::string scheduler = "default";
  bool sack = true;
  bool weak_host = false;
  CommandLine cmd;
  cmd.AddValue("cc", "congestion control algo. LIA default", congestionControl);

  cmd.AddValue("sch", "scheduler for mptcp. Options: default (minRTT), weighted_delay, roundrobin, rbp (RBP+minRTT), rbp_fwd (RBP+forward-delay)", scheduler);

  cmd.Parse(argc, argv);

  std::cout << scheduler << std::endl;

  DceManagerHelper dceManager;
  dceManager.SetTaskManagerAttribute("FiberManagerType",
                                     StringValue("UcontextFiberManager"));

  dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
                             StringValue("liblinux.so"));

  NodeContainer nodes, routers, traffic;
  nodes.Create(2);
  routers.Create(18);
  traffic.Create(6);

  LinuxStackHelper stack;
  stack.Install(nodes);
  stack.Install(routers);
  stack.Install(traffic);

  dceManager.Install(nodes);
  dceManager.Install(routers);
  dceManager.Install(traffic);

  PointToPointHelper pointToPoint;
  Ipv4AddressHelper r0addr, r1addr, r2addr, r3addr, r4addr, r5addr;

  r0addr.SetBase("10.0.0.0", "255.255.255.0");
  r1addr.SetBase("10.1.0.0", "255.255.255.0");
  r2addr.SetBase("10.2.0.0", "255.255.255.0");
  r3addr.SetBase("10.3.0.0", "255.255.255.0");
  r4addr.SetBase("10.4.0.0", "255.255.255.0");
  r5addr.SetBase("10.5.0.0", "255.255.255.0");

  // SETTING UP ROUTERS

  pointToPoint.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize",
                        StringValue("50p"));

  pointToPoint.SetDeviceAttribute("DataRate", StringValue(INFINITE_BW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(INFINITE_DLY));

  // Connecting N0 - R0
  NetDeviceContainer n0r0 = pointToPoint.Install(N0, R0);
  r0addr.Assign(n0r0);

  // Connecting N0 - R2
  NetDeviceContainer n0r2 = pointToPoint.Install(N0, R2);
  r2addr.Assign(n0r2);

  // Connecting N0 - R4
  NetDeviceContainer n0r4 = pointToPoint.Install(N0, R4);
  r4addr.Assign(n0r4);

  // Connecting N1 - R1
  NetDeviceContainer n1r1 = pointToPoint.Install(N1, R1);
  r1addr.Assign(n1r1);

  // connecting n1 - r3
  NetDeviceContainer n1r3 = pointToPoint.Install(N1, R3);
  r3addr.Assign(n1r3);

  // connecting n1 - r5
  NetDeviceContainer n1r5 = pointToPoint.Install(N1, R5);
  r5addr.Assign(n1r5);

  // Connecting T0 - R0
  NetDeviceContainer t0r0 = pointToPoint.Install(T0, R0);
  r0addr.Assign(t0r0);

  // Connecting T1 - R1
  NetDeviceContainer t1r1 = pointToPoint.Install(T1, R1);
  r1addr.Assign(t1r1);

  // Connecting T2 - R2
  NetDeviceContainer t2r2 = pointToPoint.Install(T2, R2);
  r2addr.Assign(t2r2);

  // Connecting T3 - R3
  NetDeviceContainer t3r3 = pointToPoint.Install(T3, R3);
  r3addr.Assign(t3r3);

  // Connecting T4 - R4
  NetDeviceContainer t4r4 = pointToPoint.Install(T4, R4);
  r4addr.Assign(t4r4);

  // Connecting T5 - R5
  NetDeviceContainer t5r5 = pointToPoint.Install(T5, R5);
  r5addr.Assign(t5r5);

  // Connecting Rtx01 - R0
  NetDeviceContainer rtx01r0 = pointToPoint.Install(Rtx01, R0);
  r0addr.Assign(rtx01r0);

  // Connecting Rrx10 - R0
  NetDeviceContainer rrx10r0 = pointToPoint.Install(Rrx10, R0);
  r0addr.Assign(rrx10r0);

  // Connecting Rtx23 - R2
  NetDeviceContainer rtx23r2 = pointToPoint.Install(Rtx23, R2);
  r2addr.Assign(rtx23r2);

  // Connecting Rrx32 - R2
  NetDeviceContainer rrx32r2 = pointToPoint.Install(Rrx32, R2);
  r2addr.Assign(rrx32r2);

  // Connecting Rtx45 - R4
  NetDeviceContainer rtx45r4 = pointToPoint.Install(Rtx45, R4);
  r4addr.Assign(rtx45r4);

  // Connecting Rrx54 - R4
  NetDeviceContainer rrx54r4 = pointToPoint.Install(Rrx54, R4);
  r4addr.Assign(rrx54r4);

  // Connecting Rtx10 - R1
  NetDeviceContainer rtx10r1 = pointToPoint.Install(Rtx10, R1);
  r1addr.Assign(rtx10r1);

  // Connecting Rrx01 - R1
  NetDeviceContainer rrx01r1 = pointToPoint.Install(Rrx01, R1);
  r1addr.Assign(rrx01r1);

  // Connecting Rtx32 - R3
  NetDeviceContainer rtx32r3 = pointToPoint.Install(Rtx32, R3);
  r3addr.Assign(rtx32r3);

  // Connecting Rrx23 - R3
  NetDeviceContainer rrx23r3 = pointToPoint.Install(Rrx23, R3);
  r3addr.Assign(rrx23r3);

  // Connecting Rtx54 - R5
  NetDeviceContainer rtx54r5 = pointToPoint.Install(Rtx54, R5);
  r5addr.Assign(rtx54r5);

  // Connecting Rrx45 - R5
  NetDeviceContainer rrx45r5 = pointToPoint.Install(Rrx45, R5);
  r5addr.Assign(rrx45r5);

  // Connecting Rtx01 - Rrx01
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(PATH_1_FWBW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(PATH_1_FWDLY));
  NetDeviceContainer rtx01rrx01 = pointToPoint.Install(Rtx01, Rrx01);
  r0addr.Assign(rtx01rrx01);

  // Connecting Rtx10 - Rrx10
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(PATH_1_RVBW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(PATH_1_RVDLY));
  NetDeviceContainer rtx10rrx10 = pointToPoint.Install(Rtx10, Rrx10);
  r1addr.Assign(rtx10rrx10);

  // Connecting Rtx23 - Rrx023
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(PATH_2_FWBW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(PATH_2_FWDLY));
  NetDeviceContainer rtx23rrx23 = pointToPoint.Install(Rtx23, Rrx23);
  r2addr.Assign(rtx23rrx23);

  // Connecting Rtx32 - Rrx32
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(PATH_2_RVBW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(PATH_2_RVDLY));
  NetDeviceContainer rtx32rrx32 = pointToPoint.Install(Rtx32, Rrx32);
  r3addr.Assign(rtx32rrx32);

  // Connecting Rtx45 - Rrx45
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(PATH_3_FWBW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(PATH_3_FWDLY));
  NetDeviceContainer rtx45rrx45 = pointToPoint.Install(Rtx45, Rrx45);
  r4addr.Assign(rtx45rrx45);

  // Connecting Rtx54 - Rrx54
  pointToPoint.SetDeviceAttribute("DataRate", StringValue(PATH_3_RVBW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(PATH_3_RVDLY));
  NetDeviceContainer rtx54rrx54 = pointToPoint.Install(Rtx54, Rrx54);
  r5addr.Assign(rtx54rrx54);

  if (weak_host) {
    // Filling routing table for N0
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add 10.1.0.1/32 via 10.0.0.1 dev sim0");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add 10.3.0.1/32 via 10.2.0.1 dev sim1");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add 10.5.0.1/32 via 10.4.0.1 dev sim2");

    // Filling routing table for N1
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add 10.0.0.1/32 via 10.1.0.1 dev sim0");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add 10.2.0.1/32 via 10.3.0.1 dev sim1");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add 10.4.0.1/32 via 10.5.0.1 dev sim2");
  } else {
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "rule add from 10.0.0.1 lookup 10");
    LinuxStackHelper::RunIp(N0, Seconds(0.2), "rule add to 10.1.0.1 lookup 10");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add default via 10.0.0.2 dev sim0 table 10");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "rule add from 10.2.0.1 lookup 20");
    LinuxStackHelper::RunIp(N0, Seconds(0.2), "rule add to 10.3.0.1 lookup 20");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add default via 10.2.0.2 dev sim1 table 20");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "rule add from 10.4.0.1 lookup 30");
    LinuxStackHelper::RunIp(N0, Seconds(0.2), "rule add to 10.5.0.1 lookup 30");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add default via 10.4.0.2 dev sim2 table 30");

    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "rule add from 10.1.0.1 lookup 10");
    LinuxStackHelper::RunIp(N1, Seconds(0.2), "rule add to 10.0.0.1 lookup 10");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add default via 10.1.0.2 dev sim0 table 10");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "rule add from 10.3.0.1 lookup 20");
    LinuxStackHelper::RunIp(N1, Seconds(0.2), "rule add to 10.2.0.1 lookup 20");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add default via 10.3.0.2 dev sim1 table 20");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "rule add from 10.5.0.1 lookup 30");
    LinuxStackHelper::RunIp(N1, Seconds(0.2), "rule add to 10.4.0.1 lookup 30");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add default via 10.5.0.2 dev sim2 table 30");
  }

  // Filling routing table for R0
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.0.0.1/32 via 10.0.0.2 dev sim0");
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.0.0.3/32 via 10.0.0.4 dev sim1");
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.1.0.1/32 via 10.0.0.6 dev sim2");
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.1.0.3/32 via 10.0.0.6 dev sim2");

  // Filling routing table for R1
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.1.0.1/32 via 10.1.0.2 dev sim0");
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.1.0.3/32 via 10.1.0.4 dev sim1");
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.0.0.1/32 via 10.1.0.6 dev sim2");
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.0.0.3/32 via 10.1.0.6 dev sim2");

  // Filling routing table for R2
  LinuxStackHelper::RunIp(R2, Seconds(0.2),
                          "route add 10.2.0.1/32 via 10.2.0.2 dev sim0");
  LinuxStackHelper::RunIp(R2, Seconds(0.2),
                          "route add 10.2.0.3/32 via 10.2.0.4 dev sim1");
  LinuxStackHelper::RunIp(R2, Seconds(0.2),
                          "route add 10.3.0.1/32 via 10.2.0.6 dev sim2");
  LinuxStackHelper::RunIp(R2, Seconds(0.2),
                          "route add 10.3.0.5/32 via 10.2.0.6 dev sim2");

  // Filling routing table for R3
  LinuxStackHelper::RunIp(R3, Seconds(0.2),
                          "route add 10.3.0.1/32 via 10.3.0.2 dev sim0");
  LinuxStackHelper::RunIp(R3, Seconds(0.2),
                          "route add 10.3.0.3/32 via 10.3.0.4 dev sim1");
  LinuxStackHelper::RunIp(R3, Seconds(0.2),
                          "route add 10.2.0.1/32 via 10.3.0.6 dev sim2");
  LinuxStackHelper::RunIp(R3, Seconds(0.2),
                          "route add 10.2.0.3/32 via 10.3.0.6 dev sim2");

  // Filling routing table for R4
  LinuxStackHelper::RunIp(R4, Seconds(0.2),
                          "route add 10.4.0.1/32 via 10.4.0.2 dev sim0");
  LinuxStackHelper::RunIp(R4, Seconds(0.2),
                          "route add 10.4.0.3/32 via 10.4.0.4 dev sim1");
  LinuxStackHelper::RunIp(R4, Seconds(0.2),
                          "route add 10.5.0.1/32 via 10.4.0.6 dev sim2");
  LinuxStackHelper::RunIp(R4, Seconds(0.2),
                          "route add 10.5.0.3/32 via 10.4.0.6 dev sim2");

  // Filling routing table for R5
  LinuxStackHelper::RunIp(R5, Seconds(0.2),
                          "route add 10.5.0.1/32 via 10.5.0.2 dev sim0");
  LinuxStackHelper::RunIp(R5, Seconds(0.2),
                          "route add 10.5.0.3/32 via 10.5.0.4 dev sim1");
  LinuxStackHelper::RunIp(R5, Seconds(0.2),
                          "route add 10.4.0.1/32 via 10.5.0.6 dev sim2");
  LinuxStackHelper::RunIp(R5, Seconds(0.2),
                          "route add 10.4.0.3/32 via 10.5.0.6 dev sim2");

  // Filling routing table for Rtx01
  LinuxStackHelper::RunIp(Rtx01, Seconds(0.2),
                          "route add 10.1.0.1/32 via 10.0.0.9 dev sim1");
  LinuxStackHelper::RunIp(Rtx01, Seconds(0.2),
                          "route add 10.1.0.3/32 via 10.0.0.9 dev sim1");

  // Filling routing table for Rrx01
  LinuxStackHelper::RunIp(Rrx01, Seconds(0.2),
                          "route add 10.1.0.1/32 via 10.1.0.7 dev sim0");
  LinuxStackHelper::RunIp(Rrx01, Seconds(0.2),
                          "route add 10.1.0.3/32 via 10.1.0.7 dev sim0");

  // Filling routing table for Rtx10
  LinuxStackHelper::RunIp(Rtx10, Seconds(0.2),
                          "route add 10.0.0.1/32 via 10.1.0.9 dev sim1");
  LinuxStackHelper::RunIp(Rtx10, Seconds(0.2),
                          "route add 10.0.0.3/32 via 10.1.0.9 dev sim1");

  // Filling routing table for Rrx10
  LinuxStackHelper::RunIp(Rrx10, Seconds(0.2),
                          "route add 10.0.0.1/32 via 10.0.0.7 dev sim0");
  LinuxStackHelper::RunIp(Rrx10, Seconds(0.2),
                          "route add 10.0.0.3/32 via 10.0.0.7 dev sim0");

  // Filling routing table for Rtx23
  LinuxStackHelper::RunIp(Rtx23, Seconds(0.2),
                          "route add 10.3.0.1/32 via 10.2.0.9 dev sim1");
  LinuxStackHelper::RunIp(Rtx23, Seconds(0.2),
                          "route add 10.3.0.3/32 via 10.2.0.9 dev sim1");

  // Filling routing table for Rrx32
  LinuxStackHelper::RunIp(Rrx32, Seconds(0.2),
                          "route add 10.3.0.1/32 via 10.3.0.7 dev sim0");
  LinuxStackHelper::RunIp(Rrx32, Seconds(0.2),
                          "route add 10.3.0.3/32 via 10.3.0.7 dev sim0");

  // Filling routing table for Rtx32
  LinuxStackHelper::RunIp(Rtx32, Seconds(0.2),
                          "route add 10.2.0.1/32 via 10.3.0.9 dev sim1");
  LinuxStackHelper::RunIp(Rtx32, Seconds(0.2),
                          "route add 10.2.0.3/32 via 10.3.0.9 dev sim1");

  // Filling routing table for Rrx32
  LinuxStackHelper::RunIp(Rrx32, Seconds(0.2),
                          "route add 10.2.0.1/32 via 10.2.0.7 dev sim0");
  LinuxStackHelper::RunIp(Rrx32, Seconds(0.2),
                          "route add 10.2.0.3/32 via 10.2.0.7 dev sim0");

  // Filling routing table for Rtx45
  LinuxStackHelper::RunIp(Rtx45, Seconds(0.2),
                          "route add 10.5.0.1/32 via 10.4.0.9 dev sim1");
  LinuxStackHelper::RunIp(Rtx45, Seconds(0.2),
                          "route add 10.5.0.3/32 via 10.4.0.9 dev sim1");

  // Filling routing table for Rrx54
  LinuxStackHelper::RunIp(Rrx54, Seconds(0.2),
                          "route add 10.5.0.1/32 via 10.5.0.7 dev sim0");
  LinuxStackHelper::RunIp(Rrx54, Seconds(0.2),
                          "route add 10.5.0.3/32 via 10.5.0.7 dev sim0");

  // Filling routing table for Rtx54
  LinuxStackHelper::RunIp(Rtx54, Seconds(0.2),
                          "route add 10.4.0.1/32 via 10.5.0.9 dev sim1");
  LinuxStackHelper::RunIp(Rtx54, Seconds(0.2),
                          "route add 10.4.0.3/32 via 10.5.0.9 dev sim1");

  // Filling routing table for Rrx54
  LinuxStackHelper::RunIp(Rrx54, Seconds(0.2),
                          "route add 10.4.0.1/32 via 10.4.0.7 dev sim0");
  LinuxStackHelper::RunIp(Rrx54, Seconds(0.2),
                          "route add 10.4.0.3/32 via 10.4.0.7 dev sim0");

  // Filling routing table for T0
  LinuxStackHelper::RunIp(T0, Seconds(0.2),
                          "route add 10.1.0.3/32 via 10.0.0.4 dev sim0");

  // Filling routing table for T1
  LinuxStackHelper::RunIp(T1, Seconds(0.2),
                          "route add 10.0.0.3/32 via 10.1.0.4 dev sim0");

  // Filling routing table for T2
  LinuxStackHelper::RunIp(T2, Seconds(0.2),
                          "route add 10.3.0.3/32 via 10.2.0.4 dev sim0");

  // Filling routing table for T3
  LinuxStackHelper::RunIp(T3, Seconds(0.2),
                          "route add 10.2.0.3/32 via 10.3.0.4 dev sim0");

  // Filling routing table for T4
  LinuxStackHelper::RunIp(T4, Seconds(0.2),
                          "route add 10.5.0.3/32 via 10.4.0.4 dev sim0");

  // Filling routing table for T5
  LinuxStackHelper::RunIp(T5, Seconds(0.2),
                          "route add 10.4.0.3/32 via 10.5.0.4 dev sim0");

  // debug
  stack.SysctlSet(nodes, ".net.mptcp.mptcp_debug", "1");
  stack.SysctlSet(nodes, ".net.ipv4.tcp_congestion_control", congestionControl);
  stack.SysctlSet(nodes, ".net.mptcp.mptcp_scheduler", scheduler);
  stack.SysctlSet(nodes, ".net.ipv4.tcp_sack", sack ? "1" : "0");

  LinuxStackHelper::SysctlGet(N0, Seconds(1),
                              ".net.ipv4.tcp_available_congestion_control",
                              &printTcpFlags);

  LinuxStackHelper::SysctlGet(
      N0, Seconds(1), ".net.ipv4.tcp_congestion_control", &printTcpFlags);

  LinuxStackHelper::SysctlGet(N0, Seconds(1), ".net.mptcp.mptcp_scheduler",
                              &printTcpFlags);

  LinuxStackHelper::SysctlGet(N0, Seconds(1), ".net.ipv4.tcp_sack",
                              &printTcpFlags);

  // data applications
  DceApplicationHelper dce;
  ApplicationContainer apps;

  int mptcp_run_time = MPTCP_RUNTIME;
  int mptcp_start = 30;
  int mptcp_stop = mptcp_start + mptcp_run_time + 5;
  int traffic_start = mptcp_start + TRAFFIC_START_DLY;
  int traffic_stop = mptcp_stop;

  dce.SetStackSize(1 << 24);

  // Launch iperf client on node 0
  dce.SetBinary("iperf");
  dce.ResetArguments();
  dce.ResetEnvironment();
  dce.AddArgument("-c");
  dce.AddArgument("10.1.0.1");
  dce.AddArgument("-i");
  dce.AddArgument("1");
  dce.AddArgument("--time");
  dce.AddArgument(std::to_string(mptcp_run_time));

  apps = dce.Install(N0);
  apps.Start(Seconds(mptcp_start));
  apps.Stop(Seconds(mptcp_stop));

  // Launch iperf server on node 1
  dce.SetBinary("iperf");
  dce.ResetArguments();
  dce.ResetEnvironment();
  dce.AddArgument("-s");
  dce.AddArgument("-P");
  dce.AddArgument("1");
  apps = dce.Install(N1);
  apps.Start(Seconds(mptcp_start));

  if (PATH_1_RV_TRAFFIC) {
    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-c");
    dce.AddArgument("10.0.0.3");
    dce.AddArgument("-b");
    dce.AddArgument(TRAFFIC_BW);

    apps = dce.Install(T1);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));

    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-s");

    apps = dce.Install(T0);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));
  }
  if (PATH_1_FW_TRAFFIC) {
    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-c");
    dce.AddArgument("10.1.0.3");
    dce.AddArgument("-b");
    dce.AddArgument(TRAFFIC_BW);

    apps = dce.Install(T0);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));

    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-s");

    apps = dce.Install(T1);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));
  }
  if (PATH_2_RV_TRAFFIC) {
    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-c");
    dce.AddArgument("10.2.0.3");
    dce.AddArgument("-b");
    dce.AddArgument(TRAFFIC_BW);

    apps = dce.Install(T3);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));

    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-s");

    apps = dce.Install(T2);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));
  }
  if (PATH_2_FW_TRAFFIC) {
    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-c");
    dce.AddArgument("10.3.0.3");
    dce.AddArgument("-b");
    dce.AddArgument(TRAFFIC_BW);

    apps = dce.Install(T2);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));

    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-s");

    apps = dce.Install(T3);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));
  }
  if (PATH_3_RV_TRAFFIC) {
    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-c");
    dce.AddArgument("10.4.0.3");
    dce.AddArgument("-b");
    dce.AddArgument(TRAFFIC_BW);

    apps = dce.Install(T5);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));

    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-s");

    apps = dce.Install(T4);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));
  }
  if (PATH_3_FW_TRAFFIC) {
    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-c");
    dce.AddArgument("10.5.0.3");
    dce.AddArgument("-b");
    dce.AddArgument(TRAFFIC_BW);

    apps = dce.Install(T4);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));

    dce.SetBinary("iperf");
    dce.ResetArguments();
    dce.ResetEnvironment();
    dce.AddArgument("-u");
    dce.AddArgument("-s");

    apps = dce.Install(T5);
    apps.Start(Seconds(traffic_start));
    apps.Stop(Seconds(traffic_stop));
  }

  setPos(N0, 0, 0, 0);
  setPos(N1, 400, 0, 0);

  setPos(R0, 100, -100, 0);
  setPos(R1, 300, -100, 0);
  setPos(R2, 100, 0, 0);
  setPos(R3, 300, 0, 0);
  setPos(R4, 100, 100, 0);
  setPos(R5, 300, 100, 0);

  setPos(Rtx01, 150, -125, 0);
  setPos(Rrx01, 250, -125, 0);
  setPos(Rrx10, 150, -75, 0);
  setPos(Rtx10, 250, -75, 0);
  setPos(Rtx23, 150, -25, 0);
  setPos(Rrx23, 250, -25, 0);
  setPos(Rrx32, 150, 25, 0);
  setPos(Rtx32, 250, 25, 0);
  setPos(Rtx45, 150, 75, 0);
  setPos(Rrx45, 250, 75, 0);
  setPos(Rrx54, 150, 125, 0);
  setPos(Rtx54, 250, 125, 0);

  setPos(T0, 90, -120, 0);
  setPos(T1, 310, -120, 0);
  setPos(T2, 90, -20, 0);
  setPos(T3, 310, -20, 0);
  setPos(T4, 90, 80, 0);
  setPos(T5, 310, 80, 0);

  AnimationInterface anim("fw.xml");
  anim.SetMaxPktsPerTraceFile(1 * 10000000);
  anim.EnablePacketMetadata(true);

  anim.UpdateNodeColor(N0->GetId(), 0, 255, 0);
  anim.UpdateNodeSize(N0->GetId(), 8.0, 8.0);
  anim.UpdateNodeColor(N1->GetId(), 0, 255, 0);
  anim.UpdateNodeSize(N1->GetId(), 8.0, 8.0);

  anim.UpdateNodeColor(R0->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(R0->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(R1->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(R1->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(R2->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(R2->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(R3->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(R3->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(R4->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(R4->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(R5->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(R5->GetId(), 6.0, 6.0);

  anim.UpdateNodeColor(Rrx01->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rrx01->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rtx01->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rtx01->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rrx10->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rrx10->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rtx10->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rtx10->GetId(), 4.0, 4.0);

  anim.UpdateNodeColor(Rrx23->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rrx23->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rtx23->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rtx23->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rrx32->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rrx32->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rtx32->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rtx32->GetId(), 4.0, 4.0);

  anim.UpdateNodeColor(Rrx45->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rrx45->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rtx45->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rtx45->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rrx54->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rrx54->GetId(), 4.0, 4.0);
  anim.UpdateNodeColor(Rtx54->GetId(), 255, 0, 0);
  anim.UpdateNodeSize(Rtx54->GetId(), 4.0, 4.0);

  anim.UpdateNodeColor(T0->GetId(), 0, 0, 255);
  anim.UpdateNodeSize(T0->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(T1->GetId(), 0, 0, 255);
  anim.UpdateNodeSize(T1->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(T2->GetId(), 0, 0, 255);
  anim.UpdateNodeSize(T2->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(T3->GetId(), 0, 0, 255);
  anim.UpdateNodeSize(T3->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(T4->GetId(), 0, 0, 255);
  anim.UpdateNodeSize(T4->GetId(), 6.0, 6.0);
  anim.UpdateNodeColor(T5->GetId(), 0, 0, 255);
  anim.UpdateNodeSize(T5->GetId(), 6.0, 6.0);

  pointToPoint.EnablePcapAll("fw_dly_test", false);

  Simulator::Stop(Seconds(mptcp_stop + 5));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
