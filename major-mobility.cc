//mobility
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Topology involves an AP with less than 5 stations. nStas = number of stations
// Simulating mobility scenario in which node(s) move with a constant velocity for "t_move" seconds   
// and remains stationery for "t_sta" seconds, loop.
//      +-----+      +-----+                
//      WIFI STA     WIFI STA           
//      --------     --------           
//        ((*))       ((*))           
//                               
//              ((*))                         
//             -------        
//             WIFI AP   CSMA 
//             -------   ----         
//   
//               
//               +---------+             
//               | AP Node |             
//               +---------+             
//

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/bridge-helper.h"
#include "ns3/ideal-wifi-manager.h"
#include "ns3/aarf-wifi-manager.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/queue-disc.h"
#include "ns3/object-map.h"
#include "ns3/object.h" 
#include "ns3/wifi-mac-queue.h"
#include "ns3/dca-txop.h" 
#include "ns3/wifi-mac-header.h"   
#include "ns3/log.h"   
#include <vector>
#include <stdint.h>
#include <iostream>
#include <sstream>
#include <fstream>

NS_LOG_COMPONENT_DEFINE ("wifi-major-dcatxop");
//LogComponentEnableAll(wifi-major-dcatxop); 

using namespace ns3;
uint32_t mac_drop = 0;
Ptr<PacketSink> apSink, staSink;                         /* Pointer to the packet sink application */  
uint64_t lastTotalRxAp = 0;  
const uint32_t nStas = 3;
uint64_t lastTotalRxSta[nStas] = {};
ApplicationContainer sinkApps;

uint32_t t_move = 5;
uint32_t t_sta = 10;

void initialize ()
{
  for (uint32_t i = 0; i < nStas; ++i)
  {
    lastTotalRxSta[i] = 0;
  }
}

/*calculate throughput*/
void
CalculateThroughput ()
{
  Time now = Simulator::Now ();

  /* Calculate throughput for downlink */
  double sumRx = 0, avgRx = 0; 
  for (uint32_t i = 0; i < nStas; ++i)
    {
      staSink = StaticCast<PacketSink>(sinkApps.Get(i));
      double curRxSta = (staSink->GetTotalRx() - lastTotalRxSta[i]) * (double) 8/1e5; 
      lastTotalRxSta[i] = staSink->GetTotalRx ();
      sumRx += curRxSta;
    } 
  avgRx = sumRx/nStas;  
  

  /* Calculate throughput for uplink */
  double curRxAp = (apSink->GetTotalRx() - lastTotalRxAp) * (double) 8/1e5;    
  lastTotalRxAp = apSink->GetTotalRx (); 
  curRxAp = curRxAp/nStas;                             
  

  /* Log to CSV */ 
  std::cout << now.GetSeconds () << "s: \t" << avgRx << " " << curRxAp << " Mbit/s" << std::endl;
  std::ofstream myfile;
  myfile.open ("OnoeWifiManager.csv",std::ios_base::app);
  myfile << std::endl;
  myfile << avgRx << ",";
  myfile << curRxAp << ",";
  myfile.close();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
} 

/* Mac Drop */
void 
MacDrop (Ptr<const Packet> p)
{
   mac_drop+=1;
}

/*
void PrintPositions ()
{
    for (uint32_t i=0; i <NodeList::GetNNodes(); i++ )
    {
      Ptr<MobilityModel> mob = nodes.Get(i)->GetObject<MobilityModel>();
      Vector pos = mob->GetPosition ();
      std::cout << "POS: x=" << pos.x << ", y=" << pos.y << std::endl;
    }
//Simulator::Schedule(Seconds(1), MakeCallback(&PrintPositions));
}
*/

using namespace ns3;

