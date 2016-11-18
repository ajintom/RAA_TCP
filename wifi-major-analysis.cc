
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

//
// Default network topology includes some number of AP nodes specified by
// the variable nWifis (defaults to two).  Off of each AP node, there are some
// number of STA nodes specified by the variable nStas (defaults to two).
// Each AP talks to its associated STA nodes.  There are bridge net devices
// on each AP node that bridge the whole thing into one network.
//
//      +-----+      +-----+            +-----+      +-----+
//      | STA |      | STA |            | STA |      | STA | 
//      +-----+      +-----+            +-----+      +-----+
//    192.168.0.2  192.168.0.3        192.168.0.5  192.168.0.6
//      --------     --------           --------     --------
//      WIFI STA     WIFI STA           WIFI STA     WIFI STA
//      --------     --------           --------     --------
//        ((*))       ((*))       |      ((*))        ((*))
//                                |
//              ((*))             |             ((*))
//             -------                         -------
//             WIFI AP   CSMA ========= CSMA   WIFI AP 
//             -------   ----           ----   -------
//             ##############           ##############
//                 BRIDGE                   BRIDGE
//             ##############           ############## 
//               192.168.0.1              192.168.0.4
//               +---------+              +---------+
//               | AP Node |              | AP Node |
//               +---------+              +---------+
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
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */
ApplicationContainer sinkApps;


// calculate throughput
void
CalculateThroughput ()
{
  Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
  //double cur = (sink->GetTotalRx() - lastTotalRx) * (double) 8/1e5;     /* Convert Application RX Packets to MBits. */
  //std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
} 

void 
MacDrop (Ptr<const Packet> p)
{
   mac_drop+=1;
}


using namespace ns3;

