// Behavioral application integration over native SNS-3 relay links; no cryptography.
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/satellite-module.h"
#include <array>
#include <bitset>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;
namespace {
enum Kind : uint32_t { REQUEST = 1, PERMIT, DATA, RECEIPT, REJECT };
constexpr uint32_t CHUNKS = 16;
constexpr uint32_t CHUNK_BYTES = 512;

// Deliberately synthetic wire format: eight network-order uint32 fields.
struct Message
{
    uint32_t kind, token = 1, task = 1, subject = 1, audience = 1;
    uint32_t expiresMs = 19000, chunk = 0, chunks = CHUNKS;
    Ptr<Packet> Encode() const
    {
        std::array<uint32_t, 8> words{kind, token, task, subject, audience,
                                      expiresMs, chunk, chunks};
        uint32_t size = kind == DATA ? 32 + CHUNK_BYTES : (kind == REQUEST ? 256 : 64);
        std::vector<uint8_t> bytes(size, 0);
        for (size_t i = 0; i < words.size(); ++i)
            for (size_t j = 0; j < 4; ++j)
                bytes[4 * i + j] = (words[i] >> (24 - 8 * j)) & 255;
        return Create<Packet>(bytes.data(), bytes.size());
    }
    static bool Decode(Ptr<Packet> packet, Message& message)
    {
        if (packet->GetSize() < 32) return false;
        std::array<uint8_t, 32> bytes{};
        packet->CopyData(bytes.data(), bytes.size());
        std::array<uint32_t, 8> words{};
        for (size_t i = 0; i < words.size(); ++i)
            for (size_t j = 0; j < 4; ++j)
                words[i] = (words[i] << 8) | bytes[4 * i + j];
        message = {words[0], words[1], words[2], words[3], words[4],
                   words[5], words[6], words[7]};
        uint32_t size = message.kind == DATA ? 32 + CHUNK_BYTES :
                        (message.kind == REQUEST ? 256 : 64);
        return message.kind >= REQUEST && message.kind <= REJECT &&
               packet->GetSize() == size && message.chunks == CHUNKS;
    }
};

class RelayApplication : public Application
{
  public:
    static TypeId GetTypeId()
    {
        static TypeId id = TypeId("ns3::ScrapRelayApplication")
            .SetParent<Application>().AddConstructor<RelayApplication>();
        return id;
    }
    void Configure(bool executor, Address peer, std::string fault, Time deadline)
    {
        m_executor = executor;
        m_peer = peer;
        m_fault = fault;
        m_deadline = deadline;
    }
    uint32_t executions = 0, rejections = 0, attempts = 0;
    uint64_t transmittedBytes = 0;
    bool receiptReceived = false;
    double deliveredAt = -1, receiptAt = -1;

  private:
    bool m_executor = false, m_admitted = false, m_authorized = false;
    bool m_processing = false, m_droppedReceipt = false, m_conflictSent = false;
    bool m_rejected = false;
    Address m_peer;
    Ptr<Socket> m_socket;
    EventId m_retry, m_work;
    Time m_deadline;
    std::string m_fault;
    std::bitset<CHUNKS> m_chunks;
    Message m_bound{REQUEST};

