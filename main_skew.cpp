#include <utility>
#include <fstream>
#include <iomanip>
#include <set>
#include <unordered_map>
#include <algorithm>
#include <math.h>
#include "lib/evaluation.hpp"
#include "lib/inputadaptor.hpp"
#include "src/Ours.hpp"

using namespace std;
int main(int argc, char* argv[]) {
    srand ( 1234567 );
    //parameter setting
    char dir[256] = "/home/mina/Documents/dataset/caida/2016/dirA/";
    char filenames[256] = "/home/mina/Documents/dataset/caida/2016/dirA/fileList.txt";
    unsigned long long buf_size = 500000000;
    double z = atof(argv[1]);// skewness

    //sketch parameters
    int memory = atoi(argv[2]);
    double h_ratio = 0.3;
    double s_ratio = 0.6;
    int depth = 3;
    double frac = 0.6321;
   
    //1. generate zipf sequence

    //2. construct data from CAIDA traces
    //2.1 read pcap and sort by frequency
    std::ifstream tracefiles(filenames);
    if (!tracefiles.is_open()) {
        std::cout << "Error opening file" << std::endl;
        return -1;
    }

    double avg_err = 0, avg_ae = 0;
    double avg_thr = 0;
    int total_len = 0;
    double avg_frac = 0.0,error = 0.0, ae = 0.0;

    double avg_error_1 = 0.0, avg_error_10=0.0,avg_error_100=0.0,avg_error_1000=0.0;
    double avg_error_10000 = 0.0, avg_error_100000=0.0;
    Evaluation *eva = new Evaluation();

    for (std::string file; getline(tracefiles, file);) {
        std::set<key_tp> edge;
        InputAdaptor *adaptor =  new InputAdaptor(dir, file, buf_size);
        //Get ground
        std::unordered_map<key_tp,val_tp> fremap;
        fremap.clear();
        edge_tp t;
        adaptor->Reset();
        unsigned long freq_sum = 0;
        unsigned long edge_sum = 0;
        while(adaptor->GetNext(&t) == 1) {
            key_tp ip = t.src_ip;
            // ip = (ip << 32) | t.dst_ip;
            fremap[ip]++;
            edge.insert(ip);
            freq_sum++;
        }
        edge_sum = edge.size();

        uint N = edge_sum;

        std::cout << "Zipf skew = " << z << std::endl;
        uint i = 0;
        double harmonic = 0;
        double *part = new double[N];
        double temp = 0;
        double *freq = new double[N];
        for (i = 1; i <= N; i++) {
            temp = pow((double)i, z);
            part[i] = temp;
            harmonic += 1.0/temp;
            //printf("[Test] i=%d, harmonic=%lf\n", i, harmonic);
        }
        harmonic = 1.0 / harmonic;
        double accu = 0;
        for (i = 1; i <= N; i++) {
            freq[i] = (harmonic / part[i]);
            accu += freq[i];
            if (i == 1000) {
                std::cout << " top 1000 accu=" << accu << std::endl;
            }
        }
        delete part;

        std::cout << "1. Finish construct zipf sequence" << std::endl;




        std::vector<std::pair<key_tp, val_tp> > groundvec;
        groundvec.clear();
        for (auto it = fremap.begin(); it != fremap.end(); it++) {
            groundvec.push_back(*it);
        }
        std::sort(groundvec.begin(), groundvec.end(), [=](std::pair<key_tp, val_tp>&a, std::pair<key_tp, val_tp>&b)
            {
                return a.second > b.second;
            }
        );

        delete adaptor;
        std::cout << "2.1 Finish sort pcap data" << std::endl;
        //2.2. re-set the frequency according to the zipf sequence
        //
        unsigned test = 0;
        for (i = 0; i < N; i++) {
            unsigned value = freq[i+1]*freq_sum;
            // if(i==(N-1))
            // std::cout<<i<<" "<<value<<" "<<freq[i+1]<<" "<<freq_sum<<std::endl;
            value = value > 1 ? value : 1;
            groundvec[i].second = value;
            test += value;
        }
        freq_sum = test;

        std::cout << "test=" << test << "\t total_pkt=" << freq_sum << std::endl;
        delete freq;
        std::cout << "2.2 Finish reset pcap frequency according to the zipf sequence" << std::endl;

        //2.3 construct the skewed pacp trace
        unsigned long *buffer = new unsigned long[freq_sum];
        int *shuffl = new int[freq_sum];
        for (i = 0; i < freq_sum; i++) {
            shuffl[i] = i;
        }

        //shufl
        for (i = freq_sum-1; i > 0; i--) {
            int j = rand() % (i+1);
            //std::cout <<"i=" << i << "\t j=" << j << std::endl;
            int temp = shuffl[i];
            shuffl[i] = shuffl[j];
            shuffl[j] = temp;
        }


        int index = 0;
        for (i = 0; i < N; i++) {
            for (uint j = 0; j < groundvec[i].second; j++) {
                buffer[shuffl[index]] = groundvec[i].first;
                index++;
            }
        }
        delete shuffl;
        std::cout << "2.3 Finish construct shuffl data" << std::endl;

        //3. calculate ground

        std::unordered_map<key_tp, val_tp> ground;
        for (i = 0; i < freq_sum; i++) {
            key_tp ip = buffer[i];
            ground[ip]++;
        }
        std::cout << "3 Finish construct ground truth" << std::endl;
        

        //4. update sketch
        SeiveSketch *sketch = new SeiveSketch(memory*1024,h_ratio,s_ratio,depth,frac);
        sketch->Reset();
        
        std::cout<<"updating"<<std::endl;
        double t1=0, t2=0;
        t1 = Evaluation::now_us();
        for(i=0; i < freq_sum; i++) {
            key_tp ip = buffer[i];
            sketch->Update(ip,1);
        }
        t2 = Evaluation::now_us();
        double throughput = freq_sum/(double)(t2-t1)*1000000;
        
        delete buffer;
        avg_frac += sketch->GetBias(1);

        std::cout<<"PointQuery"<<std::endl;
        double error_1 = 0.0, error_10=0.0,error_100=0.0,error_1000=0.0;
        double error_10000 = 0.0, error_100000=0.0;
        int sum_1 = 0, sum_10 =0, sum_100 = 0, sum_1000=0,sum_10000=0,sum_100000=0;
        ae = 0;
        for(auto it = ground.begin(); it != ground.end(); it++) {
            int record = sketch->PointQuery(it->first);
            ae += abs((long)it->second - (long)record);
            if(it->second == 1) {
                sum_1++;
                error_1 += 1.0*abs((long)it->second - (long)record)/it->second;
                // if(record != 1) {
                //     std::cout<<it->first<<" "<<it->second<<" "<<record<<std::endl;   
                // }     
            }
            if(it->second <= 10) {
                sum_10++;
                error_10 += 1.0*abs((long)it->second - (long)record)/it->second;
                // if(record != it->second)
                    // std::cout<<it->first<<" "<<it->second<<" "<<record<<std::endl;   
            }
            if(it->second <= 100) {
                sum_100++;
                error_100 += 1.0*abs((long)it->second - (long)record)/it->second;
                // double e = 1.0*abs((long)it->second - (long)record)/it->second;
                // if(it->second > 90)
                    // std::cout<<it->first<<" "<<it->second<<" "<<record<<" "<<e<<std::endl;
            }
            if(it->second <= 1000) {
                sum_1000++;
                error_1000 += 1.0*abs((long)it->second - (long)record)/it->second;
                // if(it->second > 128){
                //     std::cout<<it->first<<" "<<it->second<<" "<<record<<std::endl;
                // }
                // else {
                //     err_241 += 1.0*abs((long)it->second - (long)record)/it->second;
                //     sum_241 ++;
                // }
                // double e = 1.0*abs((long)it->second - (long)record)/it->second;
                // if(e >= 0.00232188)
                    // std::cout<<it->first<<" "<<it->second<<" "<<record<<" "<<e<<std::endl;
            }
            if(it->second <= 10000) {
                sum_10000++;
                // if(record < 255)
                //     std::cout<<it->first<<" "<<it->second<<" "<<record<<std::endl;
                error_10000 += 1.0*abs((long)it->second - (long)record)/it->second;
                // double e = 1.0*abs((long)it->second - (long)record)/it->second;
                // // if(record != it->second)
                //     std::cout<<it->first<<" "<<it->second<<" "<<record<<" "<<e<<std::endl;
            }
            
                sum_100000++;
                error_100000 += 1.0*abs((long)it->second - (long)record)/it->second;
                // double e = 1.0*abs((long)it->second - (long)record)/it->second;
                // if(e > 0.000416079)
                //     std::cout<<it->first<<" "<<it->second<<" "<<record<<" "<<e<<std::endl;
            
        }
        // std::ofstream txtfile;
        // double acc = 0; 
        // txtfile.open("results/skew_cdf.txt",std::ios::app);
        
        // txtfile << std::setw(20) << std::left <<z
        //         << std::setw(20) << std::left <<sum_1 * 1.0 / ground.size()
        //         << std::setw(20) << std::left <<sum_10 * 1.0 / ground.size()
        //         << std::setw(20) << std::left <<sum_100 * 1.0 / ground.size()
        //         << std::setw(20) << std::left <<sum_1000 * 1.0 / ground.size()
        //         << std::setw(20) << std::left <<sum_10000 * 1.0 / ground.size()
        //         << std::setw(20) << std::left <<sum_100000 * 1.0 / ground.size()<<std::endl;
        // return 0;
        // std::cout<<error_1<<" "<<sum_1<<" "<<error_10<<" "<<sum_10<<std::endl;
        avg_error_1 += error_1 / sum_1;
        avg_error_10 += error_10 / sum_10;
        avg_error_100 += error_100 / sum_100;
        avg_error_1000 += error_1000 / sum_1000;
        avg_error_10000 += error_10000 / sum_10000;
        avg_error_100000 += error_100000 / sum_100000;
        error = (error_1 + error_10 + error_100 + error_1000 + error_10000 + error_100000) / ground.size();
        ae = ae / ground.size();
        std::cout<<" [1] error is "<<error_1  / sum_1<<std::endl;
        std::cout<<"(1-10] error is "<<error_10  / sum_10<<std::endl;
        std::cout<<"(10-100] error is "<<error_100  / sum_100<<std::endl;
        std::cout<<"(100-1000] error is "<<error_1000/ sum_1000 <<std::endl;
        std::cout<<"(1000-10000] error is "<<error_10000/ sum_10000 <<std::endl;
        std::cout<<"(10000-inf] error is "<<error_100000/ sum_100000<<std::endl;

        avg_err += error;
        avg_ae += ae;
        avg_thr += throughput;

        std::cout << "4. Finish ,skew_our error = " <<error<< std::endl;
        sketch->Reset();
        delete sketch;
        total_len++;
        srand (259374261);
    }
    delete eva;
    tracefiles.close();

    std::ofstream txtfile;
    txtfile.open("results/skew_compare_1.txt",std::ios::app);
    std::cout << std::setw(20) << std::left << "Algorithm"
    << std::setw(20) << std::left << "skew"
    << std::setw(20) << std::left << "Memory"
    << std::setw(20) << std::left << "rate"
    << std::setw(20) << std::left << "smapling"
    << std::setw(20) << std::left << "error1"
    << std::setw(20) << std::left << "error10"
    << std::setw(20) << std::left << "error100"
    << std::setw(20) << std::left << "error1000"
    << std::setw(20) << std::left << "error10000"
    << std::setw(20) << std::left << "error100000"
    << std::setw(20) << std::left << "error"<<std::endl;

    txtfile << std::setw(20) << std::left <<"Ours"
    << std::setw(20) << std::left << z
    << std::setw(20) << std::left << memory
    // << std::setw(20) << std::left << frac
    // << std::setw(20) << std::left << avg_frac / total_len
    << std::setw(20) << std::left << avg_error_1 / total_len
    << std::setw(20) << std::left << avg_error_10 / total_len
    << std::setw(20) << std::left << avg_error_100 / total_len
    << std::setw(20) << std::left << avg_error_1000 / total_len
    << std::setw(20) << std::left << avg_error_10000 / total_len
    << std::setw(20) << std::left << avg_error_100000 / total_len
    << std::setw(20) << std::left << avg_err / total_len
    << std::setw(20) << std::left << avg_ae / total_len
    << std::setw(20) << std::left << avg_thr / total_len
    <<std::endl;
    txtfile.close();
    return 0;
}
