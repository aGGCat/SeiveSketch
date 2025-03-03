#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/Ours.hpp"

int cdf_range=10;//cdf_re in [0,0.1,...,0.9,1.0] 
int judge_cdf(double re);

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 3) {
        printf("Usage: %s [Memory(KB)] [Trace]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    int memory = atoi(argv[1]);
    double h_ratio = 0.3;
    double s_ratio = 0.6;
    int depth = 3;//atoi(argv[5]);
    double frac = 0.6321;
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
        std::cout << "[Message] Total packets: " << sum <<", distinct flows: "<<ground.size()<< std::endl;

        SeiveSketch *sketch = new SeiveSketch(memory*1024, h_ratio, s_ratio, depth, frac);
        sketch->Reset();

        // Update sketch
        std::cout<<"updating"<<std::endl;
        adaptor->Reset();
        while(adaptor->GetNext(&t) == 1) {
            // if(ground[t.src_ip]<threshold)
            // key_tp key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
            key_tp key = t.src_ip;
            sketch->Update(key,1);
        }
        

        // init cdf array
        uint64_t cdf[cdf_range+1];
        // cdf[i] indicates the number of items whose relative error<= i*0.1 
        // cdf_range=10 indicates cdf_re from 0 to cdf_range*0.1
        memset(cdf,0,(cdf_range+1)*sizeof(uint64_t));

        for(auto it = ground.begin(); it != ground.end(); ++it)
        {
            int val = sketch->PointQuery(it->first);
            double re=1.0*abs((long)it->second - (long)val)/it->second;    
            int pos=judge_cdf(re);
            cdf[pos]++;
        }

        // Analysis  Cumulative distribution function of relative error (CDF_RE)

        std::cout << std::setw(20) << std::left << "Algorithm"
            << std::setw(20) << std::left << "Memory"
            << std::setw(20) << std::left << "Trace"
            << std::setw(20) << std::left << "Relative Error"
            << std::setw(20) << std::left << "CDF"<< std::endl;

        uint64_t cdf_sum=0;
        std::ofstream txtfile;
        txtfile.open("Relative_error_cdf.txt",std::ios::app);

        for(int i=0;i<=cdf_range;i++)
        {   
            cdf_sum+=cdf[i];
            txtfile << std::setw(20) << std::left <<  "Our"
            << std::setw(20) << std::left << memory
            << std::setw(20) << std::left << trace
            << std::setw(20) << std::left << i*0.1
            << std::setw(20) << std::left << trace
            << std::setw(20) << std::left <<1.0*cdf_sum/ground.size()<<std::endl;
         
        }
        delete sketch;
        return 0;

    }
    delete eva;
    tracefiles.close();
    return 0;

}


int judge_cdf(double re)
{   
    /*
      re=0 return 0 ;
      re<=0.1 return 1;
      ...
      re<=0.9 return 9;
      re<1.0 return 10;
      re>=1.0 return 11;
    */
    
    int now=ceil(re*10); 
    if(re<=now*0.1)
    {   if(now>cdf_range)
            return cdf_range+1;
        else
            {   if(re==1)
                    return now+1;
                else
                    return now;
            }
    }
    else
    {
        std::cout<<"error in cdf_judge\n";
        return 0;
    }

}
