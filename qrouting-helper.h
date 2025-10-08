#ifndef QROUTING_HELPER_H
#define QROUTING_HELPER_H

#include "action.h"
#include "qrouting-protocol.h"

#include "ns3/ipv6-routing-helper.h"
#include "ns3/ipv6.h"
#include "ns3/node.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ns3
{

class QRoutingHelper : public Ipv6RoutingHelper
{
  public:
    QRoutingHelper();
    virtual ~QRoutingHelper();

    // Costruttore alternativo per passare le strutture dal main
    QRoutingHelper(
        std::map<std::string, Ptr<Node>>* nodeMap,
        std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>>* nameToQRegister,
        const std::vector<std::string>& nodeIds,
        const std::map<Ipv6Address, std::string>& ipv6ToHostName);
    // Ipv6RoutingHelper API
    virtual Ptr<Ipv6RoutingProtocol> Create(Ptr<Node> node) const override;
    virtual Ipv6RoutingHelper* Copy() const override;

  private:
    // Punteri alle strutture originarie; non vengono copiate per efficienza (il helper vive nel
    // main)
    std::map<std::string, Ptr<Node>>* m_nodeMap;
    std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>>* m_nameToQRegister;
    std::vector<std::string> m_nodeIds;
    std::map<Ipv6Address, std::string> m_addrToName;
};

} // namespace ns3

#endif // QROUTING_HELPER_H
