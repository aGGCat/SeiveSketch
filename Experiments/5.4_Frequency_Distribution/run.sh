#!/bin/bash

make all

for i in 8
do
    for s in 1 4 16
    do
        ./fd_our $i $s
        
        ./fd_cm $i $s

        ./fd_elastic $i $s

        ./fd_cf $i $s

        ./fd_as $i $s

        ./fd_laf $i $s
    done
done