// Network topology: Abilene network (https://sndlib.put.poznan.pl/home.action)
// //
// // Nodi totali: {    ATLAM5, ATLAng, CHINng, KSCYng,
// //                   DNVRng, HSTNng, IPLSng, LOSAng,
// //                   NYCMng, SNVAng, STTLng, WASHng      }
// //
// //
// // #link = 15
// //
// //

#include "QueueStatusReceiver.h"
#include "QueueStatusSender.h"
#include "action.h"
#include "csv_logger.h"
#include "dag_database.h"
#include "flow_demand_reader.h"
#include "qrouting-helper.h"

#include "ns3/applications-module.h"
#include "ns3/callback.h"
#include "ns3/channel.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/global-route-manager.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv6-address-helper.h"
#include "ns3/ipv6-address.h"
#include "ns3/ipv6-interface-address.h"
#include "ns3/ipv6-list-routing-helper.h"
#include "ns3/ipv6-routing-table-entry.h"
#include "ns3/ipv6-static-routing-helper.h"
#include "ns3/ipv6.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet.h"
#include "ns3/ping-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/timestamp-tag.h"
#include "ns3/traffic-control-helper.h"
#include "ns3/traffic-control-layer.h"

#include <fstream>
#include <iomanip> // per std::setprecision
#include <iostream>
#include <string>
#include <vector>

std::ofstream csvFile;
using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NeighborsQueueStatusInRealScenario");

// struct per i Collegamenti tra nodi (da XML) con capacità scalata
struct Link
{
    std::string source;
    std::string target;
    double capacityMbps;
};

void
installReceiverExchangeStateAppOnAllNodes(
    std::map<std::string, Ptr<Node>>& nodeMap,
    std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>>& nameToQRegister)
{
    for (const auto& [name, node] : nodeMap)
    {
        auto q_register = nameToQRegister[name];
        Ptr<QueueStatusReceiver> receiverApp = CreateObject<QueueStatusReceiver>();
        receiverApp->SetQRegister(q_register);
        node->AddApplication(receiverApp);
        receiverApp->SetStartTime(Seconds(1.0));
        receiverApp->SetStopTime(Seconds(10.0));
    }
}

void
installBidirectionalQueueStatusSenders(
    Ipv6Address addrA,
    Ipv6Address addrB,
    Ptr<Node> nodeA,
    Ptr<Node> nodeB,
    std::string nameA,
    std::string nameB,
    std::shared_ptr<std::vector<std::vector<Action>>> q_registerA,
    std::shared_ptr<std::vector<std::vector<Action>>> q_registerB,
    std::int32_t indexA,
    std::int32_t indexB)
{
    Ptr<QueueStatusApp> firstWaySender = CreateObject<QueueStatusApp>();
    firstWaySender->Setup(addrA, addrB, nameA, nameB, q_registerA, indexB);
    nodeA->AddApplication(firstWaySender);
    firstWaySender->SetStartTime(Seconds(2.0));
    firstWaySender->SetStopTime(Seconds(10.0));

    Ptr<QueueStatusApp> secondWaySender = CreateObject<QueueStatusApp>();
    secondWaySender->Setup(addrB, addrA, nameB, nameA, q_registerB, indexA);
    nodeB->AddApplication(secondWaySender);
    secondWaySender->SetStartTime(Seconds(2.0));
    secondWaySender->SetStopTime(Seconds(10.0));
}

int
returnIndexOfNode(std::vector<std::string> nodeIds, std::string nodeName)
{
    int index = 0;
    for (const auto& name : nodeIds)
    {
        if (name == nodeName)
        {
            return index;
        }
        index++;
    }
    return -1; // nodo non trovato
}

// Simulazione del traffico a partire dalla matrice dinamica - Ipv6
void
installOnOffApplicationV6(std::vector<FlowDemand>& demands,
                          std::map<std::string, Ptr<Node>>& hostMap,
                          std::map<std::string, Ipv6Address>& hostNameToIpv6,
                          double scale)
{
    for (const auto& flow : demands)
    {
        Ptr<Node> srcNode = hostMap.at(flow.src);
        Ipv6Address dstAddr = hostNameToIpv6.at(flow.dst);

        double scaledRate = flow.rateMbps * scale;
        std::ostringstream rateStr;
        rateStr << scaledRate << "Mbps";

        int port = 9999;

        OnOffHelper onoff("ns3::UdpSocketFactory", Inet6SocketAddress(dstAddr, port));
        onoff.SetAttribute("DataRate", StringValue(rateStr.str()));
        onoff.SetAttribute("PacketSize", UintegerValue(1024));
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

        ApplicationContainer app = onoff.Install(srcNode);
        app.Start(Seconds(2.0));
        app.Stop(Seconds(10.0)); 

        // std::cout << "[TRAFFICO] " << flow.src << " -> " << flow.dst
        //          << ", indirizzo dst = " << dstAddr << ", rate = " << rateStr.str() << std::endl;
    }
}

