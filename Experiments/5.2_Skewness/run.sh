#!/bin/bash

make all

for s in 0 0.2 0.4 0.6 0.8 1.0 1.2 1.4 1.6 1.8
do
    for i in 8
    do
        ./skew_our $s $i
        
        ./skew_cm $s $i

        ./skew_elastic $s $i

        ./skew_cf $s $i

        ./skew_as $s $i

        ./skew_laf $s $i
    done
done