#! /bin/bash

echo "RAAs to run tests:"
declare -a RAA=(AparfWifiManager AmrrWifiManager);
echo ${RAA[*]}
cd $(dirname "${BASH_SOURCE[0]}")
cd ..
cd ..
for i in ${RAA[@]}
	do
	echo "Running RAA =" $i
		for j in {1..2}
		do
		echo "Running for nStats =" $j
		./waf --run 'RAA_TCP --nStas='$j '--RateManager='$i
		done	

done