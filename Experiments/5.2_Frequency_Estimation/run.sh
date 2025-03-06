#!/bin/bash

make all

for i in 8
do
    ./our $i
    
    ./cm $i

    ./elastic $i

    ./cf $i

    ./as $i

    ./laf $i
done