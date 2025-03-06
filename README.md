# SeiveSketch

## Files
- Appendix: some supplements to the paper

- data: a small dataset for testing

- Experiments: the folder contains the experiments in our paper
    - 5.2_Frequency_Estimation: experiments on item frequency estimation in Section 5.2
    - 5.2_Relative_error_CDF: experiments on relative error cdf in Section 5.2
    - 5.2_Skewness: experiments on item frequency estimation under different skewness in Section 5.2
    - 5.3_Heavy_Hitter: experiments on heavy hitter detection in Section 5.3
    - 5.4_Heavy_Change: experiments on heavy change detection in Section 5.4
    - 5.4_Frequency_Distribution: experiments on item frequency distribution in Section 5.4

- src: the folder contains the source files of sketches.



## Compile and Run the examples
SeiveSketch and all compared sketches are implemented with C++. We compile these examples on Ubuntu with g++ and make.
```
cd Experiments/5.2_Frequency_Estimation/
make all
./our [Memory size]
```

### Requirements
- Ensure g++ and make are installed. Our experimental platform is equipped with Ubuntu 20.04, g++ 9.4.0 and make 4.2.1.
- Ensure the necessary library libpcap is installed.
    - It can be installed by the following command 
        - `apt-get install libpcap-dev in Ubuntu`



### Datasets
You can download datasets from the following links.

CAIDA: https://catalog.caida.org/dataset/passive_2019_pcap

WebDocs: http://fimi.uantwerpen.be/data/

MAWI: https://mawi.wide.ad.jp/mawi/samplepoint-F/2023/

Twitter: https://anlab-kaist.github.io/traces/WWW2010

IMDB: https://developer.imdb.com/non-commercial-datasets/