int main (int argc, char *argv[])
{
  uint32_t nWifis = 1;
  bool sendIp = true;
  bool writeMobility = false;
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "10Kbps";                   /* Application layer datarate. */
  std::string tcpVariant = "ns3::TcpNewReno";        /* TCP variant type. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  std::string RateManager = "OnoeWifiManager";
  double simulationTime = 100;                        /* Simulation time in seconds. */
  bool pcapTracing = false;                          /* PCAP Tracing is enabled or not. */
    
 //LogComponentEnable("DcaTxop", LOG_LEVEL_ALL);

  CommandLine cmd;
  cmd.AddValue ("RateManager", "RateManager", RateManager);
  cmd.AddValue ("nWifis", "Number of wifi networks", nWifis);
  //cmd.AddValue ("nStas", "Number of stations per wifi network", nStas);
  cmd.AddValue ("SendIp", "Send Ipv4 or raw packets", sendIp);
  cmd.AddValue ("writeMobility", "Write mobility trace", writeMobility);
  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Application data ate", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpTahoe, TcpReno, TcpNewReno, TcpWestwood, TcpWestwoodPlus ", tcpVariant);
  cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("pcap", "Enable/disable PCAP Tracing", pcapTracing);
  cmd.Parse (argc, argv);

  std::string RateAdaptationManager = "ns3::"; 
  RateAdaptationManager += RateManager;

  /* No fragmentation and no RTS/CTS */
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("999999"));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("999999"));

  /* Configure TCP Options */
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));
 
  WifiMacHelper wifiMac;
  WifiHelper wifiHelper;
  wifiHelper.SetStandard (WIFI_PHY_STANDARD_80211g);

  /* Set up Legacy Channel */
  YansWifiChannelHelper wifiChannel ;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel", "Frequency", DoubleValue (5e9));

  /* Setup Physical Layer */
  YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());
  wifiPhy.Set ("TxPowerStart", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerEnd", DoubleValue (10.0));
  wifiPhy.Set ("TxPowerLevels", UintegerValue (1));
  wifiPhy.Set ("TxGain", DoubleValue (10));                      
  wifiPhy.Set ("RxGain", DoubleValue (10));                      
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (-10));              
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-79));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-79 + 3));
  //wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  if (RateManager == "ConstantRateWifiManager")
  {
      wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                              "DataMode", StringValue ("DsssRate11Mbps"), 
                              "ControlMode", StringValue ("HtMcs0"));
  }
  else
  {   
    wifiHelper.SetRemoteStationManager (RateAdaptationManager);   
  }
  //wifiHelper.SetRemoteStationManager ("ns3::AparfWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::AmrrWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::AarfWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::AarfcdWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::CaraWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::IdealWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::OnoeWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
  
  /* Configure p2p */
  NodeContainer p2pNodes;
  p2pNodes.Create (2);
  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));
  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  /* Make the connection */
  NodeContainer sta;
  sta.Create (nStas);   
  NodeContainer backboneNodes = p2pNodes.Get (0);
  NodeContainer ServerNode = p2pNodes.Get (1);

  /* Install CSMA on AP */
  CsmaHelper csma;
  NetDeviceContainer backboneDevices;
  backboneDevices = csma.Install (backboneNodes);  
  
  /* Calculate ssid for wifi subnetwork */
  std::ostringstream oss;
  oss << "wifi-default-" << 0;
  Ssid ssid = Ssid (oss.str ());
  
  MobilityHelper mobility;
  MobilityHelper mobility1;
  /* Mobility of AP */         
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (5.0),
                                 "GridWidth", UintegerValue (1),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (backboneNodes);
  
  /* WiFi Mac of AP */
  wifiMac.SetType ("ns3::ApWifiMac",
                   "Ssid", SsidValue (ssid));
  NetDeviceContainer apDev;
  apDev = wifiHelper.Install (wifiPhy, wifiMac, backboneNodes);

  /* Mobility of nodes */    
  mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator", // allocates random position within a disc
                                 "X", DoubleValue (0.0), 
                                 "Y", DoubleValue (0.0),
                                 "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=10]")); //decide radius here
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel"); //nodes wont move

  mobility1.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                              "Bounds", RectangleValue (Rectangle (0, 50, 0, 50)),
                              "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=20]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"));

  //mobility.Install(sta);
  mobility1.Install (sta.Get(0));
  mobility.Install (sta.Get(1));
  mobility.Install (sta.Get(2));
  mobility.Install (ServerNode);

//moving one node using mobility

/*

//moving node(1)
  MobilityHelper movement;
  Ptr<ListPositionAllocator> stationPositionAlloc = CreateObject<ListPositionAllocator>();
  stationPositionAlloc -> Add(Vector(0.0, 0.0, 0.0)); // position1
 // stationPositionAlloc -> Add(Vector(1000.0, 0.0, 0.0)); // position2
  movement.SetPositionAllocator(stationPositionAlloc);
  movement.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
  movement.Install(sta.Get(1));//movement only on one node 
  (sta.Get(1) -> GetObject<ConstantVelocityMobilityModel>()) -> SetVelocity(Vector(10.0, 0.0, 0.0));

 Here a timer should run, need to figure out how to do that
    then move the node in reverse direction [p]


*/

