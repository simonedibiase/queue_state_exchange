#include "qrouting-helper.h"

#include "ns3/ipv6.h"
#include "ns3/log.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("QRoutingHelper");

QRoutingHelper::QRoutingHelper()
    : m_nodeMap(nullptr),
      m_nameToQRegister(nullptr)
{
}

QRoutingHelper::~QRoutingHelper()
{
}

QRoutingHelper::QRoutingHelper(
    std::map<std::string, Ptr<Node>>* nodeMap,
    std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>>* nameToQRegister,
    const std::vector<std::string>& nodeIds,
    const std::map<Ipv6Address, std::string>& ipv6ToHostName)
    : m_nodeMap(nodeMap),
      m_nameToQRegister(nameToQRegister),
      m_nodeIds(nodeIds),
      m_addrToName(ipv6ToHostName) // ✅ assegnamento diretto
{
}

Ptr<Ipv6RoutingProtocol>
QRoutingHelper::Create(Ptr<Node> node) const
{
    // crea un'istanza del protocollo e popola i riferimenti
    Ptr<QRoutingProtocol> proto = CreateObject<QRoutingProtocol>();
    proto->SetIpv6(node->GetObject<Ipv6>());

    // ricava il nome del nodo (cerco nella mappa m_nodeMap per capire quale nome corrisponde a
    // questo Ptr<Node>)
    if (m_nodeMap)
    {
        for (const auto& entry : *m_nodeMap)
        {
            if (entry.second == node)
            {
                std::string nodeName = entry.first;
                proto->SetNodeName(nodeName);
                break;
            }
        }
    }

    proto->SetNodeIdList(m_nodeIds);

    // assegna il q_register relativo a questo node, se presente
    if (m_nameToQRegister)
    {
        if (m_nodeMap)
        {
            for (const auto& entry : *m_nodeMap)
            {
                if (entry.second == node)
                {
                    auto it = m_nameToQRegister->find(entry.first);
                    if (it != m_nameToQRegister->end())
                    {
                        proto->SetQRegister(it->second);
                    }
                    break;
                }
            }
        }
    }

    // assegna mappa addr->name
    proto->SetAddressToNameMap(m_addrToName);

    return proto;
}

Ipv6RoutingHelper*
QRoutingHelper::Copy() const
{
    // ritorna un helper identico (non profondo)
    QRoutingHelper* helper = new QRoutingHelper();
    helper->m_nodeMap = m_nodeMap;
    helper->m_nameToQRegister = m_nameToQRegister;
    helper->m_nodeIds = m_nodeIds;
    helper->m_addrToName = m_addrToName;
    return helper;
}

} // namespace ns3
