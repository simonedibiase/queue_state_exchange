#ifndef ACTION_H
#define ACTION_H

#include "ns3/net-device.h"
#include "ns3/ptr.h"

#include <cstdint>
#include <string>

// Forward declaration: basta dichiarare che esiste la classe NetDevice
namespace ns3
{
class NetDevice;
}

// struttura che memorizza il nodo, la coda e l'interfaccia di uscita
// per ogni elemento del Q-register
struct Action
{
    std::string idNodeDestination;
    std::uint32_t q_value;
    ns3::Ptr<ns3::NetDevice> outDevice; // dispositivo di uscita
};

#endif // ACTION_H
