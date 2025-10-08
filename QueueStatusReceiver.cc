#include "QueueStatusReceiver.h"

#include "ns3/address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-raw-socket-factory.h"
#include "ns3/log.h"
#include "ns3/uinteger.h"
#include "ns3/simulator.h"
#include "ns3/traffic-control-layer.h"
#include "ns3/queue-disc.h"

#include <arpa/inet.h> // Per ntohl
#include "csv_logger.h"

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

void
QueueStatusReceiver::SetQRegister(std::shared_ptr<std::vector<std::vector<Action>>> q)
{
    m_q_register = q;
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
        packet->RemoveAtStart(40); // Skippa header IPv6
        uint32_t totalSize = packet->GetSize();
        std::vector<uint8_t> buffer(totalSize);
        packet->CopyData(buffer.data(), totalSize);

        size_t offset = 0;
        while (offset + 12 <= totalSize) // servono almeno minQValue + lineIndex + nameLen
        {
            // 1) leggi minQValue
            uint32_t minQValue;
            std::memcpy(&minQValue, buffer.data() + offset, 4);
            minQValue = ntohl(minQValue);
            offset += 4;

            // 2) leggi lineIndex
            uint32_t lineIndex;
            std::memcpy(&lineIndex, buffer.data() + offset, 4);
            lineIndex = ntohl(lineIndex);
            offset += 4;

            // 3) leggi nameLen
            uint32_t nameLen;
            std::memcpy(&nameLen, buffer.data() + offset, 4);
            nameLen = ntohl(nameLen);
            offset += 4;

            // 4) leggi nameSource (stringa)
            if(offset + nameLen > totalSize)
                break;

            std::string nameSource(reinterpret_cast<char*>(buffer.data() + offset), nameLen);
            offset += nameLen;
            
            std::cout << "[Receiver]  Time:" << now.GetSeconds() << "s " << " lineIndex="
                      << lineIndex << ", minQValue=" << minQValue << ", nameSource=" << nameSource
                      << std::endl;

            // --- AGGIORNAMENTO m_q_register ---
            if (lineIndex < m_q_register->size())
            {
                auto& row = (*m_q_register)[lineIndex];
                for (auto& action : row)
                {
                    if (action.idNodeDestination == nameSource)
                    {
                        uint32_t queueSize = 0;

                        if (action.outDevice)
                        {
                            Ptr<TrafficControlLayer> tc =
                                action.outDevice->GetNode()->GetObject<TrafficControlLayer>();
                            if (tc)
                            {
                                Ptr<QueueDisc> qdisc =
                                    tc->GetRootQueueDiscOnDevice(action.outDevice);
                                if (qdisc)
                                {
                                    queueSize =
                                        qdisc->GetNPackets(); // o GetCurrentSize().GetValue() per
                                                              // byte
                                }
                            }
                        }

                        action.q_value = minQValue + queueSize;
                    }
                }
            }
        }
    }
}
}
