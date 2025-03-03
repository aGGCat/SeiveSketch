# SeiveSketch

## Files
src/Ours.hpp: the implementation of SeiveSketch

Experiments/Frequency_Estimation: experiments on item frequency estimation

Experiments/Skewness: experiments on item frequency estimation under different skewness

Experiments/Heavy_Hitter: experiments on heavy hitter detection

Experiments/Heavy_Change: experiments on heavy change detection

Experiments/CDF_Error: experiments on relative error cdf 

Experiments/Frequency_Distribution: experiments on item frequency distribution

Appendix: some supplements to the paper

## Compile and Run the examples
SeiveSketch and all compared sketches are implemented with C++. We show how to compile the examples on Ubuntu with g++ and make.

### Requirements
- Ensure g++ and make are installed. Our experimental platform is equipped with Ubuntu 20.04, g++ 9.4.0 and make 4.2.1.
- Ensure the necessary library libpcap is installed.
    - It can be installed by the following command 
        - `apt-get install libpcap-dev in Ubuntu`



