/*
          +------+ +------+ |  T0  | |  T1  |
          +------+ +------+
                  \            +---------+     10Mbps, 1ms        +---------+ /
       10Mbps, 1ms \           | Rtx0->1 |>>>>>>>>>>>>>>>>>>>>>>>>| Rrx0->1 | /
   10Mbps, 1ms
                    \          +---------+                        +---------+ /
                     \        / X                                          X \ /
                      +------+ +------+ |  R0  | |  R1  |
                      +------+ +------+
                     /        \ X                                          X / \
                  X /          +---------+     10Mbps, 100ms      +---------+
   \ X
                   /           | Rrx1->0 |<<<<<<<<<<<<<<<<<<<<<<<<| Rtx1->0 | \
           +------+            +---------+                        +---------+
   +------+ |  N0  | |  N1  |
           +------+            +---------+     10Mbps, 100ms      +---------+
   +------+
                   \           | Rtx2->3 |>>>>>>>>>>>>>>>>>>>>>>>>| Rrx2->3 | /
                  X \          +---------+                        +---------+ /
   X
                     \        / X                                          X \ /
                      +------+ +------+ |  R2  | |  R3  |
                      +------+ +------+
                     /        \ X                                          X / \
                    /          +---------+     10Mbps, 1ms        +---------+ \
       10Mbps, 1ms /           | Rrx3->2 |<<<<<<<<<<<<<<<<<<<<<<<<| Rtx3->2 |
   \ 10Mbps, 1ms
                  /            +---------+                        +---------+ \
          +------+ +------+ |  T2  | |  T3  |
          +------+ +------+

          X = 100Mbps, 1ms
*/

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

#define PATH_1_FWBW "5Mbps"
#define PATH_1_FWDLY "100ms" // 10.0.0.1 - 10.1.0.1

#define PATH_1_RVBW "5Mbps"
#define PATH_1_RVDLY "100ms" // 10.1.0.1 - 10.0.0.1

#define PATH_2_FWBW "5Mbps"
#define PATH_2_FWDLY "100ms"

#define PATH_2_RVBW "5Mbps"
#define PATH_2_RVDLY "500ms"

#define INFINITE_BW "100Mbps"
#define INFINITE_DLY "1ms"

#define N0 nodes.Get(0)
#define N1 nodes.Get(1)

#define T0 traffic.Get(0)
#define T1 traffic.Get(1)
#define T2 traffic.Get(2)
#define T3 traffic.Get(3)

#define R0 routers.Get(0)
#define R1 routers.Get(1)
#define R2 routers.Get(2)
#define R3 routers.Get(3)

#define Rtx01 routers.Get(4)
#define Rrx01 routers.Get(5)
#define Rtx10 routers.Get(6)
#define Rrx10 routers.Get(7)
#define Rtx23 routers.Get(8)
#define Rrx23 routers.Get(9)
#define Rtx32 routers.Get(10)
#define Rrx32 routers.Get(11)

void printTcpFlags(std::string key, std::string value) {
  std::cout << key << " = " << value << std::endl;
}