    void StartApplication() override
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        NS_ABORT_MSG_IF(m_socket->Bind(InetSocketAddress(Ipv4Address::GetAny(), 7227)) < 0,
                        "Cannot bind application socket");
        m_socket->SetRecvCallback(MakeCallback(&RelayApplication::Receive, this));
        if (!m_executor) Retry();
    }
    void StopApplication() override
    {
        m_retry.Cancel();
        m_work.Cancel();
        if (m_socket) m_socket->Close();
    }
    void Emit(std::string event) const
    {
        std::cout << "EVENT " << Simulator::Now().GetSeconds() << " "
                  << (m_executor ? "executor " : "commander ") << event << "\n";
    }
    void Send(Message message)
    {
        auto packet = message.Encode();
        int sent = m_socket->SendTo(packet, 0, m_peer);
        if (sent >= 0) transmittedBytes += sent;
        else Emit("socket-send-failed");
    }
    void Retry()
    {
        if (receiptReceived || m_rejected || Simulator::Now() >= m_deadline) return;
        ++attempts;
        Message request{REQUEST};
        if (m_fault == "wrong-subject") request.subject = 2;
        if (m_fault == "expired") request.expiresMs = 1000;
        Send(request);
        Emit("request-sent");
        m_retry = Simulator::Schedule(Seconds(2), &RelayApplication::Retry, this);
    }
    bool SameTask(const Message& m) const
    {
        return m.token == m_bound.token && m.task == m_bound.task &&
               m.subject == m_bound.subject && m.audience == m_bound.audience &&
               m.expiresMs == m_bound.expiresMs && m.chunks == m_bound.chunks;
    }
    void Reply(uint32_t kind)
    {
        auto reply = m_bound;
        reply.kind = kind;
        Send(reply);
    }
    void Authorize()
    {
        if (Simulator::Now() >= m_deadline ||
            Simulator::Now().GetMilliSeconds() >= m_bound.expiresMs)
        {
            ++rejections;
            Reply(REJECT);
            return;
        }
        m_authorized = true;
        Emit("authorized");
        Reply(PERMIT);
    }
    void Complete()
    {
        if (Simulator::Now() >= m_deadline) return;
        ++executions;
        deliveredAt = Simulator::Now().GetSeconds();
        Emit("processed-product-delivered");
        if (m_fault == "lost-receipt" && !m_droppedReceipt)
        {
            m_droppedReceipt = true;
            Emit("fault-first-receipt-suppressed");
            return;
        }
        Reply(RECEIPT);
    }
    void Receive(Ptr<Socket> socket)
    {
        Address sender;
        while (auto packet = socket->RecvFrom(sender))
        {
            // This fixed-peer check models a trusted transport association, not authentication.
            if (sender != m_peer) continue;
            Message message{REQUEST};
            if (!Message::Decode(packet, message)) continue;
            if (m_executor) Execute(message);
            else HandleReply(message);
        }
    }
    void Execute(const Message& message)
    {
        if (Simulator::Now() >= m_deadline) return;
        if (message.kind == REQUEST)
        {
            if (message.subject != 1 || message.audience != 1 || message.token != 1 ||
                (m_admitted && !SameTask(message)) ||
                (!m_admitted && Simulator::Now().GetMilliSeconds() >= message.expiresMs))
            {
                ++rejections;
                auto rejection = message;
                rejection.kind = REJECT;
                Send(rejection);
                Emit("rejected");
            }
            else if (executions) Reply(RECEIPT);
            else if (m_authorized) Reply(PERMIT);
            else if (!m_admitted)
            {
                m_bound = message;
                m_admitted = true;
                // Explicit 1 ms sensitivity assumption; not the historical 18 ms round trip.
                m_work = Simulator::Schedule(MilliSeconds(1),
                                             &RelayApplication::Authorize, this);
            }
        }
        else if (message.kind == DATA && m_authorized && SameTask(message) &&
                 message.chunk < CHUNKS && !m_processing)
        {
            m_chunks.set(message.chunk);
            if (m_chunks.all())
            {
                m_processing = true;
                m_work = Simulator::Schedule(MilliSeconds(5),
                                             &RelayApplication::Complete, this);
            }
        }
    }
    void HandleReply(const Message& message)
    {
        if (message.kind == PERMIT && !receiptReceived)
        {
            for (uint32_t i = 0; i < CHUNKS; ++i)
            {
                auto data = message;
                data.kind = DATA;
                data.chunk = i;
                Send(data);
            }
        }
        else if (message.kind == RECEIPT)
        {
            if (!receiptReceived) receiptAt = Simulator::Now().GetSeconds();
            receiptReceived = true;
            Emit("receipt-received");
            if (m_fault == "conflict" && !m_conflictSent)
            {
                m_conflictSent = true;
                auto request = message;
                request.kind = REQUEST;
                request.task = 2;
                Send(request);
            }
        }
        else if (message.kind == REJECT) m_rejected = true;
    }
};
} // namespace

