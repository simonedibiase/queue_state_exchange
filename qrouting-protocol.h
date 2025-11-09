#ifndef QROUTING_PROTOCOL_H
#define QROUTING_PROTOCOL_H

#include "action.h"

#include "ns3/ipv6-address.h"
#include "ns3/ipv6-routing-protocol.h"
#include "ns3/net-device.h"
#include "ns3/ptr.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ns3
{

class QRoutingProtocol : public Ipv6RoutingProtocol
{
  public:
    QRoutingProtocol();
    virtual ~QRoutingProtocol();

    // Inizializzazione del protocollo con riferimenti necessari
    void SetNodeName(const std::string& name);
    void SetNodeIdList(const std::vector<std::string>& nodeIds);
    void SetQRegister(std::shared_ptr<std::vector<std::vector<Action>>> qreg);
    void SetAddressToNameMap(const std::map<Ipv6Address, std::string>& addrToName);
    void SetHostMap(const std::map<std::string, Ptr<Node>>& hostMap);

    // Interfaccia Ipv6RoutingProtocol
    virtual Ptr<Ipv6Route> RouteOutput(Ptr<Packet> p,
                                       const Ipv6Header& header,
                                       Ptr<NetDevice> oif,
                                       Socket::SocketErrno& sockerr) override;

    virtual bool RouteInput(
    Ptr<const Packet> p,
    const Ipv6Header& header,
    Ptr<const NetDevice> idev,
    const UnicastForwardCallback& ucb,
    const MulticastForwardCallback& mcb,
    const LocalDeliverCallback& lcb,
    const ErrorCallback& ecb) override;


    virtual void NotifyInterfaceUp(uint32_t interface) override
    {
    }

    virtual void NotifyInterfaceDown(uint32_t interface) override
    {
    }

    virtual void NotifyAddAddress(uint32_t interface, Ipv6InterfaceAddress address) override
    {
    }

    virtual void NotifyRemoveAddress(uint32_t interface, Ipv6InterfaceAddress address) override
    {
    }

    virtual void SetIpv6(Ptr<Ipv6> ipv6) override
    {
        m_ipv6 = ipv6;
    }

    virtual Ptr<Ipv6> GetIpv6() const
    {
        return m_ipv6;
    }


    //aggiunte all ultimo

    void NotifyAddRoute(Ipv6Address dst,
                        Ipv6Prefix prefix,
                        Ipv6Address nextHop,
                        uint32_t interface,
                        Ipv6Address prefixToUse) override;

    void NotifyRemoveRoute(Ipv6Address dst,
                           Ipv6Prefix prefix,
                           Ipv6Address nextHop,
                           uint32_t interface,
                           Ipv6Address prefixToUse) override;

    void PrintRoutingTable(Ptr<OutputStreamWrapper> stream, Time::Unit unit) const override;

    //fine aggiunta



  private:
    Ptr<Ipv6> m_ipv6;
    std::string m_nodeName;
    std::vector<std::string> m_nodeIds;
    std::shared_ptr<std::vector<std::vector<Action>>> m_qregister;
    std::map<Ipv6Address, std::string> m_addrToName;
    std::map<std::string, Ptr<Node>> m_hostMap;

    int IndexOfNodeNameInNodeIds(const std::string& name) const;
    bool FindMinActionForDestinationIndex(int destIndex, Action& outAction) const;
    void PrintInternalState() const;
};







} // namespace ns3

#endif // QROUTING_PROTOCOL_H
