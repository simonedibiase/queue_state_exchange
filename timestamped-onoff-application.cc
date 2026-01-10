#include "timestamped-onoff-application.h"

#include "ns3/address.h"
#include "ns3/boolean.h"
#include "ns3/data-rate.h"
#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/packet-socket-address.h"
#include "ns3/packet.h"
#include "ns3/pointer.h"
#include "ns3/random-variable-stream.h"
#include "ns3/seq-ts-size-header.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/string.h"
#include "ns3/timestamp-tag.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/uinteger.h"
#include "traffic-type-header.h"

namespace ns3
{

// NS_LOG_COMPONENT_DEFINE("TimeStampedOnOffApplication");

// NS_OBJECT_ENSURE_REGISTERED(TimeStampedOnOffApplication);

TypeId
TimeStampedOnOffApplication::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::TimeStampedOnOffApplication")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<TimeStampedOnOffApplication>()
            .AddAttribute("DataRate",
                          "The data rate in on state.",
                          DataRateValue(DataRate("500kb/s")),
                          MakeDataRateAccessor(&TimeStampedOnOffApplication::m_cbrRate),
                          MakeDataRateChecker())
            .AddAttribute("PacketSize",
                          "The size of packets sent in on state",
                          UintegerValue(512),
                          MakeUintegerAccessor(&TimeStampedOnOffApplication::m_pktSize),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("Remote",
                          "The address of the destination",
                          AddressValue(),
                          MakeAddressAccessor(&TimeStampedOnOffApplication::m_peer),
                          MakeAddressChecker())
            .AddAttribute("Local",
                          "The Address on which to bind the socket. If not set, it is generated "
                          "automatically.",
                          AddressValue(),
                          MakeAddressAccessor(&TimeStampedOnOffApplication::m_local),
                          MakeAddressChecker())
            .AddAttribute("Tos",
                          "The Type of Service used to send IPv4 packets. "
                          "All 8 bits of the TOS byte are set (including ECN bits).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&TimeStampedOnOffApplication::m_tos),
                          MakeUintegerChecker<uint8_t>())
            .AddAttribute("OnTime",
                          "A RandomVariableStream used to pick the duration of the 'On' state.",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&TimeStampedOnOffApplication::m_onTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("OffTime",
                          "A RandomVariableStream used to pick the duration of the 'Off' state.",
                          StringValue("ns3::ConstantRandomVariable[Constant=1.0]"),
                          MakePointerAccessor(&TimeStampedOnOffApplication::m_offTime),
                          MakePointerChecker<RandomVariableStream>())
            .AddAttribute("MaxBytes",
                          "The total number of bytes to send. Once these bytes are sent, "
                          "no packet is sent again, even in on state. The value zero means "
                          "that there is no limit.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&TimeStampedOnOffApplication::m_maxBytes),
                          MakeUintegerChecker<uint64_t>())
            .AddAttribute("Protocol",
                          "The type of protocol to use. This should be "
                          "a subclass of ns3::SocketFactory",
                          TypeIdValue(UdpSocketFactory::GetTypeId()),
                          MakeTypeIdAccessor(&TimeStampedOnOffApplication::m_tid),
                          // This should check for SocketFactory as a parent
                          MakeTypeIdChecker())
            .AddAttribute(
                "EnableSeqTsSizeHeader",
                "Enable use of SeqTsSizeHeader for sequence number and timestamp",
                BooleanValue(false),
                MakeBooleanAccessor(&TimeStampedOnOffApplication::m_enableSeqTsSizeHeader),
                MakeBooleanChecker())
            .AddTraceSource("Tx",
                            "A new packet is created and is sent",
                            MakeTraceSourceAccessor(&TimeStampedOnOffApplication::m_txTrace),
                            "ns3::Packet::TracedCallback")
            .AddTraceSource(
                "TxWithAddresses",
                "A new packet is created and is sent",
                MakeTraceSourceAccessor(&TimeStampedOnOffApplication::m_txTraceWithAddresses),
                "ns3::Packet::TwoAddressTracedCallback")
            .AddTraceSource(
                "TxWithSeqTsSize",
                "A new packet is created with SeqTsSizeHeader",
                MakeTraceSourceAccessor(&TimeStampedOnOffApplication::m_txTraceWithSeqTsSize),
                "ns3::PacketSink::SeqTsSizeCallback")
            .AddAttribute("TrafficType",
                          "0 = normal, 1 = delay-sensitive, 2 = background",
                          UintegerValue(0),
                          MakeUintegerAccessor(&TimeStampedOnOffApplication::m_trafficType),
                          MakeUintegerChecker<uint32_t>(0, 2));
    return tid;
}

TimeStampedOnOffApplication::TimeStampedOnOffApplication()
    : m_socket(nullptr),
      m_connected(false),
      m_residualBits(0),
      m_lastStartTime(Seconds(0)),
      m_totBytes(0),
      m_unsentPacket(nullptr)
{
    // NS_LOG_FUNCTION(this);
}

TimeStampedOnOffApplication::~TimeStampedOnOffApplication()
{
    // NS_LOG_FUNCTION(this);
}

void
TimeStampedOnOffApplication::SetMaxBytes(uint64_t maxBytes)
{
    // NS_LOG_FUNCTION(this << maxBytes);
    m_maxBytes = maxBytes;
}

Ptr<Socket>
TimeStampedOnOffApplication::GetSocket() const
{
    // NS_LOG_FUNCTION(this);
    return m_socket;
}

int64_t
TimeStampedOnOffApplication::AssignStreams(int64_t stream)
{
    // NS_LOG_FUNCTION(this << stream);
    m_onTime->SetStream(stream);
    m_offTime->SetStream(stream + 1);
    return 2;
}

void
TimeStampedOnOffApplication::DoDispose()
{
    // NS_LOG_FUNCTION(this);

    CancelEvents();
    m_socket = nullptr;
    m_unsentPacket = nullptr;
    // chain up
    Application::DoDispose();
}

// Application Methods
void
TimeStampedOnOffApplication::StartApplication() // Called at time specified by Start
{
    // NS_LOG_FUNCTION(this);

    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        int ret = -1;

        // NS_ABORT_MSG_IF(m_peer.IsInvalid(), "'Remote' attribute not properly set");

        if (!m_local.IsInvalid())
        {
            /*
            NS_ABORT_MSG_IF((Inet6SocketAddress::IsMatchingType(m_peer) &&
                             InetSocketAddress::IsMatchingType(m_local)) ||
                                (InetSocketAddress::IsMatchingType(m_peer) &&
                                 Inet6SocketAddress::IsMatchingType(m_local)),
                            "Incompatible peer and local address IP version");*/
            ret = m_socket->Bind(m_local);
        }
        else
        {
            if (Inet6SocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind6();
            }
            else if (InetSocketAddress::IsMatchingType(m_peer) ||
                     PacketSocketAddress::IsMatchingType(m_peer))
            {
                ret = m_socket->Bind();
            }
        }

        if (ret == -1)
        {
            // NS_FATAL_ERROR("Failed to bind socket");
        }

        m_socket->SetConnectCallback(
            MakeCallback(&TimeStampedOnOffApplication::ConnectionSucceeded, this),
            MakeCallback(&TimeStampedOnOffApplication::ConnectionFailed, this));

        if (InetSocketAddress::IsMatchingType(m_peer))
        {
            m_socket->SetIpTos(m_tos); // Affects only IPv4 sockets.
        }
        m_socket->Connect(m_peer);
        m_socket->SetAllowBroadcast(true);
        m_socket->ShutdownRecv();
    }
    m_cbrRateFailSafe = m_cbrRate;

    // Ensure no pending event
    CancelEvents();

    // If we are not yet connected, there is nothing to do here,
    // the ConnectionComplete upcall will start timers at that time.
    // If we are already connected, CancelEvents did remove the events,
    // so we have to start them again.
    if (m_connected)
    {
        ScheduleStartEvent();
    }
}

void
TimeStampedOnOffApplication::StopApplication() // Called at time specified by Stop
{
    // NS_LOG_FUNCTION(this);

    CancelEvents();
    if (m_socket)
    {
        m_socket->Close();
    }
    else
    {
        // NS_LOG_WARN("OnOffApplication found null socket to close in StopApplication");
    }
}

void
TimeStampedOnOffApplication::CancelEvents()
{
    // NS_LOG_FUNCTION(this);

    if (m_sendEvent.IsPending() && m_cbrRateFailSafe == m_cbrRate)
    { // Cancel the pending send packet event
        // Calculate residual bits since last packet sent
        Time delta(Simulator::Now() - m_lastStartTime);
        int64x64_t bits = delta.To(Time::S) * m_cbrRate.GetBitRate();
        m_residualBits += bits.GetHigh();
    }
    m_cbrRateFailSafe = m_cbrRate;
    Simulator::Cancel(m_sendEvent);
    Simulator::Cancel(m_startStopEvent);
    // Canceling events may cause discontinuity in sequence number if the
    // SeqTsSizeHeader is header, and m_unsentPacket is true
    if (m_unsentPacket)
    {
        // NS_LOG_DEBUG("Discarding cached packet upon CancelEvents ()");
    }
    m_unsentPacket = nullptr;
}

// Event handlers
void
TimeStampedOnOffApplication::StartSending()
{
    // NS_LOG_FUNCTION(this);
    m_lastStartTime = Simulator::Now();
    ScheduleNextTx(); // Schedule the send packet event
    ScheduleStopEvent();
}

void
TimeStampedOnOffApplication::StopSending()
{
    // NS_LOG_FUNCTION(this);
    CancelEvents();

    ScheduleStartEvent();
}

// Private helpers
void
TimeStampedOnOffApplication::ScheduleNextTx()
{
    // NS_LOG_FUNCTION(this);

    if (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    {
        /*
        NS_ABORT_MSG_IF(m_residualBits > m_pktSize * 8,
                        "Calculation to compute next send time will overflow");*/
        uint32_t bits = m_pktSize * 8 - m_residualBits;
        // NS_LOG_LOGIC("bits = " << bits);
        Time nextTime(
            Seconds(bits / static_cast<double>(m_cbrRate.GetBitRate()))); // Time till next packet
        // NS_LOG_LOGIC("nextTime = " << nextTime.As(Time::S));
        m_sendEvent = Simulator::Schedule(nextTime, &TimeStampedOnOffApplication::SendPacket, this);
    }
    else
    { // All done, cancel any pending events
        StopApplication();
    }
}

void
TimeStampedOnOffApplication::ScheduleStartEvent()
{ // Schedules the event to start sending data (switch to the "On" state)
    // NS_LOG_FUNCTION(this);

    Time offInterval = Seconds(m_offTime->GetValue());
    // NS_LOG_LOGIC("start at " << offInterval.As(Time::S));
    m_startStopEvent =
        Simulator::Schedule(offInterval, &TimeStampedOnOffApplication::StartSending, this);
}

void
TimeStampedOnOffApplication::ScheduleStopEvent()
{ // Schedules the event to stop sending data (switch to "Off" state)
    // NS_LOG_FUNCTION(this);

    Time onInterval = Seconds(m_onTime->GetValue());
    // NS_LOG_LOGIC("stop at " << onInterval.As(Time::S));
    m_startStopEvent =
        Simulator::Schedule(onInterval, &TimeStampedOnOffApplication::StopSending, this);
}

void
TimeStampedOnOffApplication::SendPacket()
{
    // NS_LOG_FUNCTION(this);

    // NS_ASSERT(m_sendEvent.IsExpired());

    Ptr<Packet> packet;

    if (m_unsentPacket)
    {
        packet = m_unsentPacket;
    }
    else if (m_enableSeqTsSizeHeader)
    {
        Address from;
        Address to;
        m_socket->GetSockName(from);
        m_socket->GetPeerName(to);
        SeqTsSizeHeader header;
        header.SetSeq(m_seq++);
        header.SetSize(m_pktSize);
        NS_ABORT_IF(m_pktSize < header.GetSerializedSize());
        packet = Create<Packet>(m_pktSize - header.GetSerializedSize());
        // Trace before adding header, for consistency with PacketSink
        m_txTraceWithSeqTsSize(packet, from, to, header);
        packet->AddHeader(header);
    }
    else
    {
        packet = Create<Packet>(m_pktSize);
    }

    TrafficTypeHeader tHeader;
    switch (m_trafficType)
    {
    case 0:
        tHeader.SetType(TrafficTypeHeader::NORMAL);
        break;
    case 1:
        tHeader.SetType(TrafficTypeHeader::DELAY_SENSITIVE);
        break;
    case 2:
        tHeader.SetType(TrafficTypeHeader::BACKGROUND);
        break;
    }
    packet->AddHeader(tHeader);

    TimestampTag tag;
    tag.SetTimestamp(Simulator::Now());
    TimestampTag existing;
    if (!packet->PeekPacketTag(existing))
    {
        packet->AddPacketTag(tag);
    }

    int actual = m_socket->Send(packet);
    if (actual >= 0)
    {
        m_txTrace(packet);
        m_totBytes += packet->GetSize();
        m_unsentPacket = nullptr;
        Address localAddress;
        m_socket->GetSockName(localAddress);
        if (InetSocketAddress::IsMatchingType(m_peer))
        {
            /*
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " on-off application sent "
                                   << packet->GetSize() << " bytes to "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetIpv4() << " port "
                                   << InetSocketAddress::ConvertFrom(m_peer).GetPort()
                                   << " total Tx " << m_totBytes << " bytes");*/
            m_txTraceWithAddresses(packet, localAddress, InetSocketAddress::ConvertFrom(m_peer));
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peer))
        {
            /*
            NS_LOG_INFO("At time " << Simulator::Now().As(Time::S) << " on-off application sent "
                                   << packet->GetSize() << " bytes to "
                                   << Inet6SocketAddress::ConvertFrom(m_peer).GetIpv6() << " port "
                                   << Inet6SocketAddress::ConvertFrom(m_peer).GetPort()
                                   << " total Tx " << m_totBytes << " bytes");*/
            m_txTraceWithAddresses(packet, localAddress, Inet6SocketAddress::ConvertFrom(m_peer));
        }
    }
    else
    {
        // NS_LOG_DEBUG("Unable to send packet; actual " << actual << " size " << m_pktSize
        //                                               << "; caching for later attempt");
        m_unsentPacket = packet;
    }
    m_residualBits = 0;
    m_lastStartTime = Simulator::Now();
    ScheduleNextTx();
}

void
TimeStampedOnOffApplication::ConnectionSucceeded(Ptr<Socket> socket)
{
    // NS_LOG_FUNCTION(this << socket);

    ScheduleStartEvent();
    m_connected = true;
}

void
TimeStampedOnOffApplication::ConnectionFailed(Ptr<Socket> socket)
{
    // NS_LOG_FUNCTION(this << socket);
    // NS_FATAL_ERROR("Can't connect");
}

} // Namespace ns3