int main(int argc, char* argv[])
{
    std::string testCase = "normal";
    std::string scenarioFolder = "constellation-eutelsat-geo-2-sats-isls";
    auto simulation = CreateObject<SimulationHelper>("scrap-relay-lifecycle");
    CommandLine command;
    command.AddValue("case", "normal, wrong-subject, expired, lost-receipt, conflict, deadline", testCase);
    command.AddValue("scenarioFolder", "Pinned SNS-3 satellite relay scenario", scenarioFolder);
    simulation->AddDefaultUiArguments(command);
    command.Parse(argc, argv);
    const std::set<std::string> cases{"normal", "wrong-subject", "expired", "lost-receipt", "conflict", "deadline"};
    NS_ABORT_MSG_IF(!cases.contains(testCase), "Unknown lifecycle test case");
    Config::SetDefault("ns3::SatConf::ForwardLinkRegenerationMode", EnumValue(SatEnums::REGENERATION_NETWORK));
    Config::SetDefault("ns3::SatConf::ReturnLinkRegenerationMode", EnumValue(SatEnums::REGENERATION_NETWORK));
    Config::SetDefault("ns3::SatGwMac::SendNcrBroadcast", BooleanValue(false));
    Config::SetDefault("ns3::SatHelper::GwUsers", UintegerValue(1));
    Config::SetDefault("ns3::SatHelper::BeamNetworkAddress", Ipv4AddressValue("20.1.0.0"));
    Config::SetDefault("ns3::SatHelper::GwNetworkAddress", Ipv4AddressValue("10.1.0.0"));
    Config::SetDefault("ns3::SatHelper::UtNetworkAddress", Ipv4AddressValue("250.1.0.0"));
    Config::SetDefault("ns3::SatBbFrameConf::AcmEnabled", BooleanValue(true));
    simulation->LoadScenario(scenarioFolder);
    simulation->SetBeamSet(std::set<uint32_t>{43, 30});
    simulation->SetUserCountPerUt(1);
    simulation->SetUtCountPerBeam(1);
    simulation->SetSimulationTime(Seconds(20));
    simulation->CreateSatScenario();
    auto topology = Singleton<SatTopology>::Get();
    auto clients = topology->GetUtUserNodes(topology->GetUtNodes());
    auto servers = topology->GetGwUserNodes();
    NS_ABORT_MSG_IF(!clients.GetN() || !servers.GetN(), "Scenario needs ground endpoints");
    auto helper = simulation->GetSatelliteHelper();
    auto commander = CreateObject<RelayApplication>();
    auto executor = CreateObject<RelayApplication>();
    Time deadline = Seconds(testCase == "deadline" ? 1.05 : 19);
    commander->Configure(false, InetSocketAddress(helper->GetUserAddress(servers.Get(0)), 7227), testCase, deadline);
    executor->Configure(true, InetSocketAddress(helper->GetUserAddress(clients.Get(0)), 7227), testCase, deadline);
    clients.Get(0)->AddApplication(commander);
    servers.Get(0)->AddApplication(executor);
    executor->SetStartTime(Seconds(0.5));
    commander->SetStartTime(Seconds(1));
    simulation->RunSimulation();
    bool denied = testCase == "wrong-subject" || testCase == "expired";
    bool missed = testCase == "deadline";
    bool passed = denied ? executor->executions == 0 && executor->rejections > 0 :
                  missed ? executor->executions == 0 && !commander->receiptReceived :
                  executor->executions == 1 && commander->receiptReceived;
    if (testCase == "conflict") passed &= executor->rejections == 1;
    if (testCase == "lost-receipt") passed &= commander->attempts >= 2;
    std::cout << "RESULT {\"case\":\"" << testCase << "\",\"passed\":" << (passed ? "true" : "false")
              << ",\"executions\":" << executor->executions << ",\"rejections\":" << executor->rejections
              << ",\"attempts\":" << commander->attempts << ",\"receipt\":" << commander->receiptReceived
              << ",\"delivered_s\":" << executor->deliveredAt << ",\"receipt_s\":" << commander->receiptAt
              << ",\"application_bytes_sent\":" << commander->transmittedBytes + executor->transmittedBytes << "}\n";
    return passed ? 0 : 1;
}
