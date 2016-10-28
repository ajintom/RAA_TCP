
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
#include <vector>
#include <stdint.h>
#include <sstream>
#include <fstream>

NS_LOG_COMPONENT_DEFINE ("wifi-major-dcatxop");
//LogComponentEnableAll(wifi-major-dcatxop); 

using namespace ns3;

Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */           
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

void
TcPacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void
DevicePacketsInQueueTrace (uint32_t oldValue, uint32_t newValue)
{
  std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
}


// calculate throughput 
void
CalculateThroughput ()
{
  Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx() - lastTotalRx) * (double) 8/1e5;     /* Convert Application RX Packets to MBits. */
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}


using namespace ns3;

int main (int argc, char *argv[])
{
  uint32_t nWifis = 1;
  uint32_t nStas = 15 ;
  bool sendIp = true;
  bool writeMobility = false;
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "100Mbps";                  /* Application layer datarate. */
  std::string tcpVariant = "ns3::TcpCubic";        /* TCP variant type. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  std::string RateManager;
  double simulationTime = 3;                        /* Simulation time in seconds. */
  bool pcapTracing = false;                          /* PCAP Tracing is enabled or not. */

 //LogComponentEnable("DcaTxop", LOG_LEVEL_ALL);

  CommandLine cmd;
  //cmd.AddValue ("RateManager", "RateManager", RateManager);
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
  wifiPhy.Set ("TxGain", DoubleValue (0));                      // change? 
  wifiPhy.Set ("RxGain", DoubleValue (0));                      // change?
  wifiPhy.Set ("RxNoiseFigure", DoubleValue (10));              // change?
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue (-79));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue (-79 + 3));
  wifiPhy.SetErrorRateModel ("ns3::YansErrorRateModel");
  //wifiHelper.SetRemoteStationManager ("ns3::AparfWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::AmrrWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::AarfWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::AarfcdWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::CaraWifiManager");
  wifiHelper.SetRemoteStationManager ("ns3::IdealWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::OnoeWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::MinstrelHtWifiManager");
  //wifiHelper.SetRemoteStationManager ("ns3::ConstantRateWifiManager");
  
  
  
  

  NodeContainer backboneNodes;
  NetDeviceContainer backboneDevices;

  InternetStackHelper stack;
  CsmaHelper csma;
  Ipv4AddressHelper ip;
  ip.SetBase ("192.168.0.0", "255.255.255.0");

  backboneNodes.Create (nWifis);
  stack.Install (backboneNodes);
  backboneDevices = csma.Install (backboneNodes);            // why csma only on backbone? 

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

      // assign AP IP address to bridge, not wifi
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
                       "Ssid", SsidValue (ssid));                // why does station need SSID ?
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


  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), 9));  // mismatch between this and the next line? 
  ApplicationContainer sinkApp = sinkHelper.Install (backboneNodes.Get (0));
  sink = StaticCast<PacketSink> (sinkApp.Get (0));

  OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), 9)));
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
  Simulator::Schedule (Seconds (1.1), &CalculateThroughput);

      
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

  //flowmonitor 
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  Simulator::Stop (Seconds (simulationTime + 0.5));
  AnimationInterface anim ("animation.xml");
  Simulator::Run ();

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
  std::cout << "  Tx Packets:   " << stats[1].txPackets << std::endl;
  std::cout << "  Tx Bytes:   " << stats[1].txBytes << std::endl;
  std::cout << "  Offered Load: " << stats[1].txBytes * 8.0 / (stats[1].timeLastTxPacket.GetSeconds () - stats[1].timeFirstTxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Rx Packets:   " << stats[1].rxPackets << std::endl;
  std::cout << "  Rx Bytes:   " << stats[1].rxBytes << std::endl;
  std::cout << "  Throughput: " << stats[1].rxBytes * 8.0 / (stats[1].timeLastRxPacket.GetSeconds () - stats[1].timeFirstRxPacket.GetSeconds ()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Mean delay:   " << stats[1].delaySum.GetSeconds () / stats[1].rxPackets << std::endl;
  std::cout << "  Mean jitter:   " << stats[1].jitterSum.GetSeconds () / (stats[1].rxPackets - 1) << std::endl;


  Simulator::Destroy ();

  std::cout << std::endl << "*** Application statistics ***" << std::endl;
  double thr = 0;
  uint32_t totalPacketsThr = DynamicCast<PacketSink> (sinkApp.Get (0))->GetTotalRx ();
  thr = totalPacketsThr * 8 / (simulationTime * 1000000.0); //Mbit/s
  std::cout << "  Rx Bytes: " << totalPacketsThr << std::endl;
  std::cout << "  Average Goodput: " << thr << " Mbit/s" << std::endl;

  uint32_t coll = 0;
  for (uint32_t i = 0; i < nStas; ++i)
  {
     coll = coll + dca[i]->m_collision;
  }
  std::cout << "Collissions:   " << coll << std::endl;
  std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
    
  double averageThroughput = ((sink->GetTotalRx() * 8) / (1e6  * simulationTime));
  if (averageThroughput < 5.0)
    {
      NS_LOG_ERROR ("Obtained throughput is not in the expected boundaries!");
      exit (1);
    }
  std::cout << "\nAverage throughtput: " << averageThroughput << " Mbit/s" << std::endl;
  return 0;
}
