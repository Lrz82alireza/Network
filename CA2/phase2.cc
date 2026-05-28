/*
 * phase2.cc
 * Phase 2: Wi-Fi 6 (802.11ax) simulation with OFDMA.
 * Compares performance metrics with Phase 1 (Wi-Fi 5).
 * Fixed for ns-3.41 API compatibility.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/propagation-module.h"

using namespace ns3;

// ========== Simulation parameters ==========
constexpr int NUM_STA = 5;                
constexpr double SIMULATION_TIME = 10.0;      
constexpr double AP_STA_DISTANCE = 5.0;       
constexpr double PACKET_INTERVAL = 0.5;       
constexpr uint16_t UDP_PORT = 9;              
constexpr double DEFAULT_NOISE_FIGURE = 1.0;  

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

    // ========== 2. Configure physical and MAC layers for Wi-Fi 6 ==========
    
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
    spectrumChannel->AddPropagationLossModel(lossModel);
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->SetPropagationDelayModel(delayModel);

    SpectrumWifiPhyHelper phyHelper;
    phyHelper.SetChannel(spectrumChannel);
    phyHelper.Set("RxNoiseFigure", DoubleValue(rxNoiseFigure));

    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ax);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue("HeMcs7"),
                                        "ControlMode", StringValue("HeMcs0"));

    WifiMacHelper macHelper;
    NetDeviceContainer staDevices, apDevice;

    macHelper.SetType("ns3::StaWifiMac");
    staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    // Fixed for ns-3.41: Removed invalid DL OFDMA/MU-MIMO attributes
    // DL OFDMA is handled intrinsically by 802.11ax APs in this version.
    macHelper.SetType("ns3::ApWifiMac");
    macHelper.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
                                    "EnableUlOfdma", BooleanValue(true),
                                    "EnableBsrp", BooleanValue(true));
                                    
    apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    // ========== 3. Set mobility ==========
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

    // ========== 4. Install internet stack ==========
    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase(IP_BASE, IP_MASK);

    NetDeviceContainer allNetDevices;
    allNetDevices.Add(apDevice);      
    allNetDevices.Add(staDevices);    

    Ipv4InterfaceContainer interfaces = ipv4.Assign(allNetDevices);
    Ipv4Address apAddr = interfaces.GetAddress(0);

    // ========== 5. Install applications ==========
    uint16_t port = UDP_PORT;
    UdpEchoServerHelper serverHelper(port);
    ApplicationContainer serverApp = serverHelper.Install(apNode);
    serverApp.Start(Seconds(0.0));
    serverApp.Stop(Seconds(SIMULATION_TIME));

    UdpEchoClientHelper clientHelper(apAddr, port);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF)); 
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(PACKET_INTERVAL)));

    for (uint32_t i = 0; i < staNodes.GetN(); ++i) {
        clientHelper.SetAttribute("PacketSize", UintegerValue(PACKET_SIZES[i]));
        ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(i));
        clientApp.Start(Seconds(0.0));
        clientApp.Stop(Seconds(SIMULATION_TIME));
    }

    // ========== 6. FlowMonitor ==========
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.Install(allNodes);

    Simulator::Stop(Seconds(SIMULATION_TIME));
    Simulator::Run();

    // ========== 7. Results ==========
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0.0;
    double sumSqThroughput = 0.0;
    double totalDelaySum = 0.0;
    int flowCount = 0;

    std::cout << "\n===== Phase 2 Results (Wi-Fi 6 with OFDMA) =====\n";
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