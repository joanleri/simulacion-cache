#!/bin/bash

echo "archivo, index, split I cache, split D cache, unified cache, assoc, block size, write, allocation, inst acc, inst mis, inst miss rate, inst hit rate, inst replace, data acc, data mis, data miss rate, data hit rate, data replace, demand fetch, copies back"

for f in "trazas/spice.trace" "trazas/cc.trace" "trazas/tex.trace";
do
    for i in `seq 2 12`;
    do
        # echo "Iniciando prueba" $i $'\n'
        size=`expr $((2**$i))`
        # assoc=`expr $((2**$i))`
        res=$(./sim.exe -bs $size -is 8192 -ds 8192 -a 2 -wb -wa  $f)
        echo $f,$i,$res
    done
done
