#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/ElasticSketch.hpp"

   

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 3) {
        printf("Usage: %s [Memory(KB)] [Trace]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    int total_memory = atoi(argv[1]);
    float threshold = 0.0005;
    int heavy_memory = total_memory * 1024 / 4;
    int bucket_num = heavy_memory / 64;
    int counter_num = (total_memory * 1024 - heavy_memory);//each bucket occupies 1B
    int trace = atoi(argv[2]);
    
    // pcap traces
    std::string dir,filelist;
    if(trace == 19) {
        dir = "/home/mina/Documents/dataset/caida/2019/dirA/";
        filelist = "/home/mina/Documents/dataset/caida/2019/dirA/fileList.txt";
    }
    else if(trace == 18) {
        dir = "/home/mina/Documents/dataset/caida/2018/dirA/";
        filelist = "/home/mina/Documents/dataset/caida/2018/dirA/fileList.txt";
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
    // char dir[256] = "./";
    // char filelist[256] = "./fileList.txt";

    std::ifstream tracefiles(filelist);
    if (!tracefiles.is_open()) {
        std::cout << "Error opening file" << std::endl;
        return -1;
    }

    double error, precision, recall,  f1;
    double avg_f1 = 0;
    double avg_pre = 0, max_pre = 0, min_pre = 1;
    double avg_rec = 0, max_rec = 0, min_rec = 1;
    double avg_err = 0, max_err = 0, min_err = 100;
    double avg_time = 0, max_time = 0, min_time = 10000000000;
    double avg_thr = 0, max_thr = 0, min_thr = 10000000000;
    int total_len = 0;
    double avg_frac = 0;
    int avg_HIT = 0;

    Evaluation *eva = new Evaluation();

    for (std::string file; getline(tracefiles, file);) {
        InputAdaptor* adaptor =  new InputAdaptor(dir, file, buf_size);
        std::cout << "[Dataset]: " << dir << file << std::endl;
        std::cout << "[Message] Finish read data." << std::endl;
        std::unordered_map<key_tp,int> diff;
        diff.clear();
        edge_tp t;
        adaptor->Reset();
        int sum = adaptor->GetDataSize();
        int num = 0;
        while(adaptor->GetNext(&t) == 1) {
            // key_tp key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
            key_tp key = t.src_ip;
            if(num < sum / 2)
                diff[key]++;
            else
                diff[key]--;
            ++ num;
        }
        // std::cout << "[Message] Finish Insert hash table" << std::endl;
        std::cout << "[Message] Total packets: " << sum << std::endl;
        
        int HIT = 0;
        for (auto it = diff.begin(); it != diff.end(); it++)
        {
            HIT += abs(it->second);
        }
        HIT *= threshold; 
        avg_HIT += HIT;

        std::cout << "[Message] Threshold: " << HIT << std::endl;

        ESketch *sketch1 = new ESketch(bucket_num, counter_num);
        ESketch *sketch2 = new ESketch(bucket_num, counter_num);
        sketch1->Reset();
        sketch2->Reset();

        // Update sketch
        std::cout<<"updating"<<std::endl;
        double t1=0, t2=0;
        num = 0;
        adaptor->Reset();
        t1 = Evaluation::now_us();
        while(adaptor->GetNext(&t) == 1) {
            // if(ground[t.src_ip]<threshold)
            // key_tp key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
            key_tp key = t.src_ip;
            
            if(num < sum / 2)
                sketch1->Update(key,1);
            else
                sketch2->Update(key,1);
            ++ num;
        }

        t2 = Evaluation::now_us();
        // std::cout<<(t2-t1)<<std::endl;
        double throughput = sum/(double)(t2-t1)*1000000;

        // Query the result
        std::cout<<"querying"<<std::endl;
        myvector results;
        results.clear();
        t1 = Evaluation::now_us();
        int value = 0, all = 0, hit = 0, size = 0;
        error = 0;
        for(auto it = diff.begin();it != diff.end();++it){
            value = std::abs((int)sketch1->PointQuery(it->first) - (int)sketch2->PointQuery(it->first));
            uint truth = abs(it->second);
            if(truth > HIT){
                all++;
                if(value > HIT){
                    hit += 1;
                }
            }
            if(value > HIT){
                size += 1;
                error += 1.0*abs((long)truth - (long)value)/truth;
            }
        }
        error /= size;
        t2 = Evaluation::now_us();
        double dtime = (double)(t2-t1)/1000000;


        // Calculate accuracy
        precision = hit / (double)all;
        recall = hit / (double)size;
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

        // printf("Total packets: %lu \n", num_item);
        // printf("Total HHs: %lu \n", results.size());
        std::cout << std::setw(20) << std::left << "Algorithm"
            << std::setw(20) << std::left << "Memory"
            << std::setw(20) << std::left << "Threshold"
            << std::setw(20) << std::left << "F1"
            << std::setw(20) << std::left << "Precision"
            << std::setw(20) << std::left << "Recall" 
            << std::setw(20)<< std::left << "RE" 
            << std::setw(20) << std::left << "AE"
            << std::setw(20)  << std::left << "Throughput" << std::endl;
        std::cout << std::setw(20) << std::left <<  "Our"
            << std::setw(20) << std::left << total_memory
            << std::setw(20) << std::left << threshold
            << std::setw(20) << std::left << f1
            << std::setw(20) << std::left << precision
            << std::setw(20) << std::left << recall 
            << std::setw(20)<< std::left << error 
            << std::setw(20)  << std::left << throughput << std::endl;
        delete sketch1;
        delete sketch2;
        total_len ++;
    }

    delete eva;
    tracefiles.close();
    std::cout << "--------------------------------------- statistical ------------------------------" << std::endl;
    std::cout << std::setfill(' ');
    //std::cout << std::setw(20) << std::left << "Algorithm"
    std::ofstream txtfile;
    txtfile.open("HC_Accuracy_comparision_new.txt",std::ios::app);

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

    txtfile << std::setw(20) << std::left <<  "Elastic"
            << std::setw(20) << std::left << total_memory*2
            << std::setw(20) << std::left << threshold
            << std::setw(20) << std::left << trace
            << std::setw(20) << std::left << avg_HIT/total_len
            // << std::setw(20) << std::left << avg_frac / total_len
            // << std::setw(20) << std::left << avg_hh / total_len
            << std::setw(20) << std::left << avg_f1/total_len
            << std::setw(20) << std::left << avg_pre/total_len
            << std::setw(20) << std::left << avg_rec/total_len
            << std::setw(20) << std::left << avg_err/total_len
            << std::setw(20) << std::left << avg_time/total_len
            << std::setw(20) << std::left << avg_thr/total_len
            << std::setw(20)  << std::left << min_pre
            << std::setw(20)  << std::left << min_rec
            // << std::setw(20)  << std::left << min_err
            << std::setw(20)  << std::left << min_time
            << std::setw(20)  << std::left << min_thr
            << std::setw(20)  << std::left << max_pre
            << std::setw(20)  << std::left << max_rec
            // << std::setw(20)  << std::left << max_err
            << std::setw(20)  << std::left << max_time
            << std::setw(20)  << std::left << max_thr
                << std::endl;
    
    txtfile.close();
    return 0;

}