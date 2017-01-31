#include "ns3/core-module.h"
#include "ns3/propagation-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/config-store-module.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <fstream>
#include <vector>
NS_LOG_COMPONENT_DEFINE ("Main");

using namespace ns3;

#define Downlink true
#define Uplink false
#define PI 3.14159265
#define PI_e5 314158
#define simulationTime 5

void CalculateThroughput();
ApplicationContainer sinkApps;
ApplicationContainer apSinkApps;
size_t m_apNumber;
size_t m_nodeNumber;
Ptr<PacketSink> staSink,apSink;
double lastTotalRxSta[4] = {0} , lastTotalRxAp[2] = {0};

class Experiment
{
public:
  Experiment(bool downlinkUplink, std::string in_modes);
  void SpectrumSignalArrival (std::string context, bool signalType, uint32_t senderNodeId, double rxPower, Time duration);
  void SetRtsCts(bool enableCtsRts);
  void CreateNode(size_t in_ap, size_t in_nodeNumber, double radius);
  void CreateNode(size_t in_ap, size_t in_nodeNumber);
  void InitialExperiment();
  void InstallApplication(size_t in_packetSize, size_t in_dataRate);
  void Run(size_t in_simTime);
  void PhyRxErrorTrace (std::string context, Ptr<const Packet> packet, double snr);   
  void PhyRxOkTrace (std::string context, Ptr<const Packet> packet, 
                     double snr, WifiMode mode, enum WifiPreamble preamble);
  void PhyTxTrace (std::string context, Ptr<const Packet> packet, WifiMode mode, 
              WifiPreamble preamble, uint8_t txPower);
  void ShowNodeInformation(NodeContainer in_c, size_t in_numOfNode);

private:  
  void SetWifiChannel();
  void InstallDevices();  
  void InstallIp();
  
  bool m_enableCtsRts;
  bool m_downlinkUplink;
  bool pcapTracing = false;
  double m_radius;
  size_t m_rxOkCount;
  size_t m_rxErrorCount;
  size_t m_txOkCount;
  size_t isNotWifi; 
  size_t sigPower;
  std::string m_modes;
  std::vector<std::vector<double> > m_readChannelGain;
  std::vector<int> m_serveBy;
  NodeContainer m_nodes;
  std::string dataRate = "10Kbps"; 
  std::string tcpVariant = "ns3::TcpNewReno";
  MobilityHelper m_mobility;
  Ptr<ListPositionAllocator> m_apPosAlloc;
  Ptr<ListPositionAllocator> m_nodePosAlloc;
  WifiHelper m_wifi;
  SpectrumWifiPhyHelper spectrumPhy = SpectrumWifiPhyHelper::Default ();
  NqosWifiMacHelper m_wifiMac;
  NetDeviceContainer m_devices;
  InternetStackHelper m_internet;
  Ipv4AddressHelper m_ipv4;
  ApplicationContainer m_cbrApps;
  ApplicationContainer m_upApps;
  ApplicationContainer m_pingDownApps;
  ApplicationContainer m_pingUpApps;
};

Experiment::Experiment(bool in_downlinkUplink, std::string in_modes):
  m_downlinkUplink(in_downlinkUplink), m_modes(in_modes)
{
  m_rxOkCount = 0;
  m_rxErrorCount = 0;
  m_txOkCount = 0;
  isNotWifi = 0;
  sigPower = 0;

}

