#!/bin/bash

make all

for i in 8
do
    ./our $i
    
    ./our_parallel $i
done