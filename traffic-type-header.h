#pragma once
#include "ns3/header.h"
#include "ns3/uinteger.h"

namespace ns3
{

class TrafficTypeHeader : public Header
{
  public:
    enum Type
    {
        NORMAL = 0,
        DELAY_SENSITIVE = 1
    };

    TrafficTypeHeader()
        : m_type(NORMAL)
    {
    }

    TrafficTypeHeader(Type t)
        : m_type(t)
    {
    }

    void SetType(Type t)
    {
        m_type = t;
    }

    Type GetType() const
    {
        return m_type;
    }

    static TypeId GetTypeId(void)
    {
        static TypeId tid = TypeId("ns3::TrafficTypeHeader")
                                .SetParent<Header>()
                                .AddConstructor<TrafficTypeHeader>();
        return tid;
    }

    virtual TypeId GetInstanceTypeId(void) const override
    {
        return GetTypeId();
    }

    virtual uint32_t GetSerializedSize(void) const override
    {
        return 1; // solo 1 byte
    }

    virtual void Serialize(Buffer::Iterator start) const override
    {
        start.WriteU8(static_cast<uint8_t>(m_type));
    }

    virtual uint32_t Deserialize(Buffer::Iterator start) override
    {
        m_type = static_cast<Type>(start.ReadU8());
        return 1;
    }

    virtual void Print(std::ostream& os) const override
    {
        os << "type=" << (int)m_type;
    }

  private:
    Type m_type;
};

} // namespace ns3
