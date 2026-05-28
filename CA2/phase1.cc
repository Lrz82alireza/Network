/*
 * phase1.cc
 * Phase 1: Baseline Wi-Fi 5 (802.11ac) simulation with 1 AP and 5 STAs.
 * Measures throughput, delay, packet loss, and Jain's fairness index.
 *
 * Usage:
 *   ./ns3 run phase1                # default noise figure = 1.0 dB
 *   ./ns3 run "phase1 --noise=7.0"  # increase noise to 7.0 dB
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
// STA0=1024, STA1=512, STA2=1024, STA3=512, STA4=1024
constexpr uint32_t PACKET_SIZES[NUM_STA] = {1024, 512, 1024, 512, 1024};

// IP base for addressing
const char* IP_BASE = "192.168.1.0";
const char* IP_MASK = "255.255.255.0";

int main(int argc, char *argv[]) {
    // ---------- Parse command line arguments for noise figure ----------
    double rxNoiseFigure = DEFAULT_NOISE_FIGURE;
    CommandLine cmd;
    cmd.AddValue("noise", "Rx noise figure in dB", rxNoiseFigure);
    cmd.Parse(argc, argv);

    // ========== 1. Create nodes ==========
    NodeContainer staNodes;
    staNodes.Create(NUM_STA);               // 5 stations

    NodeContainer apNode;
    apNode.Create(1);                       // 1 access point

    // Combine for later convenience (e.g., internet stack)
    NodeContainer allNodes;
    allNodes.Add(staNodes);
    allNodes.Add(apNode);

    // ========== 2. Configure physical and MAC layers for Wi-Fi 5 ==========
    // 2.1 Create channel helper (default propagation loss + delay)
    YansWifiChannelHelper channelHelper = YansWifiChannelHelper::Default();

    // 2.2 Create PHY helper and attach channel
    YansWifiPhyHelper phyHelper;
    // Set noise figure (can be changed via command line)
    phyHelper.Set("RxNoiseFigure", DoubleValue(rxNoiseFigure));
    phyHelper.SetChannel(channelHelper.Create());

    // 2.3 Create Wi-Fi helper and set standard to 802.11ac
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ac);

    // 2.4 Set rate control algorithm (constant rate for baseline)
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue("VhtMcs0"),
                                        "ControlMode", StringValue("VhtMcs0"));
    // wifiHelper.SetRemoteStationManager("ns3::IdealWifiManager");

    // 2.5 Create MAC helper and install on STAs and AP
    WifiMacHelper macHelper;
    NetDeviceContainer staDevices, apDevice;

    macHelper.SetType("ns3::StaWifiMac");           // STA role
    staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    macHelper.SetType("ns3::ApWifiMac");            // AP role
    apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    // ========== 3. Set mobility (constant positions) ==========
    MobilityHelper mobility;
    double radius = AP_STA_DISTANCE;  // meters (distance from AP to each STA)
    // Angles for 5 equally spaced STAs (0, 72, 144, 216, 288 degrees)
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

    // AP at origin
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
    allNetDevices.Add(apDevice);      // first device: AP (index 0)
    allNetDevices.Add(staDevices);    // then STAs (indices 1 to 5)

    Ipv4InterfaceContainer interfaces = ipv4.Assign(allNetDevices);

    // AP is at index 0
    Ipv4Address apAddr = interfaces.GetAddress(0);

    // ========== 5. Install UDP Echo server on AP ==========
    uint16_t port = UDP_PORT;
    UdpEchoServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(apNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(SIMULATION_TIME));

    // ========== 6. Install UDP Echo clients on each STA ==========
    // Packet sizes: as defined in PACKET_SIZES array
    UdpEchoClientHelper clientHelper(apAddr, port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); // unlimited
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
    std::vector<double> throughputs;
    int flowCount = 0;

    std::cout << "\n===== Phase 1 Results (Wi-Fi 5 baseline) =====\n";
    std::cout << "Rx Noise Figure = " << rxNoiseFigure << " dB\n";
    std::cout << "Only flows from STAs to AP are considered:\n";

    for (auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        // Check if this flow is from a STA to AP
        // STA IPs: 192.168.1.2 to 192.168.1.6 , AP IP: 192.168.1.1
        if (t.destinationAddress == apAddr && t.sourceAddress != apAddr) {
            double throughput = flow.second.rxBytes * 8.0 / SIMULATION_TIME; // bits per second
            throughputs.push_back(throughput);
            totalThroughput += throughput;
            sumSqThroughput += throughput * throughput;
            flowCount++;

            double avgDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            double lossRate = (flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets;

            std::cout << "Flow " << flow.first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
            std::cout << "  Throughput: " << throughput / 1e6 << " Mbps\n";
            std::cout << "  Average delay: " << avgDelay * 1000 << " ms\n";
            std::cout << "  Packet loss: " << lossRate << "%\n";
        }
    }

    if (flowCount > 0) {
        double fairness = (totalThroughput * totalThroughput) / (flowCount * sumSqThroughput);
        std::cout << "\nJain's Fairness Index: " << fairness << std::endl;
    } else {
        std::cout << "No STA->AP flows found!\n";
    }

    Simulator::Destroy();

    return 0;
}