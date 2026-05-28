/*
 * phase3.cc
 * Phase 3: Wi-Fi 6 (802.11ax) with UORA, BSRP, OFDMA.
 * Added STA positions, guard interval, and central RU configuration.
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
#include "ns3/random-variable-stream.h"
#include "ns3/he-configuration.h"
#include "ns3/rr-multi-user-scheduler.h"
#include <iomanip>      
#include <cmath>        

using namespace ns3;

// ========== Simulation parameters ==========
constexpr uint32_t NUM_STA = 40;
constexpr double SIMULATION_TIME = 10.0;
constexpr double MIN_RANGE = 5.0;
constexpr double MAX_RANGE = 50.0;
constexpr double PACKET_INTERVAL = 0.005;   // 5 ms
constexpr uint16_t UDP_PORT = 9;
constexpr double DEFAULT_NOISE_FIGURE = 1.0;
constexpr uint32_t PACKET_SIZE = 93;
constexpr double START_TIME_MAX = 1.0;

const char* IP_BASE = "192.168.1.0";
const char* IP_MASK = "255.255.255.0";

struct StaPosition {
    double x;
    double y;
    double distance;  
};

int main(int argc, char *argv[])
{
    // Apply 800ns Guard Interval globally for HE configurations
    Config::SetDefault("ns3::HeConfiguration::GuardInterval", TimeValue(NanoSeconds(800)));

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

    // ========== 2. Spectrum channel and PHY ==========
    Ptr<MultiModelSpectrumChannel> spectrumChannel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<LogDistancePropagationLossModel> lossModel = CreateObject<LogDistancePropagationLossModel>();
    spectrumChannel->AddPropagationLossModel(lossModel);
    Ptr<ConstantSpeedPropagationDelayModel> delayModel = CreateObject<ConstantSpeedPropagationDelayModel>();
    spectrumChannel->SetPropagationDelayModel(delayModel);

    SpectrumWifiPhyHelper phyHelper;
    phyHelper.SetChannel(spectrumChannel);
    phyHelper.Set("RxNoiseFigure", DoubleValue(rxNoiseFigure));
    phyHelper.Set("ChannelSettings", StringValue("{0, 40, BAND_5GHZ, 0}"));

    // ========== 3. 802.11ax features ==========
    HeConfiguration heConfiguration;
    Ptr<HeConfiguration> heConfigObj = CreateObject<HeConfiguration>(heConfiguration);

    // ========== 4. WifiHelper ==========
    WifiHelper wifiHelper;
    wifiHelper.SetStandard(WIFI_STANDARD_80211ax);
    wifiHelper.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                        "DataMode", StringValue("HeMcs7"),
                                        "ControlMode", StringValue("HeMcs0"));

    WifiMacHelper macHelper;
    NetDeviceContainer staDevices, apDevice;

    macHelper.SetType("ns3::StaWifiMac");
    staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    // Setup UORA and BSRP on AP's MultiUserScheduler
    macHelper.SetType("ns3::ApWifiMac");
    macHelper.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
                                    "NStations", UintegerValue(NUM_STA),
                                    "EnableUlOfdma", BooleanValue(true),
                                    "EnableBsrp", BooleanValue(true),
                                    "UseCentral26TonesRus", BooleanValue(true));
                                    
    apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    for (uint32_t i = 0; i < staDevices.GetN(); ++i) {
        Ptr<WifiNetDevice> staWifiNetDevice = DynamicCast<WifiNetDevice>(staDevices.Get(i));
        if (staWifiNetDevice) staWifiNetDevice->SetHeConfiguration(heConfigObj);
    }
    Ptr<WifiNetDevice> apWifiNetDevice = DynamicCast<WifiNetDevice>(apDevice.Get(0));
    if (apWifiNetDevice) apWifiNetDevice->SetHeConfiguration(heConfigObj);

    // ========== 5. Random positions for STAs ==========
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> positionRng = CreateObject<UniformRandomVariable>();
    positionRng->SetAttribute("Min", DoubleValue(MIN_RANGE));
    positionRng->SetAttribute("Max", DoubleValue(MAX_RANGE));

    std::vector<StaPosition> staPositions(NUM_STA);
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < NUM_STA; ++i) {
        double angle = positionRng->GetValue(0, 2 * M_PI);
        double r = positionRng->GetValue(MIN_RANGE, MAX_RANGE);
        double x = r * cos(angle);
        double y = r * sin(angle);
        positionAlloc->Add(Vector(x, y, 0.0));
        staPositions[i].x = x;
        staPositions[i].y = y;
        staPositions[i].distance = r;
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(staNodes);

    Ptr<ListPositionAllocator> apPosAlloc = CreateObject<ListPositionAllocator>();
    apPosAlloc->Add(Vector(0.0, 0.0, 0.0));
    mobility.SetPositionAllocator(apPosAlloc);
    mobility.Install(apNode);

    // ========== 6. Internet stack and addressing ==========
    InternetStackHelper internet;
    internet.Install(allNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase(IP_BASE, IP_MASK);
    NetDeviceContainer allNetDevices;
    allNetDevices.Add(apDevice);
    allNetDevices.Add(staDevices);
    Ipv4InterfaceContainer interfaces = ipv4.Assign(allNetDevices);
    Ipv4Address apAddr = interfaces.GetAddress(0);

    // ========== 7. PacketSink on AP ==========
    PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), UDP_PORT));
    ApplicationContainer sinkApp = sinkHelper.Install(apNode);
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(SIMULATION_TIME));

    // ========== 8. UDP clients with random start time ==========
    UdpClientHelper clientHelper(apAddr, UDP_PORT);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(PACKET_INTERVAL)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(PACKET_SIZE));

    Ptr<UniformRandomVariable> startTimeRng = CreateObject<UniformRandomVariable>();
    startTimeRng->SetAttribute("Min", DoubleValue(0.0));
    startTimeRng->SetAttribute("Max", DoubleValue(START_TIME_MAX));

    for (uint32_t i = 0; i < NUM_STA; ++i) {
        ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(i));
        clientApp.Start(Seconds(startTimeRng->GetValue()));
        clientApp.Stop(Seconds(SIMULATION_TIME));
    }

    // ========== 9. FlowMonitor ==========
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.Install(allNodes);

    Simulator::Stop(Seconds(SIMULATION_TIME));
    Simulator::Run();

    // ========== 10. Collect and print results ==========
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0.0;
    double sumSqThroughput = 0.0;
    int flowCount = 0;

    std::cout << "\n===== Phase 3 Results (Wi-Fi 6 with UORA, BSRP, OFDMA) =====\n";
    std::cout << "Number of STAs: " << NUM_STA << "\n";
    std::cout << "Rx Noise Figure: " << rxNoiseFigure << " dB\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nSTA positions (distance from AP, coordinates):\n";
    for (uint32_t i = 0; i < NUM_STA; ++i) {
        std::cout << "  STA " << i+1 << ": distance = " << staPositions[i].distance << " m, ("
                  << staPositions[i].x << ", " << staPositions[i].y << ")\n";
    }
    std::cout << "\nOnly flows from STAs to AP are considered:\n";

    std::map<Ipv4Address, uint32_t> ipToStaIdx;
    for (uint32_t i = 0; i < NUM_STA; ++i) {
        Ipv4Address staAddr = interfaces.GetAddress(i+1);
        ipToStaIdx[staAddr] = i;
    }

    for (auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == apAddr && t.sourceAddress != apAddr) {
            double throughput = flow.second.rxBytes * 8.0 / SIMULATION_TIME; 
            totalThroughput += throughput;
            sumSqThroughput += throughput * throughput;
            flowCount++;

            double avgDelay = flow.second.delaySum.GetSeconds() / flow.second.rxPackets;
            double lossRate = (flow.second.txPackets - flow.second.rxPackets) * 100.0 / flow.second.txPackets;

            uint32_t staIdx = 0;
            auto it = ipToStaIdx.find(t.sourceAddress);
            if (it != ipToStaIdx.end()) {
                staIdx = it->second;
            } else {
                std::stringstream ss;
                t.sourceAddress.Print(ss);
                std::string ipStr = ss.str();
                size_t lastDot = ipStr.find_last_of('.');
                if (lastDot != std::string::npos) {
                    int lastOctet = std::stoi(ipStr.substr(lastDot+1));
                    staIdx = lastOctet - 2; 
                    if (staIdx >= NUM_STA) staIdx = 0;
                }
            }

            std::cout << "Flow " << flow.first << " (STA " << staIdx+1 
                      << ", dist=" << staPositions[staIdx].distance << " m) "
                      << t.sourceAddress << " -> " << t.destinationAddress << "\n";
            std::cout << "  Throughput: " << throughput / 1e6 << " Mbps\n";
            std::cout << "  Average delay: " << avgDelay * 1000 << " ms\n";
            std::cout << "  Packet loss: " << lossRate << "%\n";
        }
    }

    if (flowCount > 0) {
        double fairness = (totalThroughput * totalThroughput) / (flowCount * sumSqThroughput);
        std::cout << "\nTotal Throughput: " << totalThroughput / 1e6 << " Mbps\n";
        std::cout << "Jain's Fairness Index: " << fairness << std::endl;
    } else {
        std::cout << "No STA->AP flows found!\n";
    }

    Simulator::Destroy();
    return 0;
}