int main (int argc, char *argv[])
{
  uint32_t nWifis = 1;
  uint32_t nStas = 10;
  bool sendIp = true;
  bool writeMobility = false;
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "10Mbps";                   /* Application layer datarate. */
  std::string tcpVariant = "ns3::TcpNewReno";        /* TCP variant type. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  std::string RateManager = "OnoeWifiManager";
  double simulationTime = 3;                        /* Simulation time in seconds. */
  bool pcapTracing = false;                          /* PCAP Tracing is enabled or not. */
    
 //LogComponentEnable("DcaTxop", LOG_LEVEL_ALL);

  CommandLine cmd;
  cmd.AddValue ("RateManager", "RateManager", RateManager);
  cmd.AddValue ("nWifis", "Number of wifi networks", nWifis);
  cmd.AddValue ("nStas", "Number of stations per wifi network", nStas);
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
  
  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NetDeviceContainer backboneDevices;

  InternetStackHelper stack;
  CsmaHelper csma;
  
   
  NodeContainer backboneNodes = p2pNodes.Get (0);
  NodeContainer ServerNode = p2pNodes.Get (1);

  stack.Install (ServerNode);
  Ipv4AddressHelper ip;
  ip.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = ip.Assign (p2pDevices);
  ip.SetBase ("10.1.3.0", "255.255.255.0");
  backboneNodes.Create (nWifis);
  stack.Install (backboneNodes);
  backboneDevices = csma.Install (backboneNodes);  

  double wifiX = 0.0;

  
      // calculate ssid for wifi subnetwork
      std::ostringstream oss;
      oss << "wifi-default-" << 0;
      Ssid ssid = Ssid (oss.str ());

      NodeContainer sta;
      NetDeviceContainer staDev;
      NetDeviceContainer apDev;
      Ipv4InterfaceContainer staInterface;
      Ipv4InterfaceContainer apInterface;
      MobilityHelper mobility;
      BridgeHelper bridge;
      
      sta.Create (nStas);
      
      mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue (wifiX),
                                     "MinY", DoubleValue (0.0),
                                     "DeltaX", DoubleValue (5.0),
                                     "DeltaY", DoubleValue (5.0),
                                     "GridWidth", UintegerValue (1),
                                     "LayoutType", StringValue ("RowFirst"));

      // setup the AP.
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
      mobility.Install (backboneNodes.Get (0));
      wifiMac.SetType ("ns3::ApWifiMac",
                       "Ssid", SsidValue (ssid));
      apDev = wifiHelper.Install (wifiPhy, wifiMac, backboneNodes.Get (0));

      NetDeviceContainer bridgeDev;
      bridgeDev = bridge.Install (backboneNodes.Get (0), NetDeviceContainer (apDev, backboneDevices.Get (0)));     // why is this needed

      //assign ip address to ap
      apInterface = ip.Assign (bridgeDev);

      // setup the STAs
      stack.Install (sta);
      
      mobility.SetPositionAllocator ("ns3::RandomDiscPositionAllocator", // allocates random position within a disc
                                     "X", DoubleValue (0.0), 
                                     "Y", DoubleValue (0.0),
                                     "Rho", StringValue ("ns3::UniformRandomVariable[Min=0|Max=10]")); //decide radius here
      mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel"); //nodes wont move
                                 
      mobility.Install (sta);
      wifiMac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid));                
      staDev = wifiHelper.Install (wifiPhy, wifiMac, sta); 

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
      
                            
      staInterface = ip.Assign (staDev);   
                 
      wifiX += 20.0;
    
      /* Populate routing table */
      Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Install Downlink Traffic */
  for(uint32_t i = 0; i< nStas; i++)
  {
    BulkSendHelper source ("ns3::TcpSocketFactory",
                          InetSocketAddress(staInterface.GetAddress (i), 5555));
    source.SetAttribute ("MaxBytes", UintegerValue (0));
    ApplicationContainer sourceApps = source.Install (p2pNodes.Get(1));
    sourceApps.Start(Seconds (0.0));
    sourceApps.Stop(Seconds (10.0));
  }
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                        InetSocketAddress (apInterface.GetAddress (0), 5555));
  sinkApps = sink.Install (sta);
  sinkApps.Start (Seconds (0.0));
  sinkApps.Stop (Seconds (10.0));


  /* upload traffic */
  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), 8080));  
  ApplicationContainer sinkApp = sinkHelper.Install (backboneNodes.Get (0));
  apSink = StaticCast<PacketSink> (sinkApp.Get (0));
  

  OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), 8080)));
  server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  
    for (uint32_t i = 0; i < nStas ; ++i)
  {
    ApplicationContainer serverApp = server.Install (sta.Get(i));
    serverApp.Start (Seconds (1.0)); 
  }
 
  sinkApp.Start (Seconds (0.0));
  //Simulator::Schedule (Seconds (1.1), &CalculateThroughput);
     
  if (pcapTracing)
    {  
      wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);
      wifiPhy.EnablePcap ("wifi-major", apDev);
      wifiPhy.EnablePcap ("Station", staDev);
    }  

  
  if (writeMobility)
    {
      AsciiTraceHelper ascii;
      MobilityHelper::EnableAsciiAll (ascii.CreateFileStream ("wifi-major.mob"));
    }

  Config::ConnectWithoutContext("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/MacTxDrop", MakeCallback(&MacDrop));

  //flowmonitor 
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 0.5));
  AnimationInterface anim ("animation.xml");
  Simulator::Run ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  
  double Avg_tp = 0;
  double Avg_delay = 0;
  double tp[nStas];
  double mean_delay[nStas];
  uint32_t Avg_lostPackets = 0;
  uint32_t mean_lostPackets[nStas];

  
    for (uint32_t i = 1; i <= nStas; ++i)
  { 
    //stats[i].timeLastRxPacket.GetSeconds () - stats[i].timeFirstRxPacket.GetSeconds ()
    tp[i] = (stats[i].txBytes * 8.0) / (simulationTime) / 1000000; 
    mean_delay[i] = stats[i].delaySum.GetSeconds () / stats[i].rxPackets; 
    mean_lostPackets[i] =  stats[i].lostPackets;
  }  
    
    uint32_t inf_flag = 1;
    for (uint32_t i = 1; i <= nStas; ++i)
  {
      if (std::isinf(tp[i]))
    { 
      if(inf_flag == 1)
      Avg_tp += 0;
      else 
      Avg_tp += tp[inf_flag]; 
    } 
    else 
    { 
      inf_flag = i;
      Avg_tp += tp[i];  
    }  
    Avg_delay += mean_delay[i];
    Avg_lostPackets+=mean_lostPackets[i];
  }  
  
  Avg_tp =  Avg_tp/nStas;
  Avg_delay = Avg_delay/nStas;
  Avg_lostPackets = Avg_lostPackets/nStas;
  //std::cout << "  Throughput: " << Avg_tp << " Mbps" << std::endl;
  //std::cout << "  Mean delay:   " << Avg_delay << "s" << std::endl;
  //std::cout << "  Packets lost: "<< Avg_lostPackets << std::endl; // value too big, something is wrong

  Simulator::Destroy ();
  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  double thr = 0;
  uint32_t totalPacketsThr = DynamicCast<PacketSink> (sinkApp.Get (0))->GetTotalRx ();
  thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
  std::cout << "  Average Goodput: " << thr << " Mbit/s" << std::endl;

  uint32_t coll = 0;                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                              
  for (uint32_t i = 0; i < nStas; ++i)                                                                                                                                                                                                                                        
  {
    coll = coll + dca[i]->m_collision;
  }
  //std::cout << "Collissions:   " << coll << std::endl;
  //std::cout << "Mac drop:" << mac_drop << std::endl;  

                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
  std::ofstream myfile;
  myfile.open ("values_"+RateManager+".csv",std::ios_base::app);
  myfile << std::endl;
  myfile << Avg_tp << ",";
  myfile << Avg_delay << ",";
  myfile << thr << ",";
  myfile << coll << ",";
  myfile.close();
  
    
  double averageThroughput = ((apSink->GetTotalRx() * 8) / (1e6  * simulationTime));
  std::cout << "\nAverage throughtput: " << averageThroughput << " Mbit/s" << std::endl;
  return 0;
}
