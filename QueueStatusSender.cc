#include "QueueStatusSender.h"

#include "ns3/core-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/queue-disc.h"
#include "ns3/queue.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/ipv6.h"

#include <arpa/inet.h> // Per ntohl

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QueueStatusSender");

QueueStatusApp::QueueStatusApp()
    : m_device(0),
      m_socket(0),
      m_running(false)
{
}

QueueStatusApp::~QueueStatusApp()
{
    m_socket = 0;
}

void
QueueStatusApp::Setup(Ptr<NetDevice> device, Ipv6Address source, Ipv6Address destination)
{
    m_device = device;
    m_destinationAddress = destination;
    m_sourceAddress = source;
}

void
QueueStatusApp::StartApplication()
{
    NS_LOG_INFO("Starting QueueStatusApp");
    m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::Ipv6RawSocketFactory"));
    m_socket->SetAttribute("Protocol", UintegerValue(200));
    m_socket->Bind();

    m_running = true;

    Time sendTime = Seconds(2.1);
    m_sendEvent =
        Simulator::Schedule(sendTime - Simulator::Now(), &QueueStatusApp::SendQueueStatus, this);
}

void
QueueStatusApp::StopApplication()
{
    NS_LOG_INFO("Stopping QueueStatusApp");
    m_running = false;
    if (m_sendEvent.IsRunning())
    {
        Simulator::Cancel(m_sendEvent);
    }

    if (m_socket)
    {
        m_socket->Close();
    }
}

void
QueueStatusApp::SendQueueStatus()
{
    NS_LOG_INFO("Sto mandando il pacchetto di stato della coda!");
    std::cout << "Sto mandando il pacchetto di stato della coda!" << std::endl;

    if (!m_running)
        return;

    // Leggi lo stato della coda dal Traffic Control Layer
    Ptr<Node> node = GetNode();
    uint32_t nodeId = node->GetId();
    Ptr<TrafficControlLayer> tc = node->GetObject<TrafficControlLayer>();

    std::vector<uint8_t> buffer; // Buffer per costruire il pacchetto

    for (uint32_t i = 0; i < node->GetNDevices(); ++i)
    {
        Ptr<NetDevice> device = node->GetDevice(i);
        Ptr<QueueDisc> queueDisc = tc->GetRootQueueDiscOnDevice(device);

        if (queueDisc)
        {
            std::cout << "Interfaccia " << i << " ha una coda con " << queueDisc->GetNPackets()
                      << " pacchetti." << std::endl;

            uint32_t queueSize = queueDisc->GetNPackets();
            queueSize = htonl(queueSize); // network byte order

            // ID del nodo (4 byte)
            uint32_t nodeIdNetwork = htonl(nodeId);
            uint8_t* nid = reinterpret_cast<uint8_t*>(&nodeIdNetwork);
            buffer.insert(buffer.end(), nid, nid + sizeof(nodeIdNetwork));

            // Indirizzo IPv6 (16 byte)
            Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
            Ipv6Address addr = ipv6->GetAddress(i, 1).GetAddress();
            uint8_t addrBytes[16];
            addr.Serialize(addrBytes);
            buffer.insert(buffer.end(), addrBytes, addrBytes + 16);

            // Queue size (4 byte)
            uint8_t* qs = reinterpret_cast<uint8_t*>(&queueSize);
            buffer.insert(buffer.end(), qs, qs + sizeof(queueSize));

            std::cout << "[Time: " << Simulator::Now().GetSeconds() << "s] "
                      << "Sender Node: " << nodeId << ", Interfaccia: " << addr
                      << ", Queue Size: " << ntohl(queueSize) << std::endl;

            std::cout << "-----------------------------------------" << std::endl;
        }
        else
        {
            std::cout << "Interfaccia " << i << " NON ha una QueueDisc." << std::endl;
        }
    }

    Ptr<Packet> packet = Create<Packet>(buffer.data(), buffer.size());

    std::cout << "[DEBUG] Pacchetto costruito, dimensione totale: " << buffer.size() << " byte ("
              << buffer.size() / 24 << " code)" << std::endl;


    m_socket->SendTo(packet, 0, Inet6SocketAddress(m_destinationAddress, 0));
    
    ScheduleNextQueueStatus();

}

void 
QueueStatusApp::ScheduleNextQueueStatus()
{
    m_sendEvent = Simulator::Schedule(Seconds(0.4), &QueueStatusApp::SendQueueStatus, this);
}
