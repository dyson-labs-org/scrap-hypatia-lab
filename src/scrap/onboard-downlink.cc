// Onboard-originated feeder downlink using unmodified SNS-3 queues and PHY.
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/satellite-module.h"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
using namespace ns3;
namespace {
constexpr uint16_t PORT = 7230;
constexpr double BANDWIDTH = 125e6, ROLL_OFF = 0.20, LIGHT_SPEED = 299792458.0;

// Trace correlation only: this tag neither delivers data nor changes link behavior.
class ProbeTag : public Tag {
public:
    static TypeId GetTypeId() {
        static TypeId t = TypeId("ns3::OnboardProbeTag").SetParent<Tag>().AddConstructor<ProbeTag>();
        return t;
    }
    TypeId GetInstanceTypeId() const override { return GetTypeId(); }
    uint32_t GetSerializedSize() const override { return 1; }
    void Serialize(TagBuffer b) const override { b.WriteU8(1); }
    void Deserialize(TagBuffer b) override { b.ReadU8(); }
    void Print(std::ostream& os) const override { os << "onboard-probe"; }
};

struct Evidence {
    Ptr<Node> source, receiver;
    Ptr<SatOrbiterNetDevice> device;
    Ptr<SatNetDevice> groundDevice;
    Ptr<SatOrbiterFeederMac> mac;
    uint32_t payloadBytes = 1024, sent = 0, received = 0, frames = 0;
    double sentAt = -1, txAt = -1, receivedAt = -1, distance = -1, frameDuration = -1;
    bool payloadValid = true;

    void Frame(Ptr<SatBbFrame> frame) {
        if (!frame) return;
        bool marked = false;
        for (auto packet : frame->GetPayload()) {
            ProbeTag tag;
            marked |= packet->PeekPacketTag(tag);
        }
        if (!marked) return;
        ++frames;
        txAt = Simulator::Now().GetSeconds();
        frameDuration = frame->GetDuration().GetSeconds();
        distance = source->GetObject<MobilityModel>()->GetDistanceFrom(receiver->GetObject<MobilityModel>());
        NS_ABORT_MSG_IF(frame->GetModcod() != SatEnums::SAT_MODCOD_QPSK_1_TO_2 ||
                        frame->GetFrameType() != SatEnums::NORMAL_FRAME,
                        "Timing fixture requires QPSK 1/2 normal frames");
        std::cout << "EVENT {\"event\":\"feeder-frame-tx\",\"time_s\":" << txAt
                  << ",\"source_node\":" << source->GetId() << ",\"device_index\":" << device->GetIfIndex()
                  << ",\"feeder_mac\":\"" << mac->GetAddress() << "\",\"beam\":" << mac->GetBeamId()
                  << ",\"frame_duration_s\":" << frameDuration << ",\"distance_m\":" << distance << "}\n";
    }
    void Receive(Ptr<Socket> socket) {
        Address sender;
        while (auto packet = socket->RecvFrom(sender)) {
            ++received;
            receivedAt = Simulator::Now().GetSeconds();
            std::vector<uint8_t> bytes(packet->GetSize());
            packet->CopyData(bytes.data(), bytes.size());
            payloadValid &= bytes.size() == payloadBytes;
            for (size_t i = 0; i < bytes.size(); ++i) payloadValid &= bytes[i] == uint8_t(i % 251);
            payloadValid &= InetSocketAddress::ConvertFrom(sender).GetIpv4() == Ipv4Address("192.0.2.1");
            std::cout << "EVENT {\"event\":\"ground-application-rx\",\"time_s\":" << receivedAt
                      << ",\"receiver_node\":" << receiver->GetId()
                      << ",\"device_index\":" << groundDevice->GetIfIndex()
                      << ",\"payload_bytes\":" << packet->GetSize() << "}\n";
        }
    }
};

class OnboardSource : public Application {
public:
    void Configure(Evidence* e, Ipv4Address dest) { m_e = e; m_dest = dest; }
private:
    Evidence* m_e = nullptr;
    Ipv4Address m_dest;
    void StartApplication() override {
        auto& e = *m_e;
        NS_ABORT_MSG_IF(GetNode() != e.device->GetNode(), "Source is not onboard the orbiter");
        std::vector<uint8_t> bytes(e.payloadBytes);
        for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = i % 251;
        auto packet = Create<Packet>(bytes.data(), bytes.size());
        // SatOrbiterNetDevice::Send asserts: no onboard IP-stack entry point.
        // Serialize a datagram, then use the existing feeder MAC/LLC. No fake
        // uplink, replacement channel, or direct ground receive callback.
        UdpHeader udp;
        udp.SetSourcePort(PORT);
        udp.SetDestinationPort(PORT);
        packet->AddHeader(udp);
        Ipv4Header ip;
        ip.SetSource(Ipv4Address("192.0.2.1")); // Unidirectional fixture identity.
        ip.SetDestination(m_dest);
        ip.SetProtocol(17);
        ip.SetTtl(64);
        ip.SetPayloadSize(packet->GetSize());
        packet->AddHeader(ip);
        auto src = Mac48Address::ConvertFrom(e.mac->GetAddress());
        auto dst = Mac48Address::ConvertFrom(e.groundDevice->GetAddress());
        SatAddressE2ETag addresses;
        addresses.SetE2ESourceAddress(src);
        addresses.SetE2EDestAddress(dst);
        packet->AddPacketTag(addresses);
        SatMacTag mac;
        mac.SetSourceAddress(src);
        mac.SetDestAddress(dst);
        packet->AddPacketTag(mac);
        // Required native metadata, also used by satellite-originated control
        // messages. Infinite upstream SINR means there is no uplink hop.
        SatUplinkInfoTag uplink;
        uplink.SetSinr(std::numeric_limits<double>::infinity(), 0);
        uplink.SetSatId(Singleton<SatTopology>::Get()->GetOrbiterSatId(GetNode()));
        uplink.SetBeamId(e.mac->GetBeamId());
        packet->AddPacketTag(uplink);
        packet->AddPacketTag(ProbeTag());
        e.sentAt = Simulator::Now().GetSeconds();
        ++e.sent;
        std::cout << "EVENT {\"event\":\"onboard-enqueue\",\"time_s\":" << e.sentAt
                  << ",\"source_node\":" << GetNode()->GetId()
                  << ",\"device_type\":\"" << e.device->GetInstanceTypeId().GetName()
                  << "\",\"device_index\":" << e.device->GetIfIndex()
                  << ",\"payload_bytes\":" << e.payloadBytes << "}\n";
        e.mac->EnquePacket(packet);
    }
};
}

