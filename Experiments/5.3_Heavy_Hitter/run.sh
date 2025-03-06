#!/bin/bash

make all

for i in 8
do
    ./hh_our $i
    
    ./hh_cm $i

    ./hh_es $i

    ./hh_ws $i

    ./hh_dhs $i

    ./hh_ss $i
done