void setPos(Ptr<Node> n, int x, int y, int z) {
  Ptr<ConstantPositionMobilityModel> loc =
      CreateObject<ConstantPositionMobilityModel>();
  n->AggregateObject(loc);
  Vector locVec2(x, y + 100, z);
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

  cmd.AddValue("sch", "schedular for mptcp. minRTT default", scheduler);

  cmd.Parse(argc, argv);

  std::cout << scheduler << std::endl;

  DceManagerHelper dceManager;
  dceManager.SetTaskManagerAttribute("FiberManagerType",
                                     StringValue("UcontextFiberManager"));

  dceManager.SetNetworkStack("ns3::LinuxSocketFdFactory", "Library",
                             StringValue("liblinux.so"));

  NodeContainer nodes, routers, traffic;
  nodes.Create(2);
  routers.Create(12);
  traffic.Create(4);

  LinuxStackHelper stack;
  stack.Install(nodes);
  stack.Install(routers);
  stack.Install(traffic);

  dceManager.Install(nodes);
  dceManager.Install(routers);
  dceManager.Install(traffic);

  PointToPointHelper pointToPoint;
  Ipv4AddressHelper r0addr, r1addr, r2addr, r3addr;

  r0addr.SetBase("10.0.0.0", "255.255.255.0");
  r1addr.SetBase("10.1.0.0", "255.255.255.0");
  r2addr.SetBase("10.2.0.0", "255.255.255.0");
  r3addr.SetBase("10.3.0.0", "255.255.255.0");

  // SETTING UP ROUTERS

  pointToPoint.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize",
                        StringValue("100p"));

  pointToPoint.SetDeviceAttribute("DataRate", StringValue(INFINITE_BW));
  pointToPoint.SetChannelAttribute("Delay", StringValue(INFINITE_DLY));

  // Connecting N0 - R0
  NetDeviceContainer n0r0 = pointToPoint.Install(N0, R0);
  r0addr.Assign(n0r0);

  // Connecting N0 - R2
  NetDeviceContainer n0r2 = pointToPoint.Install(N0, R2);
  r2addr.Assign(n0r2);

  // Connecting N1 - R1
  NetDeviceContainer n1r1 = pointToPoint.Install(N1, R1);
  r1addr.Assign(n1r1);

  // Connecting N1 - R1
  NetDeviceContainer n1r3 = pointToPoint.Install(N1, R3);
  r3addr.Assign(n1r3);

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

  if (weak_host) {
    // Filling routing table for N0
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add 10.1.0.1/32 via 10.0.0.1 dev sim0");
    LinuxStackHelper::RunIp(N0, Seconds(0.2),
                            "route add 10.3.0.1/32 via 10.2.0.1 dev sim1");

    // Filling routing table for N1
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add 10.0.0.1/32 via 10.1.0.1 dev sim0");
    LinuxStackHelper::RunIp(N1, Seconds(0.2),
                            "route add 10.2.0.1/32 via 10.3.0.1 dev sim1");
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

    // Policy-Based Routing for N1 (Receiver)
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
  }

  // Filling routing table for R0
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.0.0.1/32 via 10.0.0.2 dev sim0");
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.0.0.3/32 via 10.0.0.4 dev sim1");
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.1.0.1/32 via 10.0.0.6 dev sim2");
  LinuxStackHelper::RunIp(R0, Seconds(0.2),
                          "route add 10.1.0.5/32 via 10.0.0.6 dev sim2");

  // Filling routing table for R1
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.0.0.1/32 via 10.1.0.6 dev sim2");
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.0.0.3/32 via 10.1.0.6 dev sim2");
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.1.0.1/32 via 10.1.0.2 dev sim0");
  LinuxStackHelper::RunIp(R1, Seconds(0.2),
                          "route add 10.1.0.3/32 via 10.1.0.4 dev sim1");

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
                          "route add 10.2.0.1/32 via 10.3.0.6 dev sim2");
  LinuxStackHelper::RunIp(R3, Seconds(0.2),
                          "route add 10.2.0.3/32 via 10.3.0.6 dev sim2");
  LinuxStackHelper::RunIp(R3, Seconds(0.2),
                          "route add 10.3.0.1/32 via 10.3.0.2 dev sim0");
  LinuxStackHelper::RunIp(R3, Seconds(0.2),
                          "route add 10.3.0.3/32 via 10.3.0.4 dev sim1");

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

  // Filling routing table for Rrx01
  LinuxStackHelper::RunIp(Rrx01, Seconds(0.2),
                          "route add 10.3.0.1/32 via 10.3.0.7 dev sim0");
  LinuxStackHelper::RunIp(Rrx01, Seconds(0.2),
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

  // debug
  stack.SysctlSet(nodes, ".net.mptcp.mptcp_debug", "1");
  stack.SysctlSet(nodes, ".net.ipv4.tcp_congestion_control", congestionControl);
  stack.SysctlSet(nodes, ".net.mptcp.mptcp_scheduler", scheduler);
  stack.SysctlSet(nodes, ".net.ipv4.tcp_sack", sack ? "1" : "0");

  LinuxStackHelper::SysctlGet(N0, Seconds(1),
                              ".net.ipv4.tcp_available_congestion_control",
                              &printTcpFlags);

  LinuxStackHelper::SysctlGet(N0, Seconds(1),
                              ".net.ipv4.tcp_congestion_control",
                              &printTcpFlags);

  LinuxStackHelper::SysctlGet(N0, Seconds(1),
                              ".net.mptcp.mptcp_scheduler", &printTcpFlags);

  LinuxStackHelper::SysctlGet(N0, Seconds(1), ".net.ipv4.tcp_sack",
                              &printTcpFlags);

  // data applications
  DceApplicationHelper dce;
  ApplicationContainer apps;

  int mptcp_start = 30;
  int run_time = 50;
  int mptcp_stop = mptcp_start + run_time + 1;

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
  dce.AddArgument(std::to_string(run_time));

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

  setPos(N0, 0, 0, 0);
  setPos(N1, 200, 0, 0);

  setPos(R0, 50, 50, 0);
  setPos(R1, 150, 50, 0);
  setPos(R2, 50, -50, 0);
  setPos(R3, 150, -50, 0);

  setPos(Rtx01, 75, 75, 0);
  setPos(Rrx01, 125, 75, 0);
  setPos(Rrx10, 75, 25, 0);
  setPos(Rtx10, 125, 25, 0);
  setPos(Rtx23, 75, -75, 0);
  setPos(Rrx23, 125, -75, 0);
  setPos(Rrx32, 75, -25, 0);
  setPos(Rtx32, 125, -25, 0);

  setPos(T0, 0, 75, 0);
  setPos(T1, 200, 75, 0);
  setPos(T2, 0, -75, 0);
  setPos(T3, 200, -75, 0);

  AnimationInterface anim("fw.xml");
  anim.SetMaxPktsPerTraceFile(1 * 10000000);
  anim.EnablePacketMetadata(true);

  // anim.UpdateNodeColor(nodes.Get(0)->GetId(), 0, 255, 0);
  // anim.UpdateNodeSize(nodes.Get(0)->GetId(), 5.0, 5.0);
  // anim.UpdateNodeColor(nodes.Get(1)->GetId(), 0, 255, 0);
  // anim.UpdateNodeSize(nodes.Get(1)->GetId(), 5.0, 5.0);
  //
  // anim.UpdateNodeColor(routers.Get(0)->GetId(), 255, 0, 0);
  // anim.UpdateNodeSize(routers.Get(0)->GetId(), 4.0, 4.0);
  // anim.UpdateNodeColor(routers.Get(1)->GetId(), 255, 0, 0);
  // anim.UpdateNodeSize(routers.Get(1)->GetId(), 4.0, 4.0);
  // anim.UpdateNodeColor(routers.Get(2)->GetId(), 255, 0, 0);
  // anim.UpdateNodeSize(routers.Get(2)->GetId(), 4.0, 4.0);
  // anim.UpdateNodeColor(routers.Get(3)->GetId(), 255, 0, 0);
  // anim.UpdateNodeSize(routers.Get(3)->GetId(), 4.0, 4.0);
  //
  // anim.UpdateNodeColor(traffic.Get(0)->GetId(), 0, 0, 255);
  // anim.UpdateNodeSize(traffic.Get(0)->GetId(), 3.0, 3.0);
  // anim.UpdateNodeColor(traffic.Get(1)->GetId(), 0, 0, 255);
  // anim.UpdateNodeSize(traffic.Get(1)->GetId(), 3.0, 3.0);
  // anim.UpdateNodeColor(traffic.Get(2)->GetId(), 0, 0, 255);
  // anim.UpdateNodeSize(traffic.Get(2)->GetId(), 3.0, 3.0);
  // anim.UpdateNodeColor(traffic.Get(3)->GetId(), 0, 0, 255);
  // anim.UpdateNodeSize(traffic.Get(3)->GetId(), 3.0, 3.0);

  pointToPoint.EnablePcapAll("fw_dly_test", false);

  Simulator::Stop(Seconds(mptcp_stop));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