void CalculateThroughput()
{
  Time now = Simulator::Now ();

  double sumRx = 0, avgRx = 0, tempSinkSta = 0;
  double sumAp = 0, avgAp = 0, tempSinkAp = 0;
  /* Calculate throughput for uplink */
  
  for (size_t k = 0; k < m_apNumber; ++k)
    {
      apSink = StaticCast<PacketSink>(apSinkApps.Get(k));
      tempSinkAp = apSink->GetTotalRx();
      //std::cout << " lastAp " << lastTotalRxAp[k] << "\n";
      //std::cout << " sinkAp " << tempSinkAp << "\n";
      double curRxAp = (tempSinkAp - lastTotalRxAp[k]) * (double) 8/1e5;    
      lastTotalRxAp[k] = tempSinkAp; 
      sumAp += curRxAp;  
      //std::cout << " curRxAp " << curRxAp << "\n";
      //std::cout << " SumAp " << sumAp << "\n";                 
    }

  /* Calculate throughput for downlink */
  for (size_t m = 0; m < m_nodeNumber; ++m)
    {
      staSink = StaticCast<PacketSink>(sinkApps.Get(m));
      tempSinkSta = staSink->GetTotalRx();
      //std::cout << " lastSta " << lastTotalRxSta[m] << "\n";
      //std::cout << " sinkSta " << tempSinkSta << "\n";
      double curRxSta = (tempSinkSta - lastTotalRxSta[m]) * (double) 8/1e5; 
      lastTotalRxSta[m] = tempSinkSta;
      sumRx += curRxSta;
      //std::cout << " curRxSta " << curRxSta << "\n";
      //std::cout << " SumRx " << sumRx << "\n";
    } 
  avgRx = sumRx/m_nodeNumber;
  avgAp = sumAp/m_nodeNumber;

  /* Log to CSV */ 
  std::cout << now.GetSeconds () << "s: \t" << avgRx << "   " << avgAp << " Mbit/s" << std::endl;
  /*std::ofstream myfile;
  myfile.open ("AarfWifiManager-case4.csv",std::ios_base::app);
  myfile << std::endl;
  myfile << avgRx << ",";
  myfile << avgAp << ",";
  myfile.close();*/
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
} 

void
Experiment::SpectrumSignalArrival (std::string context, bool signalType,
uint32_t senderNodeId, double rxPower, Time duration)
{
  if(signalType == false){
    isNotWifi++;
  }

  if(rxPower < -62){
    sigPower++;
  }
}

void
Experiment::InitialExperiment()
{
  SetWifiChannel();
  InstallDevices();  
  InstallIp();
}

void
Experiment::SetRtsCts(bool in_enableCtsRts)
{
  m_enableCtsRts = in_enableCtsRts;
  UintegerValue ctsThr = (m_enableCtsRts ? UintegerValue (10) : UintegerValue (22000));
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", ctsThr);
}


void Experiment::CreateNode(size_t in_ap, size_t in_nodeNumber, double in_radius)
{  
  m_apNumber = in_ap;
  m_nodeNumber = in_nodeNumber;
  m_radius = in_radius; 
  
  m_nodes.Create(m_apNumber+m_nodeNumber);
  /*for(size_t i=0; i<m_nodeNumber; ++i){
    sta_nodes[i]=m_nodes.Get(m_apNumber + i);
  }*/
  
  m_mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  m_mobility.SetMobilityModel ("ns3::ConstantVelocityMobilityModel");
  m_apPosAlloc = CreateObject<ListPositionAllocator> ();
  m_nodePosAlloc = CreateObject<ListPositionAllocator> ();
  
  for(size_t i=0; i<m_apNumber; ++i){

    //m_apPosAlloc->Add(Vector((160*i), 0, 1)); //A1,A2 - Case 1 
   //m_apPosAlloc->Add(Vector((500*i), 0, 1)); //A1,A2 - Case 2 
   //m_apPosAlloc->Add(Vector((60*i)+90, 0, 1)); //A1,A2 - Case 3 
   m_apPosAlloc->Add(Vector((260*i)+90, 0, 1)); //A1,A2 - Case 4 
  }
  m_mobility.SetPositionAllocator(m_apPosAlloc);
  for(size_t i=0; i<m_apNumber; ++i){
    m_mobility.Install(m_nodes.Get(i));  
  }

  for(size_t i=0; i<m_nodeNumber; ++i){

  //m_nodePosAlloc->Add(Vector(80, 0, 1)); //C1,C2 - Case 1
  //m_nodePosAlloc->Add(Vector((500*i), 80, 1)); //C1,C2 - Case 2
  //m_nodePosAlloc->Add(Vector(( (20*i*i*i)-(105*i*i)+(195*i)  ), 0, 1)); //C1,C2,C3,C4 - Case 3
  m_nodePosAlloc->Add(Vector(( (-46.67*i*i*i)+(195*i*i)-(38.33*i)  ), 0, 1)); //C1,C2,C3,C4 - Case 4
  
  }
  m_mobility.SetPositionAllocator(m_nodePosAlloc);
  for(size_t i=0; i<m_nodeNumber; ++i){
    m_mobility.Install(m_nodes.Get(m_apNumber+i));  
  }   
}

