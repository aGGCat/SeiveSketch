#include "lib/inputadaptor.hpp"
#include "lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "src/Ours.hpp"

// caculate the wmre in the step of 
double wmre_step(std::unordered_map<val_tp,val_tp> distall,std::unordered_map<val_tp,val_tp> estdist,int down, int up,int step);

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 4) {
        printf("Usage: %s [Memory(KB)] [Trace]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    int memory = atoi(argv[1]);
    int threshold = atoi(argv[2]);
    double h_ratio = 0.3;
    double s_ratio = 0.6;
    int depth = 3;//atoi(argv[5]);
    double frac = 0.6321;
    int trace = atoi(argv[3]);
    int step =atoi(argv[4]); // the setting of step s 
    
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
        
        myvector truth;

        std::unordered_map<val_tp,val_tp> distall;// real flow size distribution
        distall.clear();
        int maxfre=0; // the max frequency
        for(auto it = ground.begin(); it != ground.end(); ++it) {
            if(it->second > threshold) {
                std::pair<key_tp, val_tp> node;
                node.first = it->first;
                node.second = it->second;
                truth.push_back(node);
                maxfre=maxfre<it->second?it->second:maxfre;
            }
            distall[it->second]++; // update real fsd
            
        }
        std::cout << "[Message] True HHs: " << truth.size() << std::endl;


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
        

        
        std::unordered_map<val_tp,val_tp> estdist;// estimated flow size distribution
        estdist.clear();
        for(auto it = ground.begin(); it != ground.end(); ++it)
        {
            int val = sketch->PointQuery(it->first);
            double re=1.0*abs((long)it->second - (long)val)/it->second;    
            estdist[val]++; 
            maxfre=maxfre<val?val:maxfre;
            
        }

        // Analysis  Weighted Mean Relative Error (WMRE)  of flow size distribution (item frequency distribution)

        double wmre=wmre_step(distall,estdist,1,maxfre,step);

        std::cout << std::setw(20) << std::left << "Algorithm"
            << std::setw(20) << std::left << "Memory"
            << std::setw(20) << std::left << "Trace"
            << std::setw(20) << std::left << "Step"
            << std::setw(20) << std::left << "WMRE"<< std::endl;
        std::cout << std::setw(20) << std::left <<  "Our"
            << std::setw(20) << std::left << memory
            << std::setw(20) << std::left << trace
            << std::setw(20) << std::left << step
            << std::setw(20) << std::left << wmre<<std::endl;

        delete sketch;

    }
    delete eva;
    tracefiles.close();
    return 0;

}

double wmre_step(std::unordered_map<val_tp,val_tp> distall,std::unordered_map<val_tp,val_tp> estdist,int down, int up,int step){
    uint64_t sum1=0;
    double sum2=0;
    for(int i=down;i<=up;i++) 
    {   
        uint64_t dist=0,est=0;
        int j=i;
        for(;j<i+step&& j<=up;j++)
        {
            dist+=distall[j];
            est+=estdist[j];
        }
        sum1+=abs((long)dist- (long)(est));
        sum2+=1.0*((long)dist+(long)est)/2.0;
        i=j-1;
    }
    // std::cout<<"sum1 = "<<sum1<<"\t, sum2="<<sum2<<"\t, WMRE= "<<1.0*sum1/sum2<<std::endl;
    return 1.0*sum1/sum2;
}
