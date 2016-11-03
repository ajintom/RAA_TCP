#! /bin/bash
echo "Initialising and printing nStats array"
declare -a nStats=(5 10 15 20 25 30 35 40 45 50)
echo ${nStats[*]}
for i in "${nStats[@]}"
do
	echo "Running for nStats =" $i

	result=${PWD##*/}
	actual="RAA_TCP"

	if [ "$result" = "$actual" ]; then
		cd ..
		cd ..
	
	else
		(cd ../../; pwd);
		
	fi
	
	./waf --run 'RAA_TCP --nStas='$i
done	
