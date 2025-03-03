#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/ColdFilter.hpp"

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 3) {
        printf("Usage: %s [Memory(KB)] [Trace]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    // int depth = 4;
    int memory = atoi(argv[1]);
    float frac = 0.9;
    int trace = atoi(argv[2]);
    int fil_m = memory * frac * 1024;
    int ske_m = (memory * 1024 - fil_m)/4;

    // int width = (memory*1024/depth)/4;//each bucket occupies 4B
    // std::cout<<"depth = "<<depth<<" , width = "<<width<<std::endl;
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

    // char dir[256] = "/home/mina/Documents/dataset/caida/2019/dirA/";
    // char filelist[256] = "/home/mina/Documents/dataset/caida/2019/dirA/fileList.txt";

    std::ifstream tracefiles(filelist);
    if (!tracefiles.is_open()) {
        std::cout << "Error opening file" << std::endl;
        return -1;
    }

    double error,ae;//precision, recall,  f1;
    // double avg_f1 = 0;
    // double avg_pre = 0, max_pre = 0, min_pre = 1;
    // double avg_rec = 0, max_rec = 0, min_rec = 1;
    double avg_err = 0, max_err = 0, min_err = 100;
    double avg_aae = 0, max_ae = 0, min_ae = 10000000000;//Absolute Error
    // double avg_time = 0, max_time = 0, min_time = 10000000000;
    double avg_thr = 0, max_thr = 0, min_thr = 10000000000;
    int total_len = 0;
    val_tp small = 0, middle = 0, large = 0;


    double avg_error_1 = 0.0, avg_error_10=0.0,avg_error_100=0.0,avg_error_1000=0.0;
    double avg_error_10000 = 0.0, avg_error_100000=0.0;
    double ARE_S = 0.0, ARE_M = 0.0, ARE_L = 0.0;
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
            // key_tp key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
            key_tp key = t.src_ip;
            ground[key]++;
            ++ sum;
        }
        // std::cout << "[Message] Finish Insert hash table" << std::endl;
        std::cout << "[Message] Total packets: " << sum << std::endl;
        std::cout << "[Message] Unique IP: " << ground.size() << std::endl;

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
        middle = groundvec[m_num].second;
        large = groundvec[l_num].second;
        groundvec.clear();
        // std::cout<<small<<" "<<middle<<" "<<large<<std::endl;
        // for (auto it = groundvec.begin(); it != groundvec.end(); it++)
        // {
        //     std::cout<<it->first<<" "<<it->second<<std::endl;
        // }
        


        ColdFilter *sketch = new ColdFilter(fil_m, ske_m);

        // Update sketch
        std::cout<<"updating"<<std::endl;
        double t1=0, t2=0;
        t1 = Evaluation::now_us();
        adaptor->Reset();
        while(adaptor->GetNext(&t) == 1) {
            key_tp key = t.src_ip;
            // if(ground[key] < large && ground[key] >= small) {
            //     if(mid_key.find(key) == mid_key.end()) {
            //         mid_key.insert(key);
            //         sketch->Update(key,1);
            //     }
            // }
            // else
            sketch->Update(key,1);
        }
        t2 = Evaluation::now_us();
        double throughput = sum/(double)(t2-t1)*1000000;

        // Query the result
        std::cout<<"querying"<<std::endl;
        // myvector results;
        // results.clear();
        // t1 = Evaluation::now_us();
        // // sketch->Query(threshold, results);
        // for (auto it = ground.begin(); it != ground.end(); it++)
        // {
        //     uint est = sketch->PointQuery(it->first);
        //     if(est > threshold)
        //         results.push_back(std::make_pair(it->first,est));
        // }
        // t2 = Evaluation::now_us();
        // double dtime = (double)(t2-t1)/1000000;

        double error_1 = 0.0, error_10=0.0,error_100=0.0,error_1000=0.0;
        double error_10000 = 0.0, error_100000=0.0;
        int sum_1 = 0, sum_10 =0, sum_100 = 0, sum_1000=0,sum_10000=0,sum_100000=0;
        ae = 0;
        double RE_S = 0.0, RE_M = 0.0, RE_L = 0.0;
        int sum_s = 0, sum_m = 0, sum_l = 0;
        for(auto it = ground.begin(); it != ground.end(); it++) {
            int record = sketch->PointQuery(it->first);
            ae += abs((long)it->second - (long)record);
            if(total_len == 0) {
                std::ofstream txtfile;
                txtfile.open("results/point_CF_Est.csv",std::ios::app);
                txtfile<<it->second<<","<<record<<std::endl;
                txtfile.close();
            }
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
                // if(record != 1)
                // std::cout<<it->first<<" "<<it->second<<" "<< record<<std::endl;
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
                // std::cout<<it->first<<" "<<it->second<<" "<< record<<std::endl;
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

        // Calculate accuracy
        // eva->get_accuracy(truth, results, &precision, &recall, &error);
        // f1 = 2.0*precision*recall/(precision+recall);
        // avg_f1 += f1;
        // avg_pre += precision;
        // avg_rec += recall;
        avg_err += error;
        avg_aae += ae;
        // avg_time += dtime;
        avg_thr += throughput;
        // min_pre = min_pre > precision ? precision : min_pre;
        // max_pre = max_pre < precision ? precision : max_pre;
        // min_rec = min_rec > recall ? recall : min_rec;
        // max_rec = max_rec < recall ? recall : max_rec;
        min_err = min_err > error ? error : min_err;
        max_err = max_err < error ? error : max_err;
        min_ae = min_ae > ae ? ae : min_ae;
        max_ae = max_ae < ae ? ae : max_ae;
        // min_time = min_time > dtime ? dtime : min_time;
        // max_time = max_time < dtime ? dtime : max_time;
        min_thr = min_thr > throughput ? throughput : min_thr;
        max_thr = max_thr < throughput ? throughput : max_thr;
  
        // printf("Total packets: %lu \n", num_item);
        // printf("Total HHs: %lu \n", results.size());
        std::cout << std::setw(20) << std::left << "Algorithm"
            << std::setw(20) << std::left << "Memory"
            << std::setw(20) << std::left << "Threshold"
            // << std::setw(20) << std::left << "F1"
            // << std::setw(20) << std::left << "Precision"
            // << std::setw(20) << std::left << "Recall" 
            << std::setw(20)<< std::left << "RE" 
            << std::setw(20) << std::left << "AE"
            << std::setw(20)  << std::left << "Throughput" << std::endl;
        std::cout << std::setw(20) << std::left <<  "ColdFilter"
            << std::setw(20) << std::left << memory
            // << std::setw(20) << std::left << threshold
            // << std::setw(20) << std::left << f1
            // << std::setw(20) << std::left << precision
            // << std::setw(20) << std::left << recall 
            << std::setw(20)<< std::left << error 
            << std::setw(20)<< std::left << ae 
            // << std::setw(20) << std::left << dtime
            << std::setw(20)  << std::left << throughput << std::endl;
        ground.clear();
        delete adaptor;
        delete sketch;
        total_len ++;
    }

    delete eva;
    tracefiles.close();
    std::cout << "--------------------------------------- statistical ------------------------------" << std::endl;
    std::cout << std::setfill(' ');

    std::ofstream txtfile;
    txtfile.open("accuracy_frequency.txt",std::ios::app);
    std::cout << std::setw(20) << std::left << "Algorithm"
    << std::setw(20) << std::left << "Memory"
    << std::setw(20) << std::left << "Trace"
    << std::setw(20) << std::left << "error1"
    << std::setw(20) << std::left << "error10"
    << std::setw(20) << std::left << "error100"
    << std::setw(20) << std::left << "error1000"
    << std::setw(20) << std::left << "error10000"
    << std::setw(20) << std::left << "error100000"
    << std::setw(20) << std::left << "ARE"
    << std::setw(20) << std::left << "AAE"
    << std::setw(20) << std::left << "Throughput"
    <<std::endl;
    txtfile << std::setw(20) << std::left <<  "CF"
    << std::setw(20) << std::left << memory
    << std::setw(20) << std::left << trace
    << std::setw(20) << std::left << avg_error_1 / total_len
    << std::setw(20) << std::left << avg_error_10 / total_len
    << std::setw(20) << std::left << avg_error_100 / total_len
    << std::setw(20) << std::left << avg_error_1000 / total_len
    << std::setw(20) << std::left << avg_error_10000 / total_len
    << std::setw(20) << std::left << avg_error_100000 / total_len
    << std::setw(20) << std::left << ARE_S / total_len
    << std::setw(20) << std::left << ARE_M / total_len
    << std::setw(20) << std::left << ARE_L / total_len
    << std::setw(20) << std::left << avg_err / total_len
    << std::setw(20) << std::left << avg_aae / total_len
    << std::setw(20) << std::left << avg_thr / total_len
    << std::setw(20) << std::left << min_err / total_len
    << std::setw(20) << std::left << max_err / total_len
    << std::setw(20) << std::left << min_ae / total_len
    << std::setw(20) << std::left << max_ae / total_len
    <<std::endl;
    txtfile.close();
    
    return 0;
}