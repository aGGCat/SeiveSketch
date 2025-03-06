#include "../../lib/inputadaptor.hpp"
#include "../../lib/evaluation.hpp"
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include "../../src/LaF_CU.hpp"

int cdf_range=10;//cdf_re in [0,0.1,...,0.9,1.0] 
int judge_cdf(double re);

int main(int argc,char* argv[]) {
    // Configure parameters
    if (argc < 2) {
        printf("Usage: %s [Memory(KB)]\n", argv[0]);
        return 0;
    }

    unsigned long long buf_size = 1000000000;
    int memory = atoi(argv[1]);
    int meminkb = memory;
    int percent = 10;
    int thres1 = 18;
    int thres2 = 2;
    
    std::string file = "../../data/data.pcap";

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
    std::cout << "[Message] Total packets: " << sum <<", distinct flows: "<<ground.size()<< std::endl;
    
    CUSketchWithSF *sketch = new CUSketchWithSF(meminkb * 1024, percent,(meminkb * 1024*0.01*percent / (8 * 3)) * 0.99, std::ceil((meminkb * 1024*0.01*percent  / (8 * 3)) * 0.01),
                                        8, 8,
                                        thres1, thres2);
    sketch->Reset();

    // Update sketch
    std::cout<<"updating"<<std::endl;
    adaptor->Reset();
    while(adaptor->GetNext(&t) == 1) {
        // if(ground[t.src_ip]<threshold)
        // key_tp key = ((uint64_t)t.src_ip << 32) | t.dst_ip;
        key_tp key = t.src_ip;
        sketch->insert(key);
    }
    

    // init cdf array
    uint64_t cdf[cdf_range+1];
    // cdf[i] indicates the number of items whose relative error<= i*0.1 
    // cdf_range=10 indicates cdf_re from 0 to cdf_range*0.1
    memset(cdf,0,(cdf_range+1)*sizeof(uint64_t));

    for(auto it = ground.begin(); it != ground.end(); ++it)
    {
        int val = sketch->query(it->first);
        double re=1.0*abs((long)it->second - (long)val)/it->second;    
        int pos=judge_cdf(re);
        cdf[pos]++;
    }

    // Analysis  Cumulative distribution function of relative error (CDF_RE)

    std::cout << std::setw(20) << std::left << "Algorithm"
        << std::setw(20) << std::left << "Memory"
        << std::setw(20) << std::left << "Relative Error"
        << std::setw(20) << std::left << "CDF"<< std::endl;

    uint64_t cdf_sum=0;
    // std::ofstream txtfile;
    // txtfile.open("Relative_error_cdf.txt",std::ios::app);
    for(int i=0;i<=cdf_range;i++)
    {   
        cdf_sum+=cdf[i];
        std::cout << std::setw(20) << std::left <<  "Ladder"
        << std::setw(20) << std::left << memory
        << std::setw(20) << std::left << i*0.1
        << std::setw(20) << std::left <<1.0*cdf_sum/ground.size() <<std::endl;
        
    }
    delete sketch;
    delete eva;
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