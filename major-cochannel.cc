#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/flow-monitor-module.h"
          

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

int 
main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t nWifiAp = 2;
  uint32_t nWifiSta = 2;

  CommandLine cmd;
 
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

  cmd.Parse (argc,argv);
  std::string phyMode ("DsssRate2Mbps");
  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifiSta);
  NodeContainer wifiApNode;
  wifiApNode.Create(nWifiAp);

  
  YansWifiChannelHelper channel;
  channel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  channel.AddPropagationLoss ("ns3::LogDistancePropagationLossModel");
  
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  
  phy.Set ("RxGain", DoubleValue (0) ); 
  phy.Set ("CcaMode1Threshold", DoubleValue (-90.0) );
  phy.SetChannel (channel.Create ());


  WifiHelper wifi = WifiHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  

  NqosWifiMacHelper mac = NqosWifiMacHelper::Default ();

  Ssid ssid = Ssid ("ns-3-ssid");
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices;
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  mac.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid));

  NetDeviceContainer apDevices;
  apDevices = wifi.Install (phy, mac, wifiApNode);
  Ptr<NetDevice> dev= apDevices.Get(0);
  Ptr<WifiNetDevice> dev1=DynamicCast<WifiNetDevice> (dev);
  Ptr<WifiPhy> phy1=dev1->GetPhy();
  Ptr<YansWifiPhy> wifiPhy= DynamicCast<YansWifiPhy> (phy1);
  wifiPhy->SetChannelNumber(6);
  
 
  Ptr<NetDevice> dev2= staDevices.Get(0);
  Ptr<WifiNetDevice> dev3=DynamicCast<WifiNetDevice> (dev2);
  Ptr<WifiPhy> phy2=dev3->GetPhy();
  Ptr<YansWifiPhy> wifiPhy1= DynamicCast<YansWifiPhy> (phy2);
  wifiPhy1->SetChannelNumber(6);


  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  InternetStackHelper stack;
  stack.Install (wifiApNode);
  stack.Install (wifiStaNodes);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ap_interface = address.Assign (apDevices.Get(0));
  Ipv4InterfaceContainer sta_interface = address.Assign (staDevices.Get(0));
 
  Ipv4InterfaceContainer ap_interface1 = address.Assign (apDevices.Get(1));
  Ipv4InterfaceContainer sta_interface1 = address.Assign (staDevices.Get(1));

  
  
  
//application on first Access point  
ApplicationContainer apps;
 OnOffHelper onoff ("ns3::UdpSocketFactory",InetSocketAddress (sta_interface.GetAddress(0), 1025));
onoff.SetAttribute("PacketSize", UintegerValue(1024));
 onoff.SetAttribute ("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
 //onoff.SetAttribute ("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=2]"));
 onoff.SetAttribute ("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
 onoff.SetAttribute("DataRate", StringValue("1Mbps"));
 apps = onoff.Install (wifiApNode.Get(0));
 apps.Start (Seconds (1.0));
 apps.Stop (Seconds (100.0));

 PacketSinkHelper sink ("ns3::UdpSocketFactory",InetSocketAddress (Ipv4Address::GetAny(), 1025));
 apps = sink.Install (wifiStaNodes);
 apps.Start (Seconds (0.0));
 apps.Stop (Seconds (100.0));

//application on second Access Point

ApplicationContainer apps1;
 OnOffHelper onoff1 ("ns3::UdpSocketFactory",InetSocketAddress (sta_interface1.GetAddress(0), 1025));
 onoff1.SetAttribute("PacketSize", UintegerValue(1024));
 onoff1.SetAttribute ("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
 //onoff1.SetAttribute ("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=2]"));
 onoff1.SetAttribute ("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
 onoff1.SetAttribute("DataRate", StringValue("1Mbps"));
 apps1 = onoff1.Install (wifiApNode.Get(1));
 apps1.Start (Seconds (1.0));
 apps1.Stop (Seconds (100.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  


  Simulator::Stop (Seconds (100.0));


  phy.EnablePcap ("wifi-1", staDevices.Get (0));
  

  Simulator::Run ();
  
  monitor->CheckForLostPackets();
  Ptr<Ipv4FlowClassifier> classifier=DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats> stats=monitor->GetFlowStats();
  for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i=stats.begin(); i!=stats.end(); ++i)
  {
        Ipv4FlowClassifier::FiveTuple t=classifier->FindFlow(i->first);
        std::cout<<"Flow" <<i->first -2 <<"("<<t.sourceAddress<<"->"<<t.destinationAddress<<")\n";
        std::cout<<" Tx Bytes:"<<i->second.txBytes << "\n";
        std::cout <<" Rx Bytes:"<<i->second.rxBytes<<"\n" ;
        std::cout<<" Throughput:" <<i->second.rxBytes*8.0/100.0/1024/1024 <<"Mbps\n";
  }
  


Simulator::Destroy ();
  return 0;
}