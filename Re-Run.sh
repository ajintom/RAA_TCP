#! /bin/bash
echo "Initialising and printing nStats array"
declare -a nStats=(5 10 15 20 25 30 35 40 45 50)
echo ${nStats[*]}
cd $(dirname "${BASH_SOURCE[0]}")
cd ..
cd ..
for i in "${nStats[@]}"
do
	./waf --run 'RAA_TCP --nStas='$i
done	
