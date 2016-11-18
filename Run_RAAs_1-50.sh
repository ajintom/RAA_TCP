#! /bin/bash
echo "Initialising and printing nStats array"
#declare -a RAA=(AparfWifiManager AmrrWifiManager AarfWifiManager AarfcdWifiManager CaraWifiManager IdealWifiManager OnoeWifiManager MinstrelHtWifiManager ConstantRateWifiManager)
declare -a RAA=(IdealWifiManager)
echo ${RAA[*]}
cd $(dirname "${BASH_SOURCE[0]}")
cd ..
cd ..
for i in "${RAA[@]}"
do
	for j in {16..50}
	do
		echo "Running RAA =" $i
		echo "nStas=" $j
		./waf --cwd=Results --run 'RAA_TCP --RateManager='$i' --nStas='$j
	done	
done