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
#include "ns3/queue-disc.h"
#include <vector>
#include <stdint.h>
#include <sstream>
#include <fstream>

NS_LOG_COMPONENT_DEFINE ("wifi-major");

using namespace ns3;

Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

// calculate throughput 
void
CalculateThroughput ()
{
  Time now = Simulator::Now ();

  uint32_t DropQ0 = queue0->GetTotalDroppedPackets();
  std::cout << "Total DROPPED PACKETS "<< " - Queue Node0 = " << DropQ0<< std::endl;


                                           /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx() - lastTotalRx) * (double) 8/1e5;     /* Convert Application RX Packets to MBits. */
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

using namespace ns3;

int main (int argc, char *argv[])
{
  uint32_t nWifis = 1;
  uint32_t nStas = 50;
  bool sendIp = true;
  bool writeMobility = false;
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "100Mbps";                  /* Application layer datarate. */
  std::string tcpVariant = "ns3::TcpCubic";        /* TCP variant type. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  std::string RateManager;
  double simulationTime = 4.9;                        /* Simulation time in seconds. */
  bool pcapTracing = true;                          /* PCAP Tracing is enabled or not. */

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
  
  Config::SetDefault ("ns3::WifiRemoteStationManager::m_macTxRtsFailed", MakeCallback(&m_macTxRtsFailed);


  //std::cout<<"Sthe m_macTxRtsFailed value is ============ "<<m_macTxRtsFailed<< std::endl;

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
  wifiHelper.SetRemoteStationManager ("ns3::AarfWifiManager");

  NodeContainer backboneNodes;
  NetDeviceContainer backboneDevices;
  //Ipv4InterfaceContainer backboneInterfaces;
    //std::vector<NodeContainer> staNodes;
  //std::vector<NetDeviceContainer> staDevices;
  //std::vector<Ipv4InterfaceContainer> staInterfaces;
  //NetDeviceContainer apDevice;
  //Ipv4InterfaceContainer apInterface;
  //std::vector<NetDeviceContainer> apDevices;
  //std::vector<Ipv4InterfaceContainer> apInterfaces; 

  InternetStackHelper stack;
  CsmaHelper csma;
  Ipv4AddressHelper ip;
  ip.SetBase ("192.168.0.0", "255.255.255.0");

  backboneNodes.Create (nWifis);
  stack.Install (backboneNodes);
  backboneDevices = csma.Install (backboneNodes);            // why csma only on backbone? 

  double wifiX = 0.0;

  //for (uint32_t i = 0; i < nWifis; ++i)
    //{
      //uint32_t i = 0;

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
      bridgeDev = bridge.Install (backboneNodes.Get (0), NetDeviceContainer (apDev, backboneDevices.Get (0)));

      // assign AP IP address to bridge, not wifi
      apInterface = ip.Assign (bridgeDev);

      // setup the STAs
      stack.Install (sta);
      mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                                 "Mode", StringValue ("Time"),
                                 "Time", StringValue ("2s"),
                                 "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"),
                                 "Bounds", RectangleValue (Rectangle (wifiX, wifiX+5.0,0.0, (nStas+1)*5.0)));
      mobility.Install (sta);
      wifiMac.SetType ("ns3::StaWifiMac",
                       "Ssid", SsidValue (ssid));                // why does station need SSID ?
      staDev = wifiHelper.Install (wifiPhy, wifiMac, sta);         
      staInterface = ip.Assign (staDev);                        

      // save everything in containers.
      //staNodes.push_back (sta);
      //apDevices.push_back (apDev);
      //apInterfaces.push_back (apInterface);
      //staDevices.push_back (staDev);
      //staInterfaces.push_back (staInterface);

      wifiX += 20.0;
    //}

      /* Populate routing table */
    Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


  PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny(), 9));
  ApplicationContainer sinkApp = sinkHelper.Install (backboneNodes.Get (0));
  sink = StaticCast<PacketSink> (sinkApp.Get (0));

  OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (apInterface.GetAddress (0), 9)));
  server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
  server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
  server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
  ApplicationContainer serverApp = server.Install (sta.Get(0));                   // who do you set as the server? 

  sinkApp.Start (Seconds (0.0));
  serverApp.Start (Seconds (1.0));
  Simulator::Schedule (Seconds (1.1), &CalculateThroughput);

  /*Address dest;
  std::string protocol;
  if (sendIp)
    {
      dest = InetSocketAddress (staInterfaces[1].GetAddress (1), 1025);
      protocol = "ns3::UdpSocketFactory";
    }
  else
    {
      PacketSocketAddress tmp;
      tmp.SetSingleDevice (staDevices[0].Get (0)->GetIfIndex ());
      tmp.SetPhysicalAddress (staDevices[1].Get (0)->GetAddress ());          // what is happening here
      tmp.SetProtocol (0x807);
      dest = tmp;
      protocol = "ns3::PacketSocketFactory";
    }
*/
  /*OnOffHelper onoff = OnOffHelper (protocol, dest);
  onoff.SetConstantRate (DataRate ("500kb/s"));
  ApplicationContainer apps = onoff.Install (staNodes[0].Get (0));             // what is happening here
  apps.Start (Seconds (0.5));
  apps.Stop (Seconds (3.0));*/
  
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

  Simulator::Stop (Seconds (simulationTime + 0.5));
  Simulator::Run ();
  Simulator::Destroy ();

  double averageThroughput = ((sink->GetTotalRx() * 8) / (1e6  * simulationTime));
  if (averageThroughput < 5.0)
    {
      NS_LOG_ERROR ("Obtained throughput is not in the expected boundaries!");
      exit (1);
    }
  std::cout << "\nAverage throughtput: " << averageThroughput << " Mbit/s" << std::endl;
  return 0;
}