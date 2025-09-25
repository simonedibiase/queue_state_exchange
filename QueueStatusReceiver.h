#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-raw-socket-factory.h"
#include "ns3/log.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"

#include <vector>

namespace ns3
{

struct QueueInfo
{
    uint32_t nodeId;
    Ipv6Address interfaceAddress;
    uint32_t queueSize;
};

class QueueStatusReceiver : public Application
{
  public:
    static TypeId GetTypeId(void);

    QueueStatusReceiver();

    virtual ~QueueStatusReceiver();

    const std::vector<uint32_t>& GetReceivedQueueSizes() const;
    const std::vector<QueueInfo>& GetReceivedQueueInfo() const;

  protected:
    virtual void StartApplication() override;
    virtual void StopApplication() override;

  private:
    void HandleRead(Ptr<Socket> socket);
    void UpdateOrAddQueueInfo(uint32_t nodeId, Ipv6Address interfaceAddress, uint32_t queueSize);

    Ptr<Socket> m_socket;
    std::vector<uint32_t> m_receivedQueueSizes;
    std::vector<QueueInfo> m_receivedQueueInfo;
};

} // namespace ns3