void
Experiment::SetWifiChannel()
{
  Ptr<SingleModelSpectrumChannel> spectrumChannel
     = CreateObject<SingleModelSpectrumChannel> ();
  Ptr<TwoRayGroundPropagationLossModel> lossModel
     = CreateObject<TwoRayGroundPropagationLossModel> ();
  spectrumChannel->AddPropagationLossModel (lossModel);
  Ptr<ConstantSpeedPropagationDelayModel> delayModel
     = CreateObject<ConstantSpeedPropagationDelayModel> ();
  spectrumChannel->SetPropagationDelayModel (delayModel); 
  spectrumPhy.SetChannel (spectrumChannel);
  //spectrumPhy.SetErrorRateModel ("ns3::NistErrorRateModel"); 
}

void
Experiment::InstallDevices()
{
  m_wifi.SetStandard (WIFI_PHY_STANDARD_80211g);  //doesnt work with 80211b 
  // same as b but higher rate (54mbps) 

  /*
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                              StringValue ("DsssRate2Mbps"));*/
  
  /*m_wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager", 
                              "DataMode", StringValue (m_modes), 
                              "ControlMode", StringValue (m_modes));*/
  m_wifi.SetRemoteStationManager ("ns3::AarfWifiManager");
                                
                                
  Config::SetDefault ("ns3::WifiPhy::CcaMode1Threshold", DoubleValue (-95.0));
  spectrumPhy.Set ("EnergyDetectionThreshold", DoubleValue (-95.0) );
  spectrumPhy.Set ("Frequency", UintegerValue(2400)); 
  spectrumPhy.Set ("TxPowerStart", DoubleValue (23.0));
  spectrumPhy.Set ("TxPowerEnd", DoubleValue (23.0));
  spectrumPhy.Set ("ChannelNumber", UintegerValue (1) );
  spectrumPhy.Set ("RxGain", DoubleValue (-25)); 

  m_wifiMac.SetType ("ns3::AdhocWifiMac"); // use ad-hoc MAC
  m_devices = m_wifi.Install (spectrumPhy, m_wifiMac, m_nodes); 
}

void
Experiment::InstallIp()
{  
  m_internet.Install (m_nodes);  
  m_ipv4.SetBase ("10.0.0.0", "255.0.0.0");
  m_ipv4.Assign (m_devices); 
}

void
Experiment::PhyRxErrorTrace (std::string context, Ptr<const Packet> packet, double snr)
{
  Ptr<Packet> m_currentPacket;
  WifiMacHeader hdr;
  m_currentPacket = packet->Copy();
  m_currentPacket->RemoveHeader (hdr);
  if(hdr.IsData()){
    m_rxErrorCount++;
  }
}

void
Experiment::PhyRxOkTrace (std::string context, Ptr<const Packet> packet, 
        double snr, WifiMode mode, enum WifiPreamble preamble)
{
  Ptr<Packet> m_currentPacket;
  WifiMacHeader hdr;
  
  m_currentPacket = packet->Copy();
  m_currentPacket->RemoveHeader (hdr);  
  if(hdr.IsData()){    
    m_rxOkCount++;
  }
}

