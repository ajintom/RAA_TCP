#! /bin/bash
for i in {1..50}
do
echo "Running for nStats =" $i
./waf --run 'RAA_TCP --nStas='$i
done	
