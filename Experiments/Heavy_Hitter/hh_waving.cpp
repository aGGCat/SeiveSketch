#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/WavingSketch.hpp"
// #include "src/sketch_waving.hpp"

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 3) {
        printf("Usage: %s [threshold] [Memory(KB)]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 500000000;
    // double thresh = 0.00018;
    uint32_t threshold;
    int memory = atoi(argv[1]);
    int slot = 16;
    int counter = 32;
    int bucket = memory*1024/((slot*8+counter*2));
    int trace = atoi(argv[2]);
    // pcap traces
    std::string dir,filelist;
    if(trace == 19) {
        dir = "/home/mina/Documents/dataset/caida/2019/dirA/";
        filelist = "/home/mina/Documents/dataset/caida/2019/dirA/fileList.txt";
    }
    else if(trace == 18) {
        dir = "/home/mina/Documents/dataset/caida/2018/dirB/";
        filelist = "/home/mina/Documents/dataset/caida/2018/dirB/fileList.txt";
    }
    else if(trace == 16) {
        dir = "/home/mina/Documents/dataset/caida/2016/dirA/";
        filelist = "/home/mina/Documents/dataset/caida/2016/dirA/fileList.txt";
    }
    else if(trace == 1) {
        dir = "/home/mina/Documents/dataset/univ1/";
        filelist = "/home/mina/Documents/dataset/univ1/fileList.txt";
    }
    else if(trace == 2) {
        dir = "/home/mina/Documents/dataset/univ2/";
        filelist = "/home/mina/Documents/dataset/univ2/fileList.txt";
    }
    else if(trace == 23) {
        dir = "/home/mina/Documents/dataset/MAWI_F/";
        filelist = "/home/mina/Documents/dataset/MAWI_F/fileList.txt";
    }

    std::ifstream tracefiles(filelist);
    if (!tracefiles.is_open()) {
        std::cout << "Error opening file" << std::endl;
        return -1;
    }

    double precision, recall, error, f1;
    double avg_f1 = 0;
    double avg_pre = 0, max_pre = 0, min_pre = 1;
    double avg_rec = 0, max_rec = 0, min_rec = 1;
    double avg_err = 0, max_err = 0, min_err = 100;
    double avg_time = 0, max_time = 0, min_time = 10000000000;
    double avg_thr = 0, max_thr = 0, min_thr = 10000000000;
    int total_len = 0;
    double avg_hh = 0;


    double avg_error_1 = 0.0, avg_error_10=0.0,avg_error_100=0.0,avg_error_1000=0.0;
    double avg_error_10000 = 0.0, avg_error_100000=0.0;
    Evaluation *eva = new Evaluation();

    for (std::string file; getline(tracefiles, file);) {
        InputAdaptor* adaptor =  new InputAdaptor(dir, file, buf_size);
        std::cout << "[Dataset]: " << dir << file << std::endl;
        std::cout << "[Message] Finish read data." << std::endl;
        std::unordered_map<key_tp,val_tp> ground;
        ground.clear();
        edge_tp t;
        adaptor->Reset();
        int sum = 0;
        while(adaptor->GetNext(&t) == 1) {
            // flow_key key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
            key_tp key = t.src_ip;
            ground[key]++;
            ++ sum;
        }
        // std::cout << "[Message] Finish Insert hash table" << std::endl;
        std::cout << "[Message] Total packets: " << sum << "  threshold = " << threshold << std::endl;
        threshold = sum * 0.0005;
        myvector truth;
        for(auto it = ground.begin(); it != ground.end(); ++it) {
            if(it->second > threshold) {
                std::pair<key_tp, val_tp> node;
                node.first = it->first;
                node.second = it->second;
                truth.push_back(node);
            }
        }
        avg_hh += truth.size();
        std::cout << "[Message] True HHs: " << truth.size() << std::endl;


        // CountMin *sketch = new CountMin(depth, width, threshold);
        WavingSketch *sketch = new WavingSketch(bucket,slot,counter);
        // Abstract *sketch = new WavingSketch<8,1>(memory / (8*8+1*2));

        // Update sketch
        std::cout<<"updating"<<std::endl;
        double t1=0, t2=0;
        t1 = Evaluation::now_us();
        adaptor->Reset();
        while(adaptor->GetNext(&t) == 1) {
            sketch->Update(t.src_ip,1);
        }
        t2 = Evaluation::now_us();
        double throughput = sum/(double)(t2-t1)*1000000;

        // Query the result
        std::cout<<"querying"<<std::endl;
        myvector results;
        results.clear();
        t1 = Evaluation::now_us();
        sketch->Query(threshold, results);
        t2 = Evaluation::now_us();
        double dtime = (double)(t2-t1)/1000000;

        // Calculate accuracy
        eva->get_accuracy(truth, results, &precision, &recall, &error);
        f1 = 2.0*precision*recall/(precision+recall);
        avg_f1 += f1;
        avg_pre += precision;
        avg_rec += recall;
        avg_err += error;
        avg_time += dtime;
        avg_thr += throughput;
        min_pre = min_pre > precision ? precision : min_pre;
        max_pre = max_pre < precision ? precision : max_pre;
        min_rec = min_rec > recall ? recall : min_rec;
        max_rec = max_rec < recall ? recall : max_rec;
        min_err = min_err > error ? error : min_err;
        max_err = max_err < error ? error : max_err;
        min_time = min_time > dtime ? dtime : min_time;
        max_time = max_time < dtime ? dtime : max_time;
        min_thr = min_thr > throughput ? throughput : min_thr;
        max_thr = max_thr < throughput ? throughput : max_thr;
        std::cout << "Memory = " << memory << std::endl;
        // printf("Total packets: %lu \n", num_item);
        printf("Total HHs: %lu \n", results.size());
        std::cout << std::setw(20) << std::left << "Algorithm"
            << std::setw(20) << std::left << "Memory"
            << std::setw(20) << std::left << "Threshold"
            << std::setw(20) << std::left << "F1"
            << std::setw(20) << std::left << "Precision"
            << std::setw(20) << std::left << "Recall" 
            << std::setw(20)<< std::left << "Error" 
            << std::setw(20) << std::left << "Time"
            << std::setw(20)  << std::left << "Throughput" << std::endl;
        std::cout << std::setw(20) << std::left <<  "Waving"
            << std::setw(20) << std::left << memory
            << std::setw(20) << std::left << threshold
            << std::setw(20) << std::left << f1
            << std::setw(20) << std::left << precision
            << std::setw(20) << std::left << recall 
            << std::setw(20)<< std::left << error 
            << std::setw(20) << std::left << dtime
            << std::setw(20)  << std::left << throughput << std::endl;
        delete sketch;
        total_len ++;
    }

    delete eva;
    tracefiles.close();
    std::cout << "--------------------------------------- statistical ------------------------------" << std::endl;
    std::cout << std::setfill(' ');
    //std::cout << std::setw(20) << std::left << "Algorithm"
    std::ofstream txtfile;
    txtfile.open("HH_Accuracy_comparision.txt",std::ios::app);
    std::cout << std::setw(20) << std::left << "Algorithm"
        << std::setw(20) << std::left << "Memory"
        << std::setw(20) << std::left << "Threshold"
        << std::setw(20) << std::left << "Precision"
        << std::setw(20) << std::left << "Recall"
        << std::setw(20) << std::left << "Error"
        << std::setw(20) << std::left << "Time"
        << std::setw(20) << std::left << "Throughput"
        << std::setw(20)  << std::left << "MinPre"
            << std::setw(20)  << std::left << "MinRec"
            << std::setw(20)  << std::left << "MinErr"
            << std::setw(20)  << std::left << "MinTime"
            << std::setw(20) << std::left << "MinThr"
            << std::setw(20)  << std::left << "MaxPre"
            << std::setw(20)  << std::left << "MaxRec"
            << std::setw(20)  << std::left << "MaxErr"
            << std::setw(20)  << std::left << "MaxTime"
            << std::setw(20) << std::left << "MaxThr"
            << std::endl;

    txtfile << std::setw(20) << std::left <<  "Waving"
        << std::setw(20) << std::left << memory
        << std::setw(20) << std::left << threshold
        << std::setw(20) << std::left << trace
        << std::setw(20) << std::left << avg_hh / total_len
        << std::setw(20) << std::left << avg_f1/total_len
        << std::setw(20) << std::left << avg_pre/total_len
        << std::setw(20) << std::left << avg_rec/total_len
        << std::setw(20) << std::left << avg_err/total_len
        << std::setw(20) << std::left << avg_time/total_len
        << std::setw(20) << std::left << avg_thr/total_len
        << std::setw(20)  << std::left << min_pre
        << std::setw(20)  << std::left << min_rec
        << std::setw(20)  << std::left << min_err
        << std::setw(20)  << std::left << min_time
        << std::setw(20)  << std::left << min_thr
        << std::setw(20)  << std::left << max_pre
        << std::setw(20)  << std::left << max_rec
        << std::setw(20)  << std::left << max_err
        << std::setw(20)  << std::left << max_time
        << std::setw(20)  << std::left << max_thr
            << std::endl;
    txtfile.close();
    return 0;

}