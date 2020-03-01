#!/bin/bash

echo "index, split I cache, split D cache, unified cache, assoc, block size, write, allocation, inst acc, inst mis, inst miss rate, inst hit rate, inst replace, data acc, data mis, data miss rate, data hit rate, data replace, demand fetch, copies back"

for i in `seq 1 10`;
do
    # echo "Iniciando prueba" $i $'\n'
    size=`expr $((2**$i)) \* 1024`
    res=$(./sim.exe -bs 16 -is $size -ds $size -a 1 -wb -wa  trazas/spice.trace)
    # res=$(ls -la nofile 2>&1)
    echo $i,$res
done