void
createQRegisterForAllNodes(
    std::map<std::string, Ptr<Node>>& nodeMap,
    std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>>& nameToQRegister)
{
    std::vector<Dag> dags = LoadDags();

    int index = 0;
    for (const auto& [name, node] : nodeMap)
    {
        // matrice q_register per ogni singolo nodo
        auto q_register = std::make_shared<std::vector<std::vector<Action>>>();

        // per ogni DAG, prende la riga corrispondente
        for (const auto& dag : dags)
        {
            if (index < dag.adjacency_list.size())
            {
                std::vector<Action> actions;
                for (const auto& s : dag.adjacency_list[index])
                {
                    Action action;
                    action.idNodeDestination = s;
                    action.q_value = 0; // valore iniziale di q
                    actions.push_back(action);
                }
                q_register->push_back(std::move(actions));
            }
            else
            {
                std::cout << "Index out of bounds for DAG adjacency list (node " << name
                          << ", index=" << index << ")\n";
            }
        }

        // salva nella nuova mappa: nome nodo → q_register
        nameToQRegister[name] = q_register;
        index++;
    }

}

void
printQRegisters(
    const std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>>& nameToQRegister,
    const std::map<std::string, Ptr<Node>>& nodeMap)
{
    for (const auto& [name, q_register_ptr] : nameToQRegister)
    {
        std::cout << "Nodo: " << name << "\n";

        const auto& q_register = *q_register_ptr;
        Ptr<Node> node = nodeMap.at(name);

        for (size_t d = 0; d < q_register.size(); ++d)
        {
            std::cout << "  DAG " << d << ": ";
            for (const auto& action : q_register[d])
            {
                std::ostringstream outDevStr;

                if (action.outDevice)
                {
                    // Provo a ottenere l'indirizzo IPv6 associato al device
                    Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
                    for (uint32_t i = 0; i < ipv6->GetNInterfaces(); ++i)
                    {
                        for (uint32_t j = 0; j < ipv6->GetNAddresses(i); ++j)
                        {
                            if (ipv6->GetNetDevice(i) == action.outDevice)
                            {
                                Ipv6InterfaceAddress addr = ipv6->GetAddress(i, j);
                                outDevStr << addr.GetAddress();
                                break;
                            }
                        }
                    }
                }
                else
                {
                    outDevStr << "NONE";
                }

                std::cout << "[" << action.idNodeDestination << ", q=" << action.q_value
                          << ", outDevice=" << outDevStr.str() << "] ";
            }
            std::cout << "\n";
        }
        std::cout << "-------------------------------------------\n";
    }
}

