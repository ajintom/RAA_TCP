#! /bin/bash
echo "Initialising and printing nStats array"
declare -a RAA=(AparfWifiManager AmrrWifiManager AarfWifiManager AarfcdWifiManager CaraWifiManager IdealWifiManager OnoeWifiManager MinstrelHtWifiManager ConstantRateWifiManager)
#declare -a RAA=(ConstantRateWifiManager)
echo ${RAA[*]}
cd ..
cd ..
for i in "${RAA[@]}"
do
	echo "Running RAA =" $i
	for j in {1..50}
	do
		echo "nStas=" $j
		./waf --run 'RAA_TCP --RateManager='$i' --nStas='$j
	done	
done	