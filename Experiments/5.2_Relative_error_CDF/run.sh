#!/bin/bash

make all

for i in 8
do
    ./cdf_our $i
    
    ./cdf_cm $i

    ./cdf_elastic $i

    ./cdf_cf $i

    ./cdf_as $i

    ./cdf_laf $i
done