void
Experiment::PhyTxTrace (std::string context, Ptr<const Packet> packet, 
                  WifiMode mode, WifiPreamble preamble, uint8_t txPower)
{
  Ptr<Packet> m_currentPacket;
  WifiMacHeader hdr;
  m_currentPacket = packet->Copy();
  m_currentPacket->RemoveHeader (hdr);
  if(hdr.IsData()){
    m_txOkCount++;
  }
}



void
Experiment::InstallApplication(size_t in_packetSize, size_t in_dataRate)
{
  uint16_t downPort = 12345;
  uint16_t upPort = 5555;
  uint16_t  echoDownPort = 9;
  uint16_t  echoUpPort = 8080;

  for(size_t j=1; j<=m_apNumber; ++j){
    for(size_t i=m_apNumber+(m_nodeNumber/m_apNumber)*(j-1); 
        i<m_apNumber+(m_nodeNumber/m_apNumber)*j ; ++i){
      std::string s1;
      std::string s2;
      std::stringstream ss1(s1);
      std::stringstream ss2(s2);
      if(m_downlinkUplink){

         ss1 << i+1;
         ss2 << j;
      }

      s1 = "10.0.0."+ss1.str();
      s2 = "10.0.0."+ss2.str();
      BulkSendHelper bulk ("ns3::TcpSocketFactory", 
             InetSocketAddress (Ipv4Address (s1.c_str()), downPort));
      
      
      /* Installing Sink applications */   
      PacketSinkHelper sink ("ns3::TcpSocketFactory",
                        InetSocketAddress (Ipv4Address(s1.c_str()), downPort));
      sinkApps.Add(sink.Install(m_nodes.Get(i)));
      PacketSinkHelper apsink ("ns3::TcpSocketFactory",
                        InetSocketAddress (Ipv4Address(s2.c_str()), upPort));
      apSinkApps.Add(apsink.Install (m_nodes.Get(j-1)));
           

      std::string s3;
      std::stringstream ss3(s3);
      if(m_downlinkUplink){
        ss3 << in_dataRate;
      }

      s3 = ss3.str() + "bps";

      if(m_downlinkUplink){
        bulk.SetAttribute ("MaxBytes", UintegerValue (0));
        bulk.SetAttribute ("StartTime", TimeValue (Seconds (1.00+static_cast<double>(i)/100)));
        bulk.SetAttribute ("StopTime", TimeValue (Seconds (simulationTime +static_cast<double>(i)/100)));
        m_cbrApps.Add (bulk.Install (m_nodes.Get (j-1)));

        OnOffHelper server ("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address(s2.c_str()), upPort));
        server.SetAttribute ("PacketSize", UintegerValue (in_packetSize));
        server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=5]"));
        server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
        server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
        m_upApps.Add (server.Install (m_nodes.Get (i))); 
      }
    }
  }  

  sinkApps.Start (Seconds (0.00));
  sinkApps.Stop (Seconds (simulationTime));

  apSinkApps.Start (Seconds (0.00));
  apSinkApps.Stop (Seconds (simulationTime));
     
  //again using different start times to workaround Bug 388 and Bug 912
  for(size_t j=1; j<=m_apNumber; ++j){
    for(size_t i=m_apNumber+(m_nodeNumber/m_apNumber)*(j-1); 
        i<m_apNumber+(m_nodeNumber/m_apNumber)*j ; ++i){
      std::string s1;
      std::string s2;
      std::stringstream ss1(s1);
      std::stringstream ss2(s2);
      if(m_downlinkUplink){
         ss1 << i+1;
         ss2 << j;
      }

      s1 = "10.0.0."+ss1.str();
      s2 = "10.0.0."+ss2.str();
      UdpEchoClientHelper echoDownHelper (Ipv4Address (s1.c_str()), echoDownPort);
      echoDownHelper.SetAttribute ("MaxPackets", UintegerValue (1));
      echoDownHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
      echoDownHelper.SetAttribute ("PacketSize", UintegerValue (10));
      if(m_downlinkUplink){
        echoDownHelper.SetAttribute ("StartTime", TimeValue (Seconds (0.001)));
        echoDownHelper.SetAttribute ("StopTime", TimeValue (Seconds (50.001)));
        m_pingDownApps.Add (echoDownHelper.Install (m_nodes.Get (j-1))); 
 
        
        UdpEchoClientHelper echoUpHelper (Ipv4Address (s2.c_str()), echoUpPort);
        echoUpHelper.SetAttribute ("MaxPackets", UintegerValue (1));
        echoUpHelper.SetAttribute ("Interval", TimeValue (Seconds (0.1)));
        echoUpHelper.SetAttribute ("PacketSize", UintegerValue (10));
        echoUpHelper.SetAttribute ("StartTime", TimeValue (Seconds (0.001)));
        echoUpHelper.SetAttribute ("StopTime", TimeValue (Seconds (50.001)));
        m_pingUpApps.Add (echoUpHelper.Install (m_nodes.Get (i))); 
        
      }
    }
  }
}


