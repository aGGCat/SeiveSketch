# SeiveSketch

## Files
- `Appendix`: some supplements to the paper

- `data`: a small dataset for testing

- `Experiments`: the folder contains the experiments in our paper
    - 5.2_Frequency_Estimation: experiments on item frequency estimation in Section 5.2
    - 5.2_Relative_error_CDF: experiments on relative error cdf in Section 5.2
    - 5.2_Skewness: experiments on item frequency estimation under different skewness in Section 5.2
    - 5.3_Heavy_Hitter: experiments on heavy hitter detection in Section 5.3
    - 5.4_Heavy_Change: experiments on heavy change detection in Section 5.4
    - 5.4_Frequency_Distribution: experiments on item frequency distribution in Section 5.4

- `src`: the folder contains the source files of sketches.



## Compile and Run the Examples
SeiveSketch and all compared sketches are implemented with C++. We compile these examples on Ubuntu with g++ and make.
```
cd Experiments/5.2_Frequency_Estimation/
make all
./our [Memory size]
```

### Requirements
- Ensure g++ and make are installed. Our experimental platform is equipped with Ubuntu 20.04, g++ 9.4.0 and make 4.2.1.
- Ensure the necessary library libpcap and PcapPlusPlus are installed.
    - Library libpcap can be installed by the following command 
        - `apt-get install libpcap-dev in Ubuntu`
    - PcapPlusPlus is open source on Github, you can install it by following the [installation tutorial](https://pcapplusplus.github.io/docs/install/linux) on its official website.
        - its link: https://github.com/seladb/PcapPlusPlus/releases
        - We use the November 2022 release of PcapPlusPlus (v22.11)



### Datasets
In our paper, we use five datasets (CAIDA, WebDocs, MAWI, Twitter, IMDB) to conduct the experiments. You can download these datasets from the following links. We provide a small dataset in this project (See details in the `data` folder)

CAIDA: https://catalog.caida.org/dataset/passive_2019_pcap

WebDocs: http://fimi.uantwerpen.be/data/

MAWI: https://mawi.wide.ad.jp/mawi/samplepoint-F/2023/

Twitter: https://anlab-kaist.github.io/traces/WWW2010

IMDB: https://developer.imdb.com/non-commercial-datasets/