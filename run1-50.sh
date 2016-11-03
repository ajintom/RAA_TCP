#! /bin/bash

echo "RAAs to run tests:"
declare -a RAA=(AparfWifiManager AmrrWifiManager);
echo ${RAA[*]}
for i in ${RAA[@]}
	do
	echo "Running RAA =" $i
		for j in {1..2}
		do
		echo "Running for nStats =" $j
#(cd ../../; pwd)
		./waf --run 'RAA_TCP --nStas='$j '--RateManager='$i
		done	

done