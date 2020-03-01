#!/bin/bash

for i in `seq 1 2`;
do
    echo "Iniciando prueba" $i $'\n'
    size=`expr $i \* 1024`
    ./sim.exe -bs 16 -is $size -ds $size -a 1 -wb -wa  trazas/spice.trace
done