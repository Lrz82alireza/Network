/*
 * phase3.cc
 * Phase 3: Wi-Fi 6 (802.11ax) with UORA, BSRP, MU-EDCA, and OFDMA.
 * Compatible with ns-3.38 (with known limitations).
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

using namespace ns3;

// ========== Simulation parameters ==========
constexpr uint32_t NUM_STA = 40;                       // 40 stations
constexpr double SIMULATION_TIME = 10.0;              // seconds
constexpr double MIN_RANGE = 5.0;                     // meters
constexpr double MAX_RANGE = 50.0;                    // meters
constexpr double PACKET_INTERVAL = 0.005;             // 5 ms
constexpr uint16_t UDP_PORT = 9;
constexpr double DEFAULT_NOISE_FIGURE = 1.0;          // dB
constexpr uint32_t PACKET_SIZE = 93;                  // bytes
constexpr double START_TIME_MAX = 1.0;                // seconds
constexpr uint32_t N_RA_RUS = 10;
constexpr bool USE_CENTRAL_26_TONES_RUS = true;

const char* IP_BASE = "192.168.1.0";
const char* IP_MASK = "255.255.255.0";

// Global trace files
std::ofstream sinrTraceFile;
std::ofstream rxTraceFile;

// Free functions for callbacks
void SinrTraceCallback(std::string context, double sinr, uint16_t channelWidth, uint8_t nss)
{
    sinrTraceFile << Simulator::Now().GetSeconds() << " " << sinr << " " << channelWidth << " " << (uint32_t)nss << std::endl;
}

void RxTraceCallback(std::string context, Ptr<const Packet> p, const Address &addr)
{
    rxTraceFile << Simulator::Now().GetSeconds() << " " << p->GetSize() << " " << addr << std::endl;
}

void PhyRxDropTrace(std::string context, Ptr<const Packet> p, WifiPhyRxfailureReason reason)
{
    sinrTraceFile << Simulator::Now().GetSeconds() << " DROP reason=" << reason << std::endl;
}

int main(int argc, char *argv[])
{
    double rxNoiseFigure = DEFAULT_NOISE_FIGURE;
    CommandLine cmd;
    cmd.AddValue("noise", "Rx noise figure in dB", rxNoiseFigure);
    cmd.Parse(argc, argv);

    sinrTraceFile.open("sinr-trace.txt");
    rxTraceFile.open("rx-trace.txt");

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
    // Set channel: 40 MHz, 5 GHz band
    phyHelper.Set("ChannelSettings", StringValue("{0, 40, BAND_5GHZ, 0}"));

    // ========== 3. HeConfiguration (only create, attributes not set due to ns-3.38 limitations) ==========
    // GuardInterval and EnableUlOfdma attributes do not exist in this version.
    // UL OFDMA is enabled via the MultiUserScheduler below.
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

    // STAs
    macHelper.SetType("ns3::StaWifiMac");
    staDevices = wifiHelper.Install(phyHelper, macHelper, staNodes);

    // AP with MultiUserScheduler (UORA, BSRP)
    macHelper.SetType("ns3::ApWifiMac");
    macHelper.SetMultiUserScheduler("ns3::RrMultiUserScheduler",
                                    "NStations", UintegerValue(NUM_STA),
                                    "EnableUlOfdma", BooleanValue(true),
                                    "EnableBsrp", BooleanValue(true),
                                    "NRaRus", UintegerValue(N_RA_RUS),
                                    "UseCentral26TonesRus", BooleanValue(USE_CENTRAL_26_TONES_RUS));
    apDevice = wifiHelper.Install(phyHelper, macHelper, apNode);

    // Apply HeConfiguration to all devices (even if empty, it's required for 802.11ax)
    for (uint32_t i = 0; i < staDevices.GetN(); ++i)
    {
        Ptr<WifiNetDevice> staWifiNetDevice = DynamicCast<WifiNetDevice>(staDevices.Get(i));
        if (staWifiNetDevice)
            staWifiNetDevice->SetHeConfiguration(heConfigObj);
    }
    Ptr<WifiNetDevice> apWifiNetDevice = DynamicCast<WifiNetDevice>(apDevice.Get(0));
    if (apWifiNetDevice)
        apWifiNetDevice->SetHeConfiguration(heConfigObj);

    // ========== 5. Random positions for STAs ==========
    MobilityHelper mobility;
    Ptr<UniformRandomVariable> positionRng = CreateObject<UniformRandomVariable>();
    positionRng->SetAttribute("Min", DoubleValue(MIN_RANGE));
    positionRng->SetAttribute("Max", DoubleValue(MAX_RANGE));

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < NUM_STA; ++i)
    {
        double angle = positionRng->GetValue(0, 2 * M_PI);
        double r = positionRng->GetValue(MIN_RANGE, MAX_RANGE);
        double x = r * cos(angle);
        double y = r * sin(angle);
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

    // ========== 8. UDP Clients on STAs with random start time ==========
    UdpClientHelper clientHelper(apAddr, UDP_PORT);
    clientHelper.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    clientHelper.SetAttribute("Interval", TimeValue(Seconds(PACKET_INTERVAL)));
    clientHelper.SetAttribute("PacketSize", UintegerValue(PACKET_SIZE));

    Ptr<UniformRandomVariable> startTimeRng = CreateObject<UniformRandomVariable>();
    startTimeRng->SetAttribute("Min", DoubleValue(0.0));
    startTimeRng->SetAttribute("Max", DoubleValue(START_TIME_MAX));

    for (uint32_t i = 0; i < NUM_STA; ++i)
    {
        ApplicationContainer clientApp = clientHelper.Install(staNodes.Get(i));
        clientApp.Start(Seconds(startTimeRng->GetValue()));
        clientApp.Stop(Seconds(SIMULATION_TIME));
    }

    // ========== 9. Tracing (SINR and RxTrace) using callbacks ==========
    for (uint32_t i = 0; i < allNodes.GetN(); ++i)
    {
        Ptr<WifiNetDevice> wifiNetDevice = DynamicCast<WifiNetDevice>(allNodes.Get(i)->GetDevice(0));
        if (wifiNetDevice)
        {
            Ptr<WifiPhy> wifiPhy = wifiNetDevice->GetPhy();
            // Try to connect Sinr trace (if not present, ignore)
            bool connected = wifiPhy->TraceConnect("Sinr", "sinr", MakeCallback(&SinrTraceCallback));
            if (!connected)
            {
                wifiPhy->TraceConnect("PhyRxDrop", "drop", MakeCallback(&PhyRxDropTrace));
            }
            // Connect MacRx for packet reception trace
            Ptr<WifiMac> wifiMac = wifiNetDevice->GetMac();
            wifiMac->TraceConnect("MacRx", "rx", MakeCallback(&RxTraceCallback));
        }
    }

    // ========== 10. FlowMonitor ==========
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> flowMonitor = flowHelper.Install(allNodes);

    // ========== 11. Run simulation ==========
    Simulator::Stop(Seconds(SIMULATION_TIME));
    Simulator::Run();

    // ========== 12. Collect and print results ==========
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    double totalThroughput = 0.0;
    double sumSqThroughput = 0.0;
    int flowCount = 0;

    std::cout << "\n===== Phase 3 Results (Wi-Fi 6 with UORA, BSRP, OFDMA) =====\n";
    std::cout << "Number of STAs: " << NUM_STA << "\n";
    std::cout << "Rx Noise Figure: " << rxNoiseFigure << " dB\n";
    std::cout << "Packet interval: " << PACKET_INTERVAL * 1000 << " ms\n";
    std::cout << "Random start max: " << START_TIME_MAX << " s\n";
    std::cout << "Only flows from STAs to AP are considered:\n";

    for (auto &flow : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);
        if (t.destinationAddress == apAddr && t.sourceAddress != apAddr)
        {
            double throughput = flow.second.rxBytes * 8.0 / SIMULATION_TIME;
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

    if (flowCount > 0)
    {
        double fairness = (totalThroughput * totalThroughput) / (flowCount * sumSqThroughput);
        std::cout << "\nTotal Throughput: " << totalThroughput / 1e6 << " Mbps\n";
        std::cout << "Jain's Fairness Index: " << fairness << std::endl;
    }
    else
    {
        std::cout << "No STA->AP flows found!\n";
    }

    sinrTraceFile.close();
    rxTraceFile.close();

    Simulator::Destroy();
    return 0;
}