void
Experiment::ShowNodeInformation(NodeContainer in_c, size_t in_numOfNode)
{
  for(size_t i=0; i<in_numOfNode; ++i){
    Ptr<MobilityModel> mobility = in_c.Get(i)->GetObject<MobilityModel> ();
    Vector nodePos = mobility->GetPosition ();
    // Get Ipv4 instance of the node
    Ptr<Ipv4> ipv4 = in_c.Get(i)->GetObject<Ipv4> (); 
    // Get Ipv4 instance of the node
    Ptr<MacLow> mac48 = in_c.Get(i)->GetObject<MacLow> (); 
    // Get Ipv4InterfaceAddress of xth interface.
    Ipv4Address addr = ipv4->GetAddress (1, 0).GetLocal ();     
    //Mac48Address macAddr = mac48->GetAddress();
    std::cout << in_c.Get(i)->GetId() << " " << addr << " (" << nodePos.x << ", " <<  
               nodePos.y << ")" << std::endl;
  }
}

void 
Experiment::Run(size_t in_simTime)
{ 
  if (pcapTracing)
    {  
      spectrumPhy.SetPcapDataLinkType (SpectrumWifiPhyHelper::DLT_IEEE802_11_RADIO);
      spectrumPhy.EnablePcap ("AarfWifiManager", m_devices);
    } 
  // 8. Install FlowMonitor on all nodes
  Simulator::Schedule (Seconds (1.1), &CalculateThroughput);

  ShowNodeInformation(m_nodes, m_apNumber+m_nodeNumber);
  
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();

  // 9. Run simulation
  Simulator::Stop (Seconds (in_simTime));
  Config::Connect ("/NodeList/*/DeviceList/*/Phy/State/RxError", 
             MakeCallback (&Experiment::PhyRxErrorTrace, this));
  Config::Connect ("/NodeList/*/DeviceList/*/Phy/State/RxOk", 
             MakeCallback (&Experiment::PhyRxOkTrace, this));
  Config::Connect ("/NodeList/*/DeviceList/*/Phy/State/Tx", 
             MakeCallback (&Experiment::PhyTxTrace, this));
  Config::Connect ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/$ns3::WifiPhy/SignalArrival",
             MakeCallback (&Experiment::SpectrumSignalArrival, this));

  AnimationInterface anim ("AarfWifiManager.xml");
  Simulator::Run ();

  // 10. Print per flow statistics
  monitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
  double accumulatedThroughput = 0;
  for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i=stats.begin(); 
        i!=stats.end(); ++i)
  {    
    Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (i->first);
    std::cout << "Flow " << i->first<< " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
    std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
    std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
    std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
    std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
    std::cout << "  Lost Packets: " << i->second.lostPackets << "\n";
    std::cout << "  Pkt Lost Ratio: " << ((double)i->second.txPackets-(double)i->second.rxPackets)/(double)i->second.txPackets << "\n";
    std::cout << "  Throughput: " << i->second.rxBytes * 8.0 / in_simTime / 1024 / 1024  << " Mbps\n";

    /*Calculate throughput only for downlink*/
    for(size_t j=1; j<=m_apNumber; ++j){
      std::string ip;
      std::stringstream ssip(ip);
      ssip << j;
      ip = "10.0.0."+ssip.str();
      //std::cout << "\n" << "ip1 " << ip.c_str() << " ";
      if(t.sourceAddress == ip.c_str()) 
      {
       //std::cout << "ip2 " << ip.c_str() << " " ;
       accumulatedThroughput+=(i->second.rxBytes*8.0/in_simTime/1024/1024);
      }
      }
  }  
  std::cout << "apNumber=" <<m_apNumber << " nodeNumber=" << m_nodeNumber << "\n" << std::flush;
  std::cout << "throughput=" << accumulatedThroughput << "\n" << std::flush;
  std::cout << "tx=" << m_txOkCount << " RXerror=" <<m_rxErrorCount << 
               " Rxok=" << m_rxOkCount << " isNotWifi=" << isNotWifi << " sigPower="<< sigPower << "\n" << std::flush;
  std::cout << "===========================\n" << std::flush;
  // 11. Cleanup
  Simulator::Destroy ();
}

