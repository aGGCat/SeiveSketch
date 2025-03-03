#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/CM.hpp"

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 3) {
        printf("Usage: %s [Memory(KB)] [Trace]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    // double thresh = 0.00018;
    // uint32_t threshold = 20000;
    int depth = 3;
    int memory = atoi(argv[1]);
    int trace = atoi(argv[2]);
    int width = (memory*1024/depth)/4;//each bucket occupies 4B
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
    // char dir[256] = "./";
    // char filelist[256] = "./fileList.txt";

    std::ifstream tracefiles(filelist);
    if (!tracefiles.is_open()) {
        std::cout << "Error opening file" << std::endl;
        return -1;
    }

    double error,ae;//precision, recall,  f1;
    // double avg_f1 = 0;
    // double avg_pre = 0, max_pre = 0, min_pre = 1;
    // double avg_rec = 0, max_rec = 0, min_rec = 1;
    double avg_aae = 0, max_ae = 0, min_ae = 10000000000;
    double avg_err = 0, max_err = 0, min_err = 100;
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
        middle = groundvec[m_num].second;
        large = groundvec[l_num].second;
        groundvec.clear();
        cout<<small<<" "<<large<<endl;
        CountMin *sketch = new CountMin(depth, width,10000);
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
        // for(auto it = groundvec.begin(); it != groundvec.end(); it++) {
        //     sketch->Update(it->first,it->second);
        //     // if(it->second < large && it->second >= small) {
        //     //     // it->second = small - 1;
        //     //     sketch->Update(it->first,1);
        //     // }
        //     // else
        //     //     sketch->Update(it->first,it->second);
        // }

        t2 = Evaluation::now_us();
        std::cout<<(t2-t1)<<std::endl;
        double throughput = sum/(double)(t2-t1)*1000000;

        // Query the result
        std::cout<<"querying"<<std::endl;
        // myvector results;
        // results.clear();
        // t1 = Evaluation::now_us();
        // sketch->Query(20000, results);
        // t2 = Evaluation::now_us();
        // double dtime = (double)(t2-t1)/1000000;

        double error_1 = 0.0, error_10=0.0,error_100=0.0,error_1000=0.0;
        double error_10000 = 0.0, error_100000=0.0;
        int sum_1 = 0, sum_10 =0, sum_100 = 0, sum_1000=0,sum_10000=0,sum_100000=0;
        ae = 0;
        double RE_S = 0.0, RE_M = 0.0, RE_L = 0.0;
        int sum_s = 0, sum_m = 0, sum_l = 0;
        // int est_1 = 0, est_10 = 0, est_100 = 0, est_1000 = 0, est_10000 = 0, est_100000 = 0;
        // val_tp cold_ = 0, warm_ = 0, hot_ = 0;
        // val_tp max_1 = 0, key_1 = 0;
        int get_m = 0, get_h = 0;
        for(auto it = ground.begin(); it != ground.end(); it++) {
            val_tp record = sketch->PointQuery(it->first);
            ae += abs((long)it->second - (long)record);

            if(it->second <= small) {
                sum_s ++;
                get_h += sketch->num_effect_h(it->first,small,large);
                get_m += sketch->num_effect_w(it->first,small,large);
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
                // if(max_1 < record) {
                //     key_1 = it->first;
                //     max_1 = record;
                // }
                error_1 += 1.0*abs((long)it->second - (long)record)/it->second;
                // if(record == 1) {
                //     ++est_1;
                // }
                // else if(record <= 10) {
                //     ++est_10;
                // }
                // else if(record <= 100) {
                //     ++est_100;
                // }
                // else if(record <= 1000) {
                //     ++est_1000;
                // }
                // else if(record <= 10000) {
                //     ++est_10000;
                // }
                // else {
                //     ++est_100000;
                // }
                // if(it->first == 1301450456){
                //     cold_ += sketch->getCold(it->first);
                //     warm_ += sketch->getWarm(it->first);
                //     hot_ += sketch->getHot(it->first);
                // }
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
        // std::cout<<max_1<<" "<<key_1<<std::endl;
        // std::cout<<sum_1<<" "<<" "<<est_1<<" "<<est_10<<" "<<est_100<<" "<<est_1000<<" "<<est_10000<<" "<<est_100000<<" "<<std::endl;
        // std::cout<<cold_<<" "<<warm_<<" "<<hot_<<std::endl;
        // std::cout<<cold_*1.0/(est_1000+est_10000+est_100000)<<" "<<warm_*1.0/(est_1000+est_10000+est_100000)<<" "<<hot_*1.0/(est_1000+est_10000+est_100000)<<std::endl;
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
        // std::cout<<small<<" "<<large<<std::endl;
        // std::cout<<sum_s<<" "<<sum_m<<" "<<sum_l<<std::endl;
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
        std::cout << std::setw(20) << std::left <<  "CoutMin"
            << std::setw(20) << std::left << memory
            // << std::setw(20) << std::left << threshold
            // << std::setw(20) << std::left << f1
            // << std::setw(20) << std::left << precision
            // << std::setw(20) << std::left << recall 
            << std::setw(20) << std::left << RE_S / sum_s
            << std::setw(20) << std::left << RE_L / sum_l
            << std::setw(20)<< std::left << error 
            << std::setw(20) << std::left << ae
            << std::setw(20)  << std::left << throughput << std::endl;
        delete sketch;
        delete adaptor;
        ground.clear();
        if(total_len == 0) {
            cout<<get_h<<" "<<get_m<<" "<<sum_s<<endl;
        }
        total_len ++;
    }

    delete eva;
    tracefiles.close();
    std::cout << "--------------------------------------- statistical ------------------------------" << std::endl;
    std::cout << std::setfill(' ');
    //std::cout << std::setw(20) << std::left << "Algorithm"
    std::ofstream txtfile;
    txtfile.open("accuracy_frequency.txt",std::ios::app);
    // txtfile<<"threshold = "<<threshold<<" , depth = "<<depth<<" , width = "<<width<<std::endl;
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
    txtfile << std::setw(20) << std::left <<  "CM"
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