#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/AugmentedSketch.hpp"

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 2) {
        printf("Usage: %s [Memory(KB)]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    int depth = 3;
    int memory = atoi(argv[1]);
    int k_num = 32;
    int width = (memory*1024 - 12 * k_num)/depth/4;//each bucket occupies 4B


    std::string file = "../../data/data.pcap";

    double error,ae;
    double avg_err = 0, max_err = 0, min_err = 100;
    double avg_thr = 0, max_thr = 0, min_thr = 10000000000;
    val_tp small = 0, large = 0;


    double avg_error_1 = 0.0, avg_error_10=0.0,avg_error_100=0.0,avg_error_1000=0.0;
    double avg_error_10000 = 0.0, avg_error_100000=0.0;
    double ARE_S = 0.0, ARE_M = 0.0, ARE_L = 0.0;
    Evaluation *eva = new Evaluation();

    InputAdaptor* adaptor =  new InputAdaptor("", file, buf_size);
    std::cout << "[Dataset]: " << file << std::endl;
    std::cout << "[Message] Finish read data." << std::endl;
    std::unordered_map<key_tp,val_tp> ground;
    ground.clear();
    edge_tp t;
    adaptor->Reset();
    int sum = 0;
    while(adaptor->GetNext(&t) == 1) {
        // key_tp key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
        key_tp key = t.src_ip;
        ground[key]++;
        ++ sum;
    }
    // std::cout << "[Message] Finish Insert hash table" << std::endl;
    std::cout << "[Message] Total packets: " << sum <<"; distinct flows:"<< ground.size() << std::endl;

    // sort
    std::vector<std::pair<key_tp, val_tp> > groundvec;
    groundvec.clear();
    for (auto it = ground.begin(); it != ground.end(); it++) {
        groundvec.push_back(*it);
    }
    std::sort(groundvec.begin(), groundvec.end(), [=](std::pair<key_tp, val_tp>&a, std::pair<key_tp, val_tp>&b)
        {
            return a.second < b.second;
        }
    );

    int m_num = ground.size() * 0.8;
    int l_num = ground.size() * 0.99;
    small = groundvec[m_num].second;
    large = groundvec[l_num].second;
    groundvec.clear();

    ASketch *sketch = new ASketch(depth, width, k_num);
    sketch->Reset();

    // Update sketch
    std::cout<<"updating"<<std::endl;
    double t1=0, t2=0;
    adaptor->Reset();
    t1 = Evaluation::now_us();
    while(adaptor->GetNext(&t) == 1) {
        // if(ground[t.src_ip]<threshold)
        // key_tp key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
        key_tp key = t.src_ip;
        sketch->Update(key,1);
    }

    t2 = Evaluation::now_us();
    double throughput = sum/(double)(t2-t1)*1000000;

    // Query the result
    std::cout<<"querying"<<std::endl;

    double error_1 = 0.0, error_10=0.0,error_100=0.0,error_1000=0.0;
    double error_10000 = 0.0, error_100000=0.0;
    int sum_1 = 0, sum_10 =0, sum_100 = 0, sum_1000=0,sum_10000=0,sum_100000=0;
    ae = 0;
    double RE_S = 0.0, RE_M = 0.0, RE_L = 0.0;
    int sum_s = 0, sum_m = 0, sum_l = 0;
    for(auto it = ground.begin(); it != ground.end(); it++) {
        int record = sketch->PointQuery(it->first);
        ae += abs((long)it->second - (long)record);
        if(it->second < small) {
            sum_s ++;
            RE_S += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        else if (it->second < large) {
            sum_m ++;
            RE_M += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        else {
            sum_l ++;
            RE_L += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        if(it->second == 1) {
            sum_1++;
            error_1 += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        else if(it->second <= 10) {
            sum_10++;
            error_10 += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        else if(it->second <= 100) {
            sum_100++;
            error_100 += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        else if(it->second <= 1000) {
            sum_1000++;
            error_1000 += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        else if(it->second <= 10000) {
            sum_10000++;
            error_10000 += 1.0*abs((long)it->second - (long)record)/it->second;
        }
        else{
            sum_100000++;
            error_100000 += 1.0*abs((long)it->second - (long)record)/it->second;
        }
    }
    avg_error_1 += error_1 / sum_1;
    avg_error_10 += error_10 / sum_10;
    avg_error_100 += error_100 / sum_100;
    avg_error_1000 += error_1000 / sum_1000;
    avg_error_10000 += error_10000 / sum_10000;
    avg_error_100000 += error_100000 / sum_100000;
    error = (error_1 + error_10 + error_100 + error_1000 + error_10000 + error_100000) / ground.size();
    ae = ae / ground.size();
    std::cout<<"[1] error is "<<error_1  / sum_1<<std::endl;
    std::cout<<"(1-10] error is "<<error_10  / sum_10<<std::endl;
    std::cout<<"(10-100] error is "<<error_100  / sum_100<<std::endl;
    std::cout<<"(100-1000] error is "<<error_1000/ sum_1000 <<std::endl;
    std::cout<<"(1000-10000] error is "<<error_10000/ sum_10000 <<std::endl;
    std::cout<<"(10000-inf] error is "<<error_100000/ sum_100000<<std::endl;


    ARE_S += RE_S / sum_s;
    ARE_M += RE_M / sum_m;
    ARE_L += RE_L / sum_l;
    avg_err += error;
    avg_thr += throughput;
    min_err = min_err > error ? error : min_err;
    max_err = max_err < error ? error : max_err;
    min_thr = min_thr > throughput ? throughput : min_thr;
    max_thr = max_thr < throughput ? throughput : max_thr;

    std::cout << std::setw(20) << std::left << "Algorithm"
        << std::setw(20) << std::left << "Memory"
        << std::setw(20)<< std::left << "RE" 
        << std::setw(20) << std::left << "AE"
        << std::setw(20)  << std::left << "Throughput" << std::endl;
    std::cout << std::setw(20) << std::left <<  "Augmented"
        << std::setw(20) << std::left << memory
        << std::setw(20)<< std::left << error 
        << std::setw(20) << std::left << ae
        << std::setw(20)  << std::left << throughput << std::endl;
    delete sketch;
    delete adaptor;
    delete eva;
    ground.clear();
    return 0;
}