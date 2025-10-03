#include "action.h"

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

    void Setup(Ipv6Address source,
               Ipv6Address destination,
               std::string nameSource,
               std::string nameDestination,
               std::shared_ptr<std::vector<std::vector<Action>>> q_registerSource,
               int32_t indexNodeDestination);

  private:
    virtual void StartApplication() override;
    virtual void StopApplication() override;


    void SendQueueStatus();
    void ScheduleNextQueueStatus();
    std::vector<uint32_t> selectQRegisterLines(int32_t indexNodeDestination,
                                               std::string nameSource);
    uint32_t GetMinQValueInRow(const std::vector<Action>& row);
    void PrintQRegisterForNode(
        const std::string& nameSource,
        const std::shared_ptr<std::vector<std::vector<Action>>>& q_registerSource);

    EventId m_sendEvent;
    Ptr<NetDevice> m_device;
    Ipv6Address m_destinationAddress;
    Ipv6Address m_sourceAddress;
    bool m_running;
    Ptr<Socket> m_socket;
    std:: string m_nameSource;
    std:: string m_nameDestination;
    std::shared_ptr<std::vector<std::vector<Action>>> m_q_registerSource;
    std:: int32_t m_indexNodeDestination;
};
