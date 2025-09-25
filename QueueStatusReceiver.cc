#include "QueueStatusReceiver.h"

#include "ns3/address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-raw-socket-factory.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/core-module.h"
#include "csv_logger.h";

#include <arpa/inet.h> // Per ntohl

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QueueStatusReceiver");

TypeId
QueueStatusReceiver::GetTypeId(void)
{
    static TypeId tid = TypeId("ns3::QueueStatusReceiver")
                            .SetParent<Application>()
                            .SetGroupName("Tutorial")
                            .AddConstructor<QueueStatusReceiver>();
    return tid;
}

QueueStatusReceiver::QueueStatusReceiver()
{
}

QueueStatusReceiver::~QueueStatusReceiver()
{
}

const std::vector<uint32_t>&
QueueStatusReceiver::GetReceivedQueueSizes() const
{
    return m_receivedQueueSizes;
}

const std::vector<QueueInfo>&
QueueStatusReceiver::GetReceivedQueueInfo() const
{
    return m_receivedQueueInfo;
}

void
QueueStatusReceiver::StartApplication()
{
    NS_LOG_INFO("Starting QueueStatusReceiver");
    m_socket = Socket::CreateSocket(GetNode(), TypeId::LookupByName("ns3::Ipv6RawSocketFactory"));
    m_socket->SetAttribute("Protocol", UintegerValue(200));
    m_socket->Bind(Inet6SocketAddress(Ipv6Address::GetAny(), 0));
    m_socket->SetRecvCallback(MakeCallback(&QueueStatusReceiver::HandleRead, this));
}

void
QueueStatusReceiver::StopApplication()
{
    NS_LOG_INFO("Stopping QueueStatusReceiver");
    if (m_socket)
    {
        m_socket->Close();
        m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
        m_socket = nullptr;
    }
}

void
QueueStatusReceiver::HandleRead(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address from;
    while ((packet = socket->RecvFrom(from)))
    {
        Time now = Simulator::Now();
        NS_LOG_INFO("QueueStatusReceiverApplication: Ricevuto un pacchetto di " << packet->GetSize()
                                                                                << " byte");

        if (packet->GetSize() >= 24) 
        {
            packet->RemoveAtStart(40); // Skippa header IPv6

            uint32_t numEntries = packet->GetSize() / 24;
            uint8_t* buffer = new uint8_t[packet->GetSize()];
            //uint32_t queueSize;
            //packet->CopyData(reinterpret_cast<uint8_t*>(&queueSize), sizeof(queueSize));
            packet->CopyData(buffer, packet->GetSize());

            for(uint32_t i = 0; i < numEntries; ++i){

                uint32_t offset = i * 24;

                // Node ID (4 byte)
                uint32_t nodeId;
                std::memcpy(&nodeId, buffer + offset, 4);
                nodeId = ntohl(nodeId);

                // IPv6 address (16 byte)
                uint8_t rawAddr[16];
                std::memcpy(rawAddr, buffer + offset + 4, 16);
                Ipv6Address addr = Ipv6Address(rawAddr);

                // Queue Size (4 byte)
                uint32_t queueSize;
                std::memcpy(&queueSize, buffer + offset + 20, 4);
                queueSize = ntohl(queueSize);
                
                std::cout << "[Time: " << now.GetSeconds() << "s] "
                          << "Ricevuta info - Nodo ID: " << nodeId << ", Interfaccia: " << addr
                          << ", Queue Size: " << queueSize << std::endl;

                UpdateOrAddQueueInfo(nodeId, addr, queueSize);
            }

            delete [] buffer;

        }
    }
}

void
QueueStatusReceiver::UpdateOrAddQueueInfo(uint32_t nodeId,
                                     Ipv6Address interfaceAddress,
                                     uint32_t queueSize)
{
    for (auto& info : m_receivedQueueInfo) {
        if(info.nodeId == nodeId && info.interfaceAddress == interfaceAddress)
        {
            info.queueSize = queueSize;
            // salvo le informazioni nel csv
            double now = Simulator::Now().GetSeconds();

            csvFile << now << "," << GetNode()->GetId() << "," // ricevente
                    << nodeId << ","                           // mittente
                    << interfaceAddress << ","                 // interfaccia del mittenteà
                    << queueSize << std::endl;
            return;
        }
    }

    // se non trovo niente, inserisco le info per la prima volta
    m_receivedQueueInfo.push_back({nodeId, interfaceAddress, queueSize});

    // salvo le informazioni della prima ricezione nel csv
    double now = Simulator::Now().GetSeconds();
    
    csvFile << now << ","
            << GetNode()->GetId() << "," // ricevente
            << nodeId << ","             // mittente
            << interfaceAddress << ","   // interfaccia del mittenteà
            << queueSize << std::endl;
}

} // namespace ns3
