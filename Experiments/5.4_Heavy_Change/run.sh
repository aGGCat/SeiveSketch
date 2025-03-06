#!/bin/bash

make all

for i in 16
do
    ./hc_our $i
    
    ./hc_es $i

    ./hc_ws $i

    ./hc_dhs $i

    ./hc_ss $i
done