int main (int argc, char **argv)
{ 
  
  size_t numOfAp[6] = {1, 2, 3, 4, 5, 6};
  double range[4] = {60, 120, 180, 240};
  std::vector <std::string> modes;
  modes.push_back ("DsssRate1Mbps");
  modes.push_back ("DsssRate2Mbps");
  modes.push_back ("DsssRate5_5Mbps");
  modes.push_back ("DsssRate11Mbps");
  std::cout << "Hidden station experiment with RTS/CTS enabled:\n" << std::flush;
  for(size_t i=1; i<2; ++i){
    for(size_t j=0; j<1; ++j){
      for(size_t k=2; k<3; ++k){
        std::cout << "Range=" << range[j] << ", Mode=" << modes[k] << "\n";
        Experiment exp(Downlink, modes[k]);
        exp.SetRtsCts(true);
        exp.CreateNode(numOfAp[i], 4, range[j]); // case 1 & 2 = 2 ------case 2 & 3 = 4
        exp.InitialExperiment();
        exp.InstallApplication(1024, 5500000);
        exp.Run(simulationTime);  
      }
    }
  }
  /*
  std::cout << "Hidden station experiment with RTS/CTS enable:\n" << std::flush;
  for(size_t i=0; i<6; ++i){
    for(size_t j=0; j<4; ++j){
      for(size_t k=0; k<modes.size(); ++k){
        std::cout << "Range=" << range[j] << "Mode=" << modes[k] << "\n";
        Experiment exp(Downlink, modes[k]);
        exp.SetRtsCts(true);
        exp.CreateNode(numOfAp[i], 60, range[j]);
        exp.InitialExperiment();
        exp.InstallApplication(1024, 16000000);
        exp.Run(60);
      }
    }
  }
  */
  return 0;
}


    // m_apPosAlloc->Add(Vector(m_radius*std::cos(i*2*PI/m_apNumber), 
    //                   m_radius*std::sin(i*2*PI/m_apNumber), 1)); 
   // m_apPosAlloc->Add(Vector(m_radius*i,0, 1));



   // size_t inAp = i/(m_nodeNumber/m_apNumber);
   // double nodeRadius = rand()%120+(rand()%1000)/1000;
   // m_nodePosAlloc->Add(Vector(m_radius*std::cos(inAp*2*PI/m_apNumber)+
   //                     nodeRadius*std::cos((rand()%(2*PI_e5))/pow(10, 5)), 
   //                     m_radius*std::sin(inAp*2*PI/m_apNumber)+
   //                     nodeRadius*std::sin((rand()%(2*PI_e5))/pow(10, 5)), 
   //                     1)); 
   //m_nodePosAlloc->Add(Vector(m_radius*i,m_radius*(i+1),1)); 