#include "qrouting-protocol.h"

#include "ns3/ipv6-address.h"
#include "ns3/ipv6-header.h"
#include "ns3/ipv6-route.h"
#include "ns3/ipv6.h"
#include "ns3/log.h"
#include "ns3/net-device.h"
#include "ns3/packet.h"
#include "ns3/socket.h"
#include "ns3/node.h"

#include <limits>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QRoutingProtocol");

QRoutingProtocol::QRoutingProtocol()
{
}

QRoutingProtocol::~QRoutingProtocol()
{
}

void
QRoutingProtocol::SetNodeName(const std::string& name)
{
    m_nodeName = name;
}

void
QRoutingProtocol::SetNodeIdList(const std::vector<std::string>& nodeIds)
{
    m_nodeIds = nodeIds;
}

void
QRoutingProtocol::SetQRegister(std::shared_ptr<std::vector<std::vector<Action>>> qreg)
{
    m_qregister = qreg;
}

void
QRoutingProtocol::SetAddressToNameMap(const std::map<Ipv6Address, std::string>& addrToName)
{
    m_addrToName = addrToName;
}

int
QRoutingProtocol::IndexOfNodeNameInNodeIds(const std::string& name) const
{
    for (size_t i = 0; i < m_nodeIds.size(); ++i)
    {
        if (m_nodeIds[i] == name)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool
QRoutingProtocol::FindMinActionForDestinationIndex(int destIndex, Action& outAction) const
{
    if (!m_qregister)
    {
        return false;
    }

    if (destIndex < 0 || static_cast<size_t>(destIndex) >= m_qregister->size())
    {
        return false;
    }

    const auto& row = (*m_qregister)[destIndex];
    if (row.empty())
    {
        return false;
    }

    bool found = false;
    std::uint32_t minQ = std::numeric_limits<std::uint32_t>::max();
    for (const auto& a : row)
    {
        if (a.outDevice != nullptr)
        {
            if (!found || a.q_value < minQ)
            {
                outAction = a;
                minQ = a.q_value;
                found = true;
            }
        }
    }
    return found;
}

Ptr<Ipv6Route>
QRoutingProtocol::RouteOutput(Ptr<Packet> p,
                              const Ipv6Header& header,
                              Ptr<NetDevice> oif,
                              Socket::SocketErrno& sockerr)
{
    Ipv6Address dst = header.GetDestination();
    std::cout << "[QROUTING] Entrato in RouteOutput verso " << dst << std::endl;

    // Imposta default errore
    sockerr = Socket::ERROR_NOROUTETOHOST;

    // Ignora pacchetti verso indirizzi non globali
    if (dst.IsLinkLocal() || dst.IsMulticast())
    {
        std::cout << "[QROUTING] Ignoro pacchetto verso indirizzo non globale: " << dst << std::endl;
        return nullptr;
    }

    // 1) Risolvi nome del nodo di destinazione dalla mappa addr->nome
    auto it = m_addrToName.find(dst);
    if (it == m_addrToName.end())
    {
        std::cout << "[QROUTING] WARNING OUTPUT: Destination address not found in addr->name map: " << dst << std::endl;
        return nullptr;
    }

    std::string destName = it->second;
    int destIndex = IndexOfNodeNameInNodeIds(destName);
    if (destIndex < 0)
    {
        std::cout << "[QROUTING] WARNING OUTPUT: Destination name not found in nodeIds: "<< destName << std::endl;
        return nullptr;
    }

    // 2) Trova l'action migliore dal Q-table
    Action chosen;
    if (!FindMinActionForDestinationIndex(destIndex, chosen))
    {
        std::cout << "[QROUTING] WARNING OUTPUT: No valid action found for destIndex " << destIndex<< " (" << destName << ")" << std::endl;
        return nullptr;
    }

    if (chosen.outDevice == nullptr)
    {
        std::cout << "[QROUTING] WARNING OUTPUT: Chosen action has no outDevice" << std::endl;
        return nullptr;
    }

    // 3) Costruisci la route
    Ptr<Ipv6Route> route = Create<Ipv6Route>();
    route->SetDestination(dst);
    route->SetOutputDevice(chosen.outDevice);

    // Trova un indirizzo globale dell’interfaccia scelta come sorgente
    if (m_ipv6)
    {
        int32_t ifIndex = m_ipv6->GetInterfaceForDevice(chosen.outDevice);
        if (ifIndex >= 0)
        {
            for (uint32_t i = 0; i < m_ipv6->GetNAddresses(ifIndex); ++i)
            {
                Ipv6InterfaceAddress ifAddr = m_ipv6->GetAddress(ifIndex, i);
                if (!ifAddr.GetAddress().IsLinkLocal())
                {
                    route->SetSource(ifAddr.GetAddress());
                    break; // usa il primo globale disponibile
                }
            }
        }
    }

    std::cout << "[QROUTING] RouteOutput: chosen interface " << chosen.outDevice->GetIfIndex()<< " per destinazione " << dst << std::endl;

    sockerr = Socket::ERROR_NOTERROR;
    return route;
}

bool
QRoutingProtocol::RouteInput(Ptr<const Packet> p,
                             const Ipv6Header& header,
                             Ptr<const NetDevice> idev,
                             const UnicastForwardCallback& ucb,
                             const MulticastForwardCallback& mcb,
                             const LocalDeliverCallback& lcb,
                             const ErrorCallback& ecb)
{
    Ipv6Address dst = header.GetDestination();
    Ipv6Address src = header.GetSource();

    std::cout << "[QROUTING] RouteInput: pacchetto da " << src << " a " << dst << std::endl;

    // Ignora pacchetti link-local o multicast
    if (dst.IsLinkLocal() || dst.IsMulticast())
    {
        std::cout << "[QROUTING] Ignoro pacchetto verso indirizzo non globale: " << dst << std::endl;
        return false;
    }

    // Stampiamo la lista di indirizzi globali locali
    std::cout << "[QROUTING] Lista indirizzi globali locali per questo nodo:" << std::endl;
    if (m_ipv6)
    {
        for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
        {
            for (uint32_t j = 0; j < m_ipv6->GetNAddresses(i); ++j)
            {
                Ipv6Address addr = m_ipv6->GetAddress(i, j).GetAddress();
                if (!addr.IsLinkLocal() && !addr.IsMulticast())
                {
                    std::cout << "  Interface " << i << ": " << addr << std::endl;
                }
            }
        }
    }

    // Controllo se la destinazione è locale
    if (m_ipv6)
    {
        for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
        {
            for (uint32_t j = 0; j < m_ipv6->GetNAddresses(i); ++j)
            {
                Ipv6Address addr = m_ipv6->GetAddress(i, j).GetAddress();
                if (!addr.IsLinkLocal() && !addr.IsMulticast() && addr == dst)
                {
                    std::cout << "[QROUTING] PACCHETTO DESTINAZIONE LOCALE sulla interface " << i << std::endl;
                    lcb(p, header, i);
                    return true;
                }
            }
        }
    }

    // Risolvi nome del nodo di destinazione dalla mappa addr->nome
    auto it = m_addrToName.find(dst);
    if (it == m_addrToName.end())
    {
        std::cout << "[QROUTING] WARNING: Destination address non trovato nella mappa addr->nome: " << dst << std::endl;
        return false;
    }

    std::string destName = it->second;
    int destIndex = IndexOfNodeNameInNodeIds(destName);
    if (destIndex < 0)
    {
        std::cout << "[QROUTING] WARNING: Destination name non trovato in nodeIds: " << destName << std::endl;
        return false;
    }

    // Trova l'action migliore dal Q-table
    Action chosen;
    if (!FindMinActionForDestinationIndex(destIndex, chosen))
    {
        std::cout << "[QROUTING] WARNING: Nessuna action valida trovata per destIndex " << destIndex << " (" << destName << ")" << std::endl;
        return false;
    }

    if (chosen.outDevice == nullptr)
    {
        std::cout << "[QROUTING] WARNING: Chosen action ha outDevice nullo" << std::endl;
        return false;
    }

    // Stampiamo interfaccia su cui rilanciamo il pacchetto
    std::cout << "[QROUTING] Pacchetto inoltrato tramite interface " << chosen.outDevice->GetIfIndex() << std::endl;

    // Costruisci la route e inoltra
    Ptr<Ipv6Route> route = Create<Ipv6Route>();
    route->SetSource(src);
    route->SetDestination(dst);
    route->SetOutputDevice(chosen.outDevice);

    ucb(idev, route, p, header);
    return true;
}

void
QRoutingProtocol::NotifyAddRoute(Ipv6Address dst,
                                 Ipv6Prefix prefix,
                                 Ipv6Address nextHop,
                                 uint32_t interface,
                                 Ipv6Address prefixToUse)
{
    std::cout << "[QROUTING] entrato in :NotifyAddRoute " << std::endl;
    NS_LOG_FUNCTION(this << dst << prefix << nextHop << interface << prefixToUse);
}

void
QRoutingProtocol::NotifyRemoveRoute(Ipv6Address dst,
                                    Ipv6Prefix prefix,
                                    Ipv6Address nextHop,
                                    uint32_t interface,
                                    Ipv6Address prefixToUse)
{
    std::cout << "[QROUTING] entrato in :NotifyRemoveRoute " << std::endl;
    NS_LOG_FUNCTION(this << dst << prefix << nextHop << interface << prefixToUse);
}

void
QRoutingProtocol::PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const
{
    std::ostream* os = stream->GetStream();
    *os << "QRouting table printing not yet implemented." << std::endl;
}

void
QRoutingProtocol::PrintInternalState() const
{
    std::cout << "================= QRoutingProtocol Internal State =================" << std::endl;
    std::cout << "Node name: " << m_nodeName << std::endl;

    // m_ipv6
    if (m_ipv6)
    {
        std::cout << "IPv6 GLOBAL addresses for this node:" << std::endl;
        for (uint32_t i = 0; i < m_ipv6->GetNInterfaces(); ++i)
        {
            bool printedInterface = false;
            for (uint32_t j = 0; j < m_ipv6->GetNAddresses(i); ++j)
            {
                Ipv6InterfaceAddress ifAddr = m_ipv6->GetAddress(i, j);
                Ipv6Address addr = ifAddr.GetAddress();

                // Salta link-local (fe80::/10) e multicast (ff00::/8)
                if (addr.IsLinkLocal() || addr.IsMulticast())
                {
                    continue;
                }

                if (!printedInterface)
                {
                    std::cout << "  Interface " << i << ":" << std::endl;
                    printedInterface = true;
                }

                std::cout << "    " << addr << std::endl;
            }
        }
    }
    else
    {
        std::cout << "IPv6 pointer is NULL." << std::endl;
    }

    // m_nodeIds
    std::cout << "Node IDs list (" << m_nodeIds.size() << "): ";
    for (const auto& id : m_nodeIds)
    {
        std::cout << id << " ";
    }
    std::cout << std::endl;

    // m_addrToName
    std::cout << "Address → Name map (" << m_addrToName.size() << " entries):" << std::endl;
    for (const auto& [addr, name] : m_addrToName)
    {
        std::cout << "  " << addr << " → " << name << std::endl;
    }

    // m_qregister
    if (m_qregister)
    {
        std::cout << "Q-table size: " << m_qregister->size() << " rows" << std::endl;
        for (size_t i = 0; i < m_qregister->size(); ++i)
        {
            std::cout << "  Dest index " << i << " (" << (i < m_nodeIds.size() ? m_nodeIds[i] : "?")
                      << "): ";
            const auto& row = (*m_qregister)[i];
            for (size_t j = 0; j < row.size(); ++j)
            {
                const auto& a = row[j];
                std::cout << "[q=" << a.q_value << ", outDev="
                          << (a.outDevice ? std::to_string(a.outDevice->GetIfIndex()) : "null")
                          << "] ";
            }
            std::cout << std::endl;
        }
    }
    else
    {
        std::cout << "Q-table pointer is NULL." << std::endl;
    }

    std::cout << "===================================================================="
              << std::endl;
}

} // namespace ns3
