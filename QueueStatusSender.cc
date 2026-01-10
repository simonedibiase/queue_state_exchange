#include "QueueStatusSender.h"

#include "dag_database.h"

#include "ns3/core-module.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-l3-protocol.h"
#include "ns3/ipv6.h"
#include "ns3/log.h"
#include "ns3/packet.h"
#include "ns3/queue-disc.h"
#include "ns3/queue.h"
#include "ns3/traffic-control-layer.h"

#include <arpa/inet.h> // Per ntohl

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("QueueStatusSender");

QueueStatusApp::QueueStatusApp()
    : m_socket(0),
      m_running(false)
{
}

QueueStatusApp::~QueueStatusApp()
{
    m_socket = 0;
}

void
QueueStatusApp::Setup(Ipv6Address source,
                      Ipv6Address destination,
                      std::string nameSource,
                      std::string nameDestination,
                      std::shared_ptr<std::vector<std::vector<Action>>> q_registerSource,
                      int32_t indexNodeDestination)
{
    m_destinationAddress = destination;
    m_sourceAddress = source;
    m_nameSource = nameSource;
    m_nameDestination = nameDestination;
    m_q_registerSource = q_registerSource;
    m_indexNodeDestination = indexNodeDestination;
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

std::vector<uint32_t>
QueueStatusApp::selectQRegisterLines(int32_t indexNodeDestination, std::string nameSource)
{
    std::vector<Dag> dags = LoadDags();
    std::vector<uint32_t> selectedLines;

    for (uint32_t i = 0; i < dags.size(); ++i)
    {
        const auto& dag = dags[i];

        if (indexNodeDestination >= 0 &&
            static_cast<size_t>(indexNodeDestination) < dag.adjacency_list.size())
        {
            const auto& row = dag.adjacency_list[indexNodeDestination];

            if (std::find(row.begin(), row.end(), nameSource) != row.end())
            {
                selectedLines.push_back(i);
            }
        }
    }

    return selectedLines;
}

uint32_t
QueueStatusApp::GetMinQValueInRow(const std::vector<Action>& row)
{
    if (row.empty())
        return 0;

    auto it = std::min_element(row.begin(), row.end(), [](const Action& a, const Action& b) {
        return a.q_value < b.q_value;
    });
    return it->q_value;
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
QueueStatusApp::PrintQRegisterForNode(
    const std::string& nameSource,
    const std::shared_ptr<std::vector<std::vector<Action>>>& q_registerSource)
{
    std::cout << "=== Q-Register del nodo: " << nameSource << " ===\n";

    if (!q_registerSource)
    {
        std::cout << "[WARN] q_registerSource è nullptr\n";
        return;
    }

    const auto& qRegister = *q_registerSource;

    for (size_t rowIndex = 0; rowIndex < qRegister.size(); ++rowIndex)
    {
        std::cout << "Riga " << rowIndex << ": ";
        for (const auto& action : qRegister[rowIndex])
        {
            std::string outDevStr = "NONE";
            if (action.outDevice)
            {
                outDevStr = "DevicePtr"; // puoi sostituire con info aggiuntiva se vuoi
            }

            std::cout << "[Dest=" << action.idNodeDestination << ", q=" << action.q_value
                      << ", outDevice=" << outDevStr << "] ";
        }
        std::cout << "\n";
    }

    std::cout << "-------------------------------------------\n";
}

void
QueueStatusApp::SendQueueStatus()
{
    Time now = Simulator::Now();

    //PrintQRegisterForNode(m_nameSource, m_q_registerSource);
    /*std::cout << "[Sender] Time:" << now.GetSeconds()
              << "s Sto mandando il pacchetto contenente i valori q, destinato a: "
              << m_nameDestination << std::endl;
*/
    // individua le righe da selezionare
    std::vector<uint32_t> selectedLines =
        selectQRegisterLines(m_indexNodeDestination, m_nameSource);

    if (!m_running)
        return;

    std::vector<uint8_t> buffer; // buffer che conterrà i dati del pacchetto

    for (uint32_t lineIndex : selectedLines)
    {
        if (lineIndex < m_q_registerSource->size() && !(*m_q_registerSource)[lineIndex].empty())
        {
            const auto& row = (*m_q_registerSource)[lineIndex];

            // inserisco il valore minimo della riga
            uint32_t minQValue = GetMinQValueInRow(row);
            uint32_t minQValueNetwork = htonl(minQValue);
            uint8_t* qv = reinterpret_cast<uint8_t*>(&minQValueNetwork);
            buffer.insert(buffer.end(), qv, qv + sizeof(minQValueNetwork));

            // inserisco l' indice della riga
            uint32_t lineIndexNetwork = htonl(lineIndex);
            uint8_t* li = reinterpret_cast<uint8_t*>(&lineIndexNetwork);
            buffer.insert(buffer.end(), li, li + sizeof(lineIndexNetwork));

            // inserisco il nome del nodo sorgente
            uint32_t nameLen = m_nameSource.size();
            uint32_t nameLenNetwork = htonl(nameLen);
            uint8_t* nl = reinterpret_cast<uint8_t*>(&nameLenNetwork);
            buffer.insert(buffer.end(), nl, nl + sizeof(nameLenNetwork));

            buffer.insert(buffer.end(), m_nameSource.begin(), m_nameSource.end());

            //std::cout << "[SENDER] lineIndex=" << lineIndex << ", minQValue=" << minQValue
            //          << ", nameSource=" << m_nameSource << "n nameDestination: " << m_nameDestination << std::endl;
        }
        else
        {
            std::cout << "[WARN] Riga " << lineIndex << " non valida nella q_register di "
                      << m_nameSource << std::endl;
        }
    }

    // costruisco il pacchetto
    Ptr<Packet> packet = Create<Packet>(buffer.data(), buffer.size());

    //std::cout << "[DEBUG] Pacchetto costruito, dimensione totale: " << buffer.size() << " byte"
    //          << std::endl;

    // invio pacchetto
    m_socket->SendTo(packet, 0, Inet6SocketAddress(m_destinationAddress, 0));

    ScheduleNextQueueStatus();
}

void
QueueStatusApp::ScheduleNextQueueStatus()
{
    m_sendEvent = Simulator::Schedule(Seconds(0.01), &QueueStatusApp::SendQueueStatus, this);
}