void
assignOutDevices(
    const std::map<std::string, Ptr<Node>>& nodeMap,
    std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>>& nameToQRegister)
{
    // Step 1: cstruzione mappa <{nome_nodo_sorgente, nome_nodo_destinazione}, NetDevice>
    std::map<std::pair<std::string, std::string>, Ptr<NetDevice>> nodeToDestDevice;

    for (const auto& [nodeName, node] : nodeMap)
    {
        for (uint32_t i = 0; i < node->GetNDevices(); ++i)
        {
            Ptr<PointToPointNetDevice> p2pDev =
                DynamicCast<PointToPointNetDevice>(node->GetDevice(i));
            if (!p2pDev)
                continue;

            Ptr<Channel> channel = p2pDev->GetChannel();
            if (!channel)
                continue;

            for (uint32_t j = 0; j < channel->GetNDevices(); ++j)
            {
                Ptr<NetDevice> peerDev = channel->GetDevice(j);
                Ptr<Node> peerNode = peerDev->GetNode();

                if (peerNode->GetId() != node->GetId())
                {
                    // Trova il nome del peerNode
                    for (const auto& [peerName, peerNodePtr] : nodeMap)
                    {
                        if (peerNodePtr == peerNode)
                        {
                            nodeToDestDevice[{nodeName, peerName}] = p2pDev;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Step 2: assegnazione outDevice usando la mappa
    for (auto& [nodeName, q_register_ptr] : nameToQRegister)
    {
        auto& q_register = *q_register_ptr;
        for (auto& dagRow : q_register)
        {
            for (auto& action : dagRow)
            {
                auto it = nodeToDestDevice.find({nodeName, action.idNodeDestination});
                if (it != nodeToDestDevice.end())
                {
                    action.outDevice = it->second;
                }
                else
                {
                    std::cout << "WARNING: Nessun NetDevice trovato da " << nodeName << " a "
                              << action.idNodeDestination << std::endl;
                }
            }
        }
    }
}

void
installUdpSinkOnAllNodes(std::map<std::string, Ptr<Node>>& nodeMap, uint16_t port)
{
    for (auto& [name, node] : nodeMap)
    {
        // Il PacketSink ascolta su tutte le interfacce (:: è l'indirizzo "any" IPv6)
        Inet6SocketAddress local = Inet6SocketAddress(Ipv6Address::GetAny(), port);
        PacketSinkHelper sinkHelper("ns3::UdpSocketFactory", local);

        ApplicationContainer sinkApp = sinkHelper.Install(node);
        sinkApp.Start(Seconds(1.0));
        sinkApp.Stop(Seconds(10.0));
    }
}

int
main()
{
    // csv generato dai receiver
    csvFile.open("receiver_queue_log.csv");
    csvFile << "Time,ReceiverNodeID,SenderNodeID,SenderInterfaceAddress,QueueSize\n";

    Time::SetResolution(Time::NS);

    // Creazione della topologia Abilene Networl

    // Mappa degli ID nodo
    std::map<std::string, Ptr<Node>> nodeMap;
    NodeContainer allNodes;

    // memorizzo gli Ipv4 dei nodi per la simulazione del traffico
    // std::map<std::string, Ipv4Address> nodeNameToIpv4;

    // memorizzo gli Ipv6 dei nodi per la simulazione del traffico
    std::map<std::string, Ipv6Address> nodeNameToIpv6;
    std::map<Ipv6Address, std::string> ipv6ToHostName;

    // Lisa dei nomi dei nodi (dal file XML su abilene Network)
    std::vector<std::string> nodeIds = {"ATLAM5",
                                        "ATLAng",
                                        "CHINng",
                                        "DNVRng",
                                        "HSTNng",
                                        "IPLSng",
                                        "KSCYng",
                                        "LOSAng",
                                        "NYCMng",
                                        "SNVAng",
                                        "STTLng",
                                        "WASHng"};

    for (const auto& id : nodeIds)
    {
        Ptr<Node> node = CreateObject<Node>();
        nodeMap[id] = node;
        allNodes.Add(node);
    }

    std::map<std::string, std::shared_ptr<std::vector<std::vector<Action>>>> nameToQRegister;

    QRoutingHelper qRoutingHelper(&nodeMap, &nameToQRegister, nodeIds, ipv6ToHostName);

    RipNgHelper ripngRouting;
    Ipv6ListRoutingHelper listRH;
    listRH.Add(qRoutingHelper, 100);
    listRH.Add(ripngRouting, 10);

    InternetStackHelper internet;
    internet.SetRoutingHelper(listRH);
    internet.Install(allNodes);

    std::vector<Link> links;

    // link originali    
    // Aggiungi tutti i link con valori scalati
    links.push_back({"ATLAng", "ATLAM5", 9.92}); // 99.2Mbps
    links.push_back({"HSTNng", "ATLAng", 9.92});
    links.push_back({"IPLSng", "ATLAng", 2.48});
    links.push_back({"WASHng", "ATLAng", 9.92});
    links.push_back({"IPLSng", "CHINng", 9.92});
    links.push_back({"NYCMng", "CHINng", 9.92});
    links.push_back({"KSCYng", "DNVRng", 9.92});
    links.push_back({"SNVAng", "DNVRng", 9.92});
    links.push_back({"STTLng", "DNVRng", 9.92});
    links.push_back({"KSCYng", "HSTNng", 9.92});
    links.push_back({"LOSAng", "HSTNng", 9.92});
    links.push_back({"KSCYng", "IPLSng", 9.92});
    links.push_back({"SNVAng", "LOSAng", 9.92});
    links.push_back({"WASHng", "NYCMng", 9.92});
    links.push_back({"STTLng", "SNVAng", 9.92});


    /*double linkScale = 0.0125;

    // usa questi per vedere dei risultati
    links.push_back({"ATLAng", "ATLAM5", 120 * linkScale});  // ok
    links.push_back({"HSTNng", "ATLAng", 7900 * linkScale}); // ok
    links.push_back({"IPLSng", "ATLAng", 5000 * linkScale}); // ok
    links.push_back({"WASHng", "ATLAng", 8800 * linkScale}); // ok
    links.push_back({"IPLSng", "CHINng", 8500 * linkScale}); // ok
    links.push_back({"NYCMng", "CHINng", 5000 * linkScale}); // ok
    links.push_back({"KSCYng", "DNVRng", 4300 * linkScale}); // ok ma forse più basso per KSCY
    links.push_back({"SNVAng", "DNVRng", 1150 * linkScale}); // ok ma forse poco per SNVA
    links.push_back({"STTLng", "DNVRng", 2200 * linkScale}); // ok
    links.push_back({"KSCYng", "HSTNng", 2350 * linkScale}); // ok ma forse poco per KSCY
    links.push_back({"LOSAng", "HSTNng", 6100 * linkScale}); // ok ma 0 per HST
    links.push_back(
        {"KSCYng", "IPLSng", 3800 * linkScale}); // ok ma devo aumentare il carico d'invio di IPLS
    links.push_back({"SNVAng", "LOSAng", 1000 * linkScale}); // pk
    links.push_back({"WASHng", "NYCMng", 4700 * linkScale}); // ok
    links.push_back({"STTLng", "SNVAng", 500 * linkScale});  // ok
*/
    // contenitore di tutti i netDevice della rete
    NetDeviceContainer allDevices;

    uint32_t subnetCount = 0;

    std::map<std::pair<std::string, std::string>, Ipv6InterfaceContainer> linkToIfaces;

    for (const auto& link : links)
    {
        PointToPointHelper p2p;
        std::ostringstream rate;
        rate << link.capacityMbps << "Mbps";
        p2p.SetDeviceAttribute("DataRate", StringValue(rate.str()));
        p2p.SetChannelAttribute("Delay", StringValue("1ms"));

        auto devices = p2p.Install(nodeMap[link.source], nodeMap[link.target]);

        std::cout << "installazione collegamentr tra " << nodeMap[link.source]->GetId() << " e "
                  << nodeMap[link.target]->GetId() << std::endl;

        allDevices.Add(devices);

        // assegna indirizzi IPv6
        // l'obiettivo è impostare indirizzi IPv6 ai nodi
        // ed abilitare un prtocollo di dynamic routing per la
        // simulazione del traffico
        //
        Ipv6AddressHelper ipv6;
        std::ostringstream prefix;
        prefix << "fd00:" << std::hex << subnetCount << "::"; // prefisso ULA, non riservato
        ipv6.SetBase(Ipv6Address(prefix.str().c_str()), Ipv6Prefix(64));
        Ipv6InterfaceContainer ifaces = ipv6.Assign(devices);
        linkToIfaces[{link.source, link.target}] = ifaces;

        // Attivo il forwarding IPv6
        ifaces.SetForwarding(0, true);
        ifaces.SetForwarding(1, true);
        // ifaces.SetDefaultRouteInAllNodes(0);
        // ifaces.SetDefaultRouteInAllNodes(1);

        // devo salvare gli indirizzi Ipv6 per la generazione del traffico
        Ptr<Node> node1 = nodeMap[link.source];
        Ptr<Node> node2 = nodeMap[link.target];

        Ipv6Address addr1 = ifaces.GetAddress(0, 1);
        Ipv6Address addr2 = ifaces.GetAddress(1, 1);

        ipv6ToHostName[addr1] = link.source;
        ipv6ToHostName[addr2] = link.target;

        // Salva solo se non è già presente
        if (nodeNameToIpv6.find(link.source) == nodeNameToIpv6.end())
            nodeNameToIpv6[link.source] = addr1;

        if (nodeNameToIpv6.find(link.target) == nodeNameToIpv6.end())
            nodeNameToIpv6[link.target] = addr2;

        std::cout << "Subnet " << subnetCount << ": " << prefix.str() << std::endl;
        for (uint32_t i = 0; i < devices.GetN(); ++i)
        {
            auto addr = ifaces.GetAddress(i, 1);
            std::cout << "\tDevice " << i << " IPv6 = " << addr << std::endl;
        }

        // installo già qui l'app dei sender
        // adattamento dell'meccanismo di notifica dello stato della coda
        // sull'abilene network

        // Abilita PCAP per entrambi i dispositivi del link
        p2p.EnablePcap(link.source + "-" + link.target, devices.Get(0), true);
        p2p.EnablePcap(link.source + "-" + link.target, devices.Get(1), true);

        std::cout << "[SENDER INSTALLATO] " << link.source << " → " << link.target
                  << "\n\tSrc: " << ifaces.GetAddress(0, 1)
                  << "\n\tDst: " << ifaces.GetAddress(1, 1) << std::endl;

        std::cout << "[SENDER INSTALLATO] " << link.target << " → " << link.source
                  << "\n\tSrc: " << ifaces.GetAddress(1, 1)
                  << "\n\tDst: " << ifaces.GetAddress(0, 1) << std::endl;
        subnetCount++;
    }

    createQRegisterForAllNodes(nodeMap, nameToQRegister);
    assignOutDevices(nodeMap, nameToQRegister);
    printQRegisters(nameToQRegister, nodeMap);

    for (const auto& link : links)
    {
        auto key = std::make_pair(link.source, link.target);
        auto it = linkToIfaces.find(key);
        if (it == linkToIfaces.end())
        {
            std::cerr << "Errore: ifaces non trovati per " << link.source << " → " << link.target
                      << std::endl;
            continue;
        }

        const auto& ifaces = it->second;

        installBidirectionalQueueStatusSenders(ifaces.GetAddress(0, 1),
                                               ifaces.GetAddress(1, 1),
                                               nodeMap[link.source],
                                               nodeMap[link.target],
                                               link.source,
                                               link.target,
                                               nameToQRegister[link.source],
                                               nameToQRegister[link.target],
                                               returnIndexOfNode(nodeIds, link.source),
                                               returnIndexOfNode(nodeIds, link.target));
    }

    for (const auto& [name, node] : nodeMap)
    {
        Ptr<Ipv6> ipv6 = node->GetObject<Ipv6>();
        Ptr<Ipv6RoutingProtocol> proto = ipv6->GetRoutingProtocol();
        Ptr<Ipv6ListRouting> list = DynamicCast<Ipv6ListRouting>(proto);
        if (list)
        {
            for (uint32_t i = 0; i < list->GetNRoutingProtocols(); ++i)
            {
                int16_t priority;
                Ptr<Ipv6RoutingProtocol> subProto = list->GetRoutingProtocol(i, priority);
                Ptr<QRoutingProtocol> qproto = DynamicCast<QRoutingProtocol>(subProto);
                if (qproto)
                {
                    qproto->SetAddressToNameMap(ipv6ToHostName);
                    qproto->SetQRegister(nameToQRegister[name]);
                }
            }
        }
    }

    // installo i receiver per ottenere e far salvare le info sulle code
    installReceiverExchangeStateAppOnAllNodes(nodeMap, nameToQRegister);

    // set della disciplina delle code
    TrafficControlHelper tch;
    tch.Uninstall(allDevices);
    // tch.Uninstall(allHostsDevs);
    uint16_t handle =
        tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc", "MaxSize", StringValue("500000p"));
    tch.AddInternalQueues(handle, 3, "ns3::DropTailQueue", "MaxSize", StringValue("500000p"));
    QueueDiscContainer qdiscs = tch.Install(allDevices);

    // matrici di traffico per testing
    /*
        std::vector<FlowDemand> demands = {
            {"ATLAM5", "ATLAng", 100},
            {"ATLAM5", "HSTNng", 1100},
            {"ATLAng", "ATLAM5", 900},
            {"ATLAM5", "CHINng", 100},
            {"ATLAM5", "DNVRng", 10},
            {"ATLAM5", "LOSAng", 10},
            {"ATLAM5", "IPLSng", 100},
            {"ATLAM5", "NYCMng", 40},
            {"ATLAM5", "SNVAng", 32},
            {"ATLAM5", "STTLng", 70},
            {"ATLAM5", "WASHng", 30},
            //{"ATLAM5", "", 1000},

        };*/

    // installo le app onoff per generare traffico
    auto allDemands = LoadAllMatrices();
    installUdpSinkOnAllNodes(nodeMap, 9999);
    installOnOffApplicationV6(allDemands[0],
                              nodeMap,
                              nodeNameToIpv6,
                              0.248); // uso solo la prima demand


    Simulator::Stop(Seconds(10.0));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}