/*
 * phase1.cc
 * Phase 1: Baseline Wi-Fi 5 (802.11ac) simulation with 1 AP and 5 STAs.
 * Measures throughput, delay, packet loss, and Jain's fairness index.
 *
 * Usage:
 * ./ns3 run phase1                # default noise figure = 1.0 dB
 * ./ns3 run "phase1 --noise=7.0"  # increase noise to 7.0 dB
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

// ========== Simulation parameters (easily tunable) ==========
constexpr int NUM_STA = 5;                // number of stations
constexpr double SIMULATION_TIME = 10.0;      // seconds
constexpr double AP_STA_DISTANCE = 5.0;       // meters (radius)
constexpr double PACKET_INTERVAL = 0.5;       // seconds between packets
constexpr uint16_t UDP_PORT = 9;              // port for UDP echo
constexpr double DEFAULT_NOISE_FIGURE = 1.0;  // dB (default for YansWifiPhy)

// Packet sizes (bytes) for each STA: index 0..4 according to project statement
constexpr uint32_t PACKET_SIZES[NUM_STA] = {1024, 512, 1024, 512, 1024};

const char* IP_BASE = "192.168.1.0";
const char* IP_MASK = "255.255.255.0";

int main(int argc, char *argv[]) {
    double rxNoiseFigure = DEFAULT_NOISE_FIGURE;
    CommandLine cmd;
    cmd.AddValue("noise", "Rx noise figure in dB", rxNoiseFigure);
    cmd.Parse(argc, argv);

    // ========== 1. Create nodes ==========
    NodeContainer staNodes;
    staNodes.Create(NUM_STA);
    NodeContainer apNode;
    apNode.Create(1);

    NodeContainer allNodes;
    allNodes.Add(staNodes);
    allNodes.Add(apNode);

    // ========== 2. Configure physical and MAC layers for Wi-Fi 5 ==========
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phyHelper;
    phyHelper.Set("RxNoiseFigure", DoubleValue(rxNoiseFigure));
    phyHelper.SetChannel(channelHelper.Create());

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ac);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue("VhtMcs0"),
                                        "ControlMode", StringValue("VhtMcs0"));

    WifiMacHelper macHelper;
    NetDeviceContainer staDevices, apDevice;

    macHelper.SetType("ns3::StaWifiMac");
    staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    macHelper.SetType("ns3::ApWifiMac");
    apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    // ========== 3. Set mobility (constant positions) ==========
    MobilityHelper mobility;
    double radius = AP_STA_DISTANCE;
    double angles[] = {0, 72, 144, 216, 288};

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (int i = 0; i < NUM_STA; ++i) {
        double rad = angles[i] * M_PI / 180.0;
        double x = radius * cos(rad);
        double y = radius * sin(rad);
        positionAlloc->Add(Vector(x, y, 0.0));
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPosAlloc);
    mobility.Install(apNode);

    // ========== 4. Install internet stack and assign IPv4 addresses ==========
    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase(IP_BASE, IP_MASK);

    NetDeviceContainer allNetDevices;
    allNetDevices.Add(apDevice);
    allNetDevices.Add(staDevices);

    Ipv4InterfaceContainer interfaces = ipv4.Assign(allNetDevices);
    Ipv4Address apAddr = interfaces.GetAddress(0);

    // ========== 5. Install UDP Echo server on AP ==========
    uint16_t port = UDP_PORT;
    UdpEchoServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(apNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(SIMULATION_TIME));

    // ========== 6. Install UDP Echo clients on each STA ==========
    UdpEchoClientHelper clientHelper(apAddr, port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(PACKET_INTERVAL)));

    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        clientHelper.SetAttribute("PacketSize", UintegerValue(PACKET_SIZES[i]));
        ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(i));
        clientApp.Start(Seconds(0.0));
        clientApp.Stop(Seconds(SIMULATION_TIME));
    }

    // ========== 7. Enable FlowMonitor to collect statistics ==========
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.Install(allNodes);

    // ========== 8. Run simulation ==========
    Simulator::Stop(Seconds(SIMULATION_TIME));
    Simulator::Run();

    // ========== 9. Extract and print metrics ==========
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0.0;
    double sumSqThroughput = 0.0;
    double totalDelaySum = 0.0;
    int flowCount = 0;

    std::cout << "\n===== Phase 1 Results (Wi-Fi 5 baseline) =====\n";
    std::cout << "Rx Noise Figure = " << rxNoiseFigure << " dB\n";
    std::cout << "Only flows from STAs to AP are considered:\n";

    for (auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        if (t.destinationAddress == apAddr && t.sourceAddress != apAddr) {
            double throughput = flow.second.rxBytes * 8.0 / SIMULATION_TIME;
            totalThroughput += throughput;
            sumSqThroughput += throughput * throughput;
            flowCount++;

            double avgDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            totalDelaySum += avgDelay;
            double lossRate = (flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets;

            std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Throughput: " << throughput / 1e6 << " Mbps\n";
            std::cout << "  Average delay: " << avgDelay * 1000 << " ms\n";
            std::cout << "  Packet loss: " << lossRate << "%\n";
        }
    }

    if (flowCount > 0) {
        double fairness = (totalThroughput * totalThroughput) / (flowCount * sumSqThroughput);
        double overallAvgThroughput = totalThroughput / flowCount;
        double overallAvgDelay = totalDelaySum / flowCount;

        std::cout << "\n--- Overall Metrics ---\n";
        std::cout << "Average Throughput per STA: " << overallAvgThroughput / 1e6 << " Mbps\n";
        std::cout << "Average Delay per STA: " << overallAvgDelay * 1000 << " ms\n";
        std::cout << "Jain's Fairness Index: " << fairness << std::endl;
    } else {
        std::cout << "No STA->AP flows found!\n";
    }

    Simulator::Destroy();
    return 0;
}