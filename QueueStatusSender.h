#include "ns3/application.h"
#include "ns3/ipv6-address.h"
#include "ns3/ptr.h"
#include "ns3/socket.h"

using namespace ns3;

class QueueStatusApp : public Application
{
  public:
    QueueStatusApp();
    virtual ~QueueStatusApp();

    void Setup(Ptr<NetDevice> device, Ipv6Address source, Ipv6Address destination);

  private:
    virtual void StartApplication() override;
    virtual void StopApplication() override;

    void SendQueueStatus();
    void ScheduleNextQueueStatus();

    EventId m_sendEvent;
    Ptr<NetDevice> m_device;
    Ipv6Address m_destinationAddress;
    Ipv6Address m_sourceAddress;
    bool m_running;
    Ptr<Socket> m_socket;
};