//movement.Install(sta);//should we have separate NodeContainers for fixed and moving nodes?





  
  /* WiFi Mac of nodes */
  wifiMac.SetType ("ns3::StaWifiMac",
                   "Ssid", SsidValue (ssid));  
  NetDeviceContainer staDev;                               
  staDev = wifiHelper.Install (wifiPhy, wifiMac, sta); 
 
  /* Internet Stack */
  InternetStackHelper stack;
  stack.Install (ServerNode);
  stack.Install (backboneNodes);
  stack.Install (sta);
  
  /* Assign ip to p2p */
  Ipv4AddressHelper ip;
  ip.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = ip.Assign (p2pDevices);
  
  /* Assign ip to AP */
  ip.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface;
  apInterface = ip.Assign (apDev);
  
  /* Track Collisions */
  Ptr<DcaTxop> dca[nStas];

    for (uint32_t i = 0; i < nStas; ++i)
     {
       Ptr<WifiNetDevice> dev = DynamicCast<WifiNetDevice> (staDev.Get(i));
       NS_ASSERT (dev != NULL);
       PointerValue ptr;
       dev->GetAttribute ("Mac",ptr);
       Ptr<StaWifiMac> mac = ptr.Get<StaWifiMac> ();
       NS_ASSERT (mac != NULL);
       mac->GetAttribute ("DcaTxop",ptr);
       dca[i] = ptr.Get<DcaTxop> ();
       NS_ASSERT (dca[i] != NULL);
     }  
  
  /* Assign ip to Stations */    
  Ipv4InterfaceContainer staInterface;                           
  staInterface = ip.Assign (staDev);   
                 
    /* Install Downlink Traffic */
  for(uint32_t i = 0; i< nStas; i++)
  {
    BulkSendHelper source ("ns3::TcpSocketFactory",
                          InetSocketAddress(staInterface.GetAddress (i), 5555));
    source.SetAttribute ("MaxBytes", UintegerValue (100e6));
    ApplicationContainer sourceApps = source.Install (p2pNodes.Get(1));
    sourceApps.Start(Seconds (0.0));
    sourceApps.Stop(Seconds (simulationTime));
  }
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                        InetSocketAddress (Ipv4Address::GetAny (), 5555));
  sinkApps = sink.Install (sta);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (simulationTime));
  

  /* upload traffic */ 
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), 8080));  
  ApplicationContainer sinkApp = sinkHelper.Install (backboneNodes.Get (0));
  apSink = StaticCast<PacketSink> (sinkApp.Get (0));
  

  OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (p2pInterfaces.GetAddress (0), 8080)));
  server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  
    for (uint32_t i = 0; i < nStas ; ++i)
  {
    ApplicationContainer serverApp = server.Install (sta.Get(i));
    serverApp.Start (Seconds (1.0)); 
  }
  
  /* Start AP sink app */ 
  sinkApp.Start (Seconds (0.0));

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  Simulator::Schedule (Seconds (1.1), &CalculateThroughput);

  /* PcaP Tracing */   
  if (pcapTracing)
    {  
      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("wifi-major", apDev);
      wifiPhy.EnablePcap ("Station", staDev);
    }  

  /* Mobility Control */
  if (writeMobility)
    {
      AsciiTraceHelper ascii;
      MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("wifi-major.mob"));
    }
  
  /* Callback on MacDrop */
  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&MacDrop));

  /* Install flowmonitor */ 
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 0.5));
  AnimationInterface anim ("animation.xml");
  Simulator::Run ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  
  uint32_t Avg_lostPackets = 0;
  uint32_t mean_lostPackets[nStas];
  
    for (uint32_t i = 1; i <= nStas; ++i)
  { 
    mean_lostPackets[i] =  stats[i].lostPackets;
    Avg_lostPackets+=mean_lostPackets[i];
  }  

  Avg_lostPackets = Avg_lostPackets/nStas;
  std::cout << "  Packets lost: "<< Avg_lostPackets << std::endl; // value too big, something is wrong

  Simulator::Destroy ();
  
  /* Calculate Collisions */ 
  uint32_t coll = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              
  for (uint32_t i = 0; i < nStas; ++i)                                                                                                                                                                                                                                        
  {
    coll = coll + dca[i]->m_collision;
  }
  std::cout << "Collissions:   " << coll << std::endl;
  std::cout << "Mac drop:" << mac_drop << std::endl;  

 
  return 0;
}
