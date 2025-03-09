#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/ElasticSketch.hpp"

// caculate the wmre in the step of 
double wmre_step(std::unordered_map<val_tp,val_tp> distall,std::unordered_map<val_tp,val_tp> estdist,int down, int up,int step);

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 3) {
        printf("Usage: %s [Memory(KB)] [Step(1,2,4,8...)]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    int memory = atoi(argv[1]);
    int heavy_memory = memory * 1024 / 4;
    int bucket_num = heavy_memory / 64;
    int counter_num = (memory * 1024 - heavy_memory);
    int step =atoi(argv[2]); // the setting of step s 
    
    std::string file = "../../data/data.pcap";

    Evaluation *eva = new Evaluation();
    double avg_wmre = 0;

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
    std::cout << "[Message] Total packets: " << sum <<", distinct flows: "<<ground.size()<< std::endl;

    std::unordered_map<val_tp,val_tp> distall;// real flow size distribution
    distall.clear();
    uint maxfre=0; // the max frequency
    for(auto it = ground.begin(); it != ground.end(); ++it) {
        maxfre=maxfre<it->second?it->second:maxfre;
        distall[it->second]++; // update real fsd
    }

    ESketch *sketch = new ESketch(bucket_num, counter_num);

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
        uint val = sketch->PointQuery(it->first);
        estdist[val]++; 
        maxfre=maxfre<val?val:maxfre;
        
    }

    // Analysis  Weighted Mean Relative Error (WMRE)  of flow size distribution (item frequency distribution)

    double wmre=wmre_step(distall,estdist,1,maxfre,step);
    avg_wmre += wmre;
    std::cout << std::setw(20) << std::left << "Algorithm"
        << std::setw(20) << std::left << "Memory"
        << std::setw(20) << std::left << "Step"
        << std::setw(20) << std::left << "WMRE"<< std::endl;
    std::cout << std::setw(20) << std::left <<  "Elastic"
        << std::setw(20) << std::left << memory
        << std::setw(20) << std::left << step
        << std::setw(20) << std::left << wmre <<std::endl;
        // << std::setw(20) << std::left << max_qe <<std::endl;

    delete sketch;
    delete eva;
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