int main(int argc, char* argv[]) {
    std::cout << std::setprecision(15);
    std::string scenarioFolder = "constellation-leo-2-satellites", testCase = "normal";
    uint32_t payloadBytes = 1024;
    double sendAt = 1.0;
    auto simulation = CreateObject<SimulationHelper>("scrap-onboard-downlink");
    CommandLine command;
    command.AddValue("scenarioFolder", "Pinned SNS-3 connectivity fixture", scenarioFolder);
    command.AddValue("case", "normal or tx-disabled", testCase);
    command.AddValue("payloadBytes", "Small UDP payload (64..2048 bytes)", payloadBytes);
    command.AddValue("sendAt", "Application start in seconds (0.5..2)", sendAt);
    simulation->AddDefaultUiArguments(command);
    command.Parse(argc, argv);
    NS_ABORT_MSG_IF(testCase != "normal" && testCase != "tx-disabled", "Unknown case");
    NS_ABORT_MSG_IF(payloadBytes < 64 || payloadBytes > 2048, "Payload outside fixture limits");
    NS_ABORT_MSG_IF(!std::isfinite(sendAt) || sendAt < 0.5 || sendAt > 2, "Invalid start time");

    Config::SetDefault("ns3::SatConf::ForwardLinkRegenerationMode", EnumValue(SatEnums::REGENERATION_NETWORK));
    Config::SetDefault("ns3::SatConf::ReturnLinkRegenerationMode", EnumValue(SatEnums::REGENERATION_NETWORK));
    Config::SetDefault("ns3::SatGwMac::SendNcrBroadcast", BooleanValue(false));
    Config::SetDefault("ns3::SatHelper::GwUsers", UintegerValue(1));
    Config::SetDefault("ns3::SatHelper::BeamNetworkAddress", Ipv4AddressValue("20.1.0.0"));
    Config::SetDefault("ns3::SatHelper::GwNetworkAddress", Ipv4AddressValue("10.1.0.0"));
    Config::SetDefault("ns3::SatHelper::UtNetworkAddress", Ipv4AddressValue("250.1.0.0"));
    Config::SetDefault("ns3::SatBbFrameConf::AcmEnabled", BooleanValue(false));
    Config::SetDefault("ns3::SatBbFrameConf::DefaultModCod", StringValue("QPSK_1_TO_2"));
    Config::SetDefault("ns3::SatBbFrameConf::BBFrameUsageMode", EnumValue(SatEnums::NORMAL_FRAMES));
    Config::SetDefault("ns3::SatConf::RtnScpcCarrierAllocatedBandwidth", DoubleValue(BANDWIDTH));
    Config::SetDefault("ns3::SatConf::RtnScpcCarrierRollOff", DoubleValue(ROLL_OFF));
    Config::SetDefault("ns3::SatConf::RtnScpcCarrierSpacing", DoubleValue(0));
    simulation->LoadScenario(scenarioFolder);
    simulation->SetBeamSet(std::set<uint32_t>{43, 30});
    simulation->SetUserCountPerUt(1);
    simulation->SetUtCountPerBeam(1);
    simulation->SetSimulationTime(Seconds(3));
    simulation->CreateSatScenario();

    auto topology = Singleton<SatTopology>::Get();
    Evidence e;
    e.payloadBytes = payloadBytes;
    e.receiver = topology->GetGwFromBeam(43);
    e.source = topology->GetOrbiterNode(topology->GetGwSatId(e.receiver));
    e.device = topology->GetOrbiterNetDevice(e.source);
    // Physical antenna-beam IDs and user-beam MAC keys are different.
    // Resolve the active egress by the native channel's registered receivers.
    const auto connected = e.device->GetGwConnected();
    for (const auto& [beam, usedMac] : e.device->GetFeederMac()) {
        auto mac = DynamicCast<SatOrbiterFeederMac>(usedMac);
        if (!mac) continue;
        auto channel = e.device->GetFeederPhy(mac->GetBeamId())->GetTxChannel();
        for (std::size_t i = 0; i < channel->GetNDevices(); ++i) {
            auto dev = DynamicCast<SatNetDevice>(channel->GetDevice(i));
            if (dev && dev->GetNode() == e.receiver &&
                dev->GetPhy()->GetPhyRx()->GetBeamId() == mac->GetBeamId() &&
                connected.contains(Mac48Address::ConvertFrom(dev->GetAddress()))) {
                e.mac = mac;
                e.groundDevice = dev;
                break;
            }
        }
        if (e.mac) break;
    }
    NS_ABORT_MSG_IF(!e.mac || !e.groundDevice, "No directly connected native feeder path");
    auto ipv4 = e.receiver->GetObject<Ipv4>();
    int32_t interface = ipv4->GetInterfaceForDevice(e.groundDevice);
    NS_ABORT_MSG_IF(interface < 0, "Ground device has no IPv4 interface");
    auto destination = ipv4->GetAddress(interface, 0).GetLocal();
    auto sink = Socket::CreateSocket(e.receiver, UdpSocketFactory::GetTypeId());
    NS_ABORT_MSG_IF(sink->Bind(InetSocketAddress(destination, PORT)) < 0, "Cannot bind ground sink");
    sink->SetRecvCallback(MakeCallback(&Evidence::Receive, &e));
    NS_ABORT_MSG_IF(!e.mac->TraceConnectWithoutContext("BBFrameTxTrace", MakeCallback(&Evidence::Frame, &e)),
                    "Missing native frame trace");
    auto source = CreateObject<OnboardSource>();
    source->Configure(&e, destination);
    e.source->AddApplication(source);
    source->SetStartTime(Seconds(sendAt));
    if (testCase == "tx-disabled") e.mac->Disable();
    simulation->RunSimulation();

    // Independent fixed-profile calculation: 360 data + 1 PL header slots,
    // 90 symbols/slot, and 22 pilot blocks of 36 symbols.
    double expectedFrame = (361.0 * 90 + 22 * 36) / (BANDWIDTH / (1 + ROLL_OFF));
    double guard = e.mac->GetGuardTime().GetSeconds();
    double expectedRx = e.txAt + expectedFrame - guard + e.distance / LIGHT_SPEED;
    double tolerance = 2e-6; // Rounding/event scheduling, not a fitted round trip.
    bool passed = e.sent == 1;
    if (testCase == "normal")
        passed &= e.received == 1 && e.payloadValid && e.frames == 1 &&
                  e.txAt >= e.sentAt && e.txAt - e.sentAt <= 2 * expectedFrame &&
                  std::abs(e.frameDuration - expectedFrame) <= tolerance &&
                  std::abs(e.receivedAt - expectedRx) <= tolerance;
    else
        passed &= e.received == 0 && e.frames == 0;
    std::cout << "RESULT {\"case\":\"" << testCase << "\",\"passed\":" << (passed ? "true" : "false")
              << ",\"source_node\":" << e.source->GetId() << ",\"source_device\":" << e.device->GetIfIndex()
              << ",\"receiver_node\":" << e.receiver->GetId() << ",\"receiver_device\":" << e.groundDevice->GetIfIndex()
              << ",\"payload_bytes\":" << payloadBytes << ",\"sent\":" << e.sent << ",\"received\":" << e.received
              << ",\"frames\":" << e.frames << ",\"payload_valid\":" << (e.payloadValid ? "true" : "false")
              << ",\"enqueue_s\":" << e.sentAt << ",\"tx_s\":" << e.txAt << ",\"received_s\":" << e.receivedAt
              << ",\"distance_m\":" << e.distance << ",\"observed_frame_s\":" << e.frameDuration
              << ",\"bandwidth_hz\":" << BANDWIDTH << ",\"roll_off\":" << ROLL_OFF
              << ",\"guard_s\":" << guard << ",\"tolerance_s\":" << tolerance << "}\n";
    sink->Close();
    return passed ? 0 : 1;
}
