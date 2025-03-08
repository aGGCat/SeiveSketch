#ifndef SEIVE_H
#define SEIVE_H

#include <utility>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "../lib/datatypes.hpp"
#include "../lib/param.h"
#include "sketch_base.hpp"
#include <set>


extern "C"
{
#include "../lib/hash.h"
}


class SeiveSketch : public SketchBase {
    public:
    struct Bucket
    {
        key_tp key[COUNTER_PER_BUCKET];
        uint32_t val[COUNTER_PER_BUCKET];
    };
    
    struct SS_type {
        //hot part
        int bucket_num;
        Bucket *buckets;
        uint32_t *v_s;
        
        //layer 1 in cold part
        int filter_num1;
        uint8_t* L1;

        //layer 2 in cold part
        int filter_num2;
        uint8_t* L2;

        //warm part
        int counter_num;
        int depth;
        int width;
        uint16_t * counters;

        int ones; // record the number of counter whose value > thre
        uint thre;// value > thre
        double r0;// sampling rate
        uint T1,T2;// T1: Layer1 Threshold; T2: Layer2 Threshold; 
        double frac; // the propotion of the number of counter exceeding thre > frac
 
        unsigned long * hash;

        //# key bits
        int lgn;
    };

    SeiveSketch(int memory, double h_ratio, double s_ratio, int depth, double frac) {
        //setting parameters of Hot Part
        ss_.bucket_num = memory * h_ratio / (8 * COUNTER_PER_BUCKET + 4);
        ss_.buckets = new Bucket[ss_.bucket_num]();
        ss_.v_s = new uint32_t[ss_.bucket_num]();
        
        //setting parameters of Cold Part
        ss_.filter_num1 =  memory * s_ratio * 0.65;
        ss_.r0 = 1;
        ss_.ones = 0;
        ss_.thre = 1;

        ss_.filter_num2 = memory * s_ratio - ss_.filter_num1;
        ss_.L1 = new uint8_t[ss_.filter_num1];
        ss_.L2 = new uint8_t[ss_.filter_num2];
        ss_.T1 = 15;
        ss_.T2 = 241; 
        ss_.frac = frac;
        
        //setting parameters of Warm Part
        ss_.counter_num = memory * (1-h_ratio-s_ratio) / 2;
        ss_.depth = depth;
        ss_.width = ss_.counter_num ;
        ss_.counters = new uint16_t[ss_.counter_num]();

        // std::cout<<ss_.bucket_num<<" "<<(ss_.filter_num1*2)<<" "<<" "<<ss_.filter_num2<<" "<<ss_.width<<std::endl; 

        ss_.lgn = sizeof(key_tp);
        ss_.hash = new unsigned long[ss_.depth]();
        char name[] = "Our sketch";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        for (int i = 0; i < ss_.depth; ++i)
        {
            ss_.hash[i] = GenHashSeed(seed++);
        }
    }

    ~SeiveSketch() {
        delete[] ss_.hash;
        delete[] ss_.counters;
        delete[] ss_.buckets;
    }

    int HotPart_insert(key_tp key, key_tp &swap_key, uint32_t &swap_val, uint32_t f = 1) {
        int pos = CalculateFP(key);
        uint32_t min_counter_val = GetCounterVal(ss_.buckets[pos].val[0]);
        int min_counter = 0;
        int empty = -1;
        for(int i = 0; i < COUNTER_PER_BUCKET; i++){
            if(ss_.buckets[pos].key[i] == key){
                ss_.buckets[pos].val[i] += f;
                return 0;
            }
            if(ss_.buckets[pos].key[i] == 0 && empty == -1)
                empty = i;
            if(min_counter_val > GetCounterVal(ss_.buckets[pos].val[i])){
                min_counter = i;
                min_counter_val = GetCounterVal(ss_.buckets[pos].val[i]);
            }
        }

        /* if there has empty bucket */
        if(empty != -1){
            ss_.buckets[pos].key[empty] = key;
            ss_.buckets[pos].val[empty] = f;
            return 0;
        }

        uint32_t guard_val = ss_.v_s[pos];
        guard_val = UPDATE_GUARD_VAL(guard_val);

        if(!JUDGE_IF_SWAP(GetCounterVal(min_counter_val), guard_val)){
            ss_.v_s[pos] = guard_val;
            return 2;
        }

        swap_key = ss_.buckets[pos].key[min_counter];
        swap_val = ss_.buckets[pos].val[min_counter];

        ss_.v_s[pos] = 0;
        ss_.buckets[pos].key[min_counter] = key;
        ss_.buckets[pos].val[min_counter] = 0x80000001;
        return 1;
    }

    uint32_t HotPart_query(key_tp key) {
        key_tp fp = key;
        int pos = CalculateFP(fp);
        for(int i = 0; i < MAX_VALID_COUNTER; ++i)
            if(ss_.buckets[pos].key[i] == fp)
                return ss_.buckets[pos].val[i];
        return 0; 
    }

    int ColdPart_insert(key_tp key, int weight) {
        uint value[4];
        int index[4] = {0};
        int offset[4] = {0};
        
        uint v1 = UINT32_MAX;
        uint64_t h1= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[0]);
        for(int i = 0; i < ss_.depth; i++) {
            index[i] = h1 % ss_.filter_num1;
            h1 >>= 4;
            offset[i] = h1 & 1;
            value[i] = (ss_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            v1 = std::min(v1,value[i]);
            h1 >>= 4;
        }

        uint tmp = v1 + weight;
        if(tmp <= ss_.T1) {
            for (int i = 0; i < ss_.depth; i++){
                uint val = (ss_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
                if(val < tmp) {
                    ss_.L1[index[i]] += (tmp - val) << (offset[i] * 4);
                    if(val == 0) {
                        ss_.ones++;
                    }
                        
                }
            }
            return 0;
        }

        for (int i = 0; i < ss_.depth; i++)
        {
            uint val = (ss_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            if(val == 0)
                ss_.ones++;
            ss_.L1[index[i]] |= 0xf << (offset[i] * 4);
        }
        

        int delta1 = ss_.T1 - v1;
        weight -= delta1;

    //update filter2; flow size <=255
        uint64_t h2= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            index[i] = h2 % ss_.filter_num2;
            value[i] = (uint)ss_.L2[index[i]];
            v2 = std::min(v2,value[i]);

            h2 >>= 4;
        }
        // if(key == 3585041257)
        //     std::cout<<nn<<" "<<v2<<" "<<weight<<std::endl;
        tmp = v2 + weight;
        if(tmp <= ss_.T2) {
            for (int i = 0; i < ss_.depth; i++){
                uint val = (uint)ss_.L2[index[i]];
                if(val < tmp) {
                    ss_.L2[index[i]] += (tmp - val);
                }
            }
            
            return 0;
        }

        for (int i = 0; i < ss_.depth; i++)
        {
            ss_.L2[index[i]] = ss_.T2;
        }

        int delta2 = ss_.T2 - v2;
        weight -= delta2;
        if(weight)
            WarmPart_insert(key,weight);
        return weight;
    }

    int ColdPart_insert_withR(key_tp key, int weight) {
        uint value[4];
        int index[4] = {0};
        int offset[4] = {0};

        uint64_t h1= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[0]);
        uint v1 = UINT32_MAX;
        
        for(int i = 0; i < ss_.depth; i++) {
            index[i] = h1 % ss_.filter_num1;
            h1 >>= 4;
            offset[i] = h1 & 1;
            value[i] = (ss_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            v1 = std::min(v1,value[i]);
            h1 >>= 4;
        }

        int tmp_w = weight;
        if(v1 < ss_.T1) {
            for (int i = 0; i < weight; i++) {
                double p = rand() * 1.0 / RAND_MAX;
                if(p < ss_.r0) {
                    uint temp = v1;
                    if(tmp_w > 1.0/ss_.r0) {
                        temp += (tmp_w * ss_.r0);
                    }
                    else 
                        temp ++;
                    if(temp <= ss_.T1) {
                        for (int i = 0; i < ss_.depth; i++)
                        {
                            uint val = (ss_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
                            if(val < temp) {
                                if(val == 0)
                                    ss_.ones++;
                                ss_.L1[index[i]] += (temp - val) << (offset[i] * 4);
                            }
                        }  
                        return 0;
                    }
                    else{
                        int delta1 = ss_.T1 - v1;
                        tmp_w -= delta1 / ss_.r0;
                        break;
                    }
                }
                else
                    tmp_w--;
            }
            if(tmp_w == 0)
                return 0;
        }

        for (int i = 0; i < ss_.depth; i++)
        {
            uint val = (ss_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            if(val == 0)
                ss_.ones++;
            ss_.L1[index[i]] |= 0xf << (offset[i] * 4);
        }

        // int delta1 = ss_.T1 - v1;
        // weight -= delta1 / ss_.r0;
        weight = tmp_w;

        uint64_t h2= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            index[i] = h2 % ss_.filter_num2;
            value[i] = (uint)ss_.L2[index[i]];
            v2 = std::min(v2,value[i]);

            h2 >>= 4;
        }
        uint tmp = v2 + weight;
        if(tmp <= ss_.T2) {
            for (int i = 0; i < ss_.depth; i++){
                uint val = (uint)ss_.L2[index[i]];
                if(val < tmp) {
                    ss_.L2[index[i]] += (tmp - val);
                }
            }
            
            return 0;
        }

        if(tmp > ss_.T2 && v2 < ss_.T2) {
            for (int i = 0; i < ss_.depth; i++){
                ss_.L2[index[i]] = ss_.T2;
            }
        }
        int delta2 = ss_.T2 - v2;
        weight -= delta2;
        if(weight)
            WarmPart_insert(key,weight);
        return weight;
    }

	uint32_t ColdPart_query(key_tp key) {
        //query filter1 
        uint v1 = UINT32_MAX;
        uint64_t h1= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[0]);
        for(int i = 0; i < ss_.depth; i++) {
            uint index = h1 % ss_.filter_num1;
            h1 >>= 4;
            uint offset = h1 & 1;
            uint value = (ss_.L1[index] >> (offset * 4)) & 0xf;
            v1 = std::min(v1,value);

            h1 >>= 4;
        }
        if(v1 < ss_.T1)
            return v1;
    //query filter2; flow size <=255
        uint64_t h2= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            uint index = h2 % ss_.filter_num2;
            uint value = (uint)ss_.L2[index];
            v2 = std::min(v2,value);

            h2 >>= 4;
        }

        if(v2 < ss_.T2)
            return (v1 + v2);
        
        uint v3 = WarmPart_query(key);
        return (v1 + v2 + v3);
    }

	uint32_t ColdPart_query_withR(key_tp key) {
        uint64_t h1= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[0]);
        uint v1 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            uint index = h1 % ss_.filter_num1;
            h1 >>= 4;
            uint offset = h1 & 1;
            uint value = (ss_.L1[index] >> (offset * 4)) & 0xf;
            v1 = std::min(v1,value);
            h1 >>= 4;
        }

        uint est = v1*1.0/ss_.r0;
        if(v1 < ss_.T1)
            return est;


    //query filter2; flow size <=255
        uint64_t h2= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            uint index = h2 % ss_.filter_num2;
            uint value = (uint)ss_.L2[index];
            v2 = std::min(v2,value);

            h2 >>= 4;
        }
        
        if(v2 < ss_.T2)
            return (est + v2);
        uint v3 = WarmPart_query(key);
        return (est + v2 + v3);
    }

    void WarmPart_insert(key_tp key, int weight) {
        uint64_t hash = MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[0]);
        uint pos = hash % ss_.width;
        if( (ss_.counters[pos]+weight) <= UINT16_MAX)
            ss_.counters[pos] += weight;
        else
            ss_.counters[pos] = UINT16_MAX;
    }

	uint32_t WarmPart_query(key_tp key) {
        uint64_t hash = MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[0]);
        uint pos = hash % ss_.width;
        return ss_.counters[pos];
    }

    void Judge_Rate() {
        while((ss_.ones * 1.0 / (2 * ss_.filter_num1)) >= ss_.frac) { 
            int avg = 0;
            ss_.ones = 0;
            for (int i = 0; i < ss_.filter_num1; i++)
            {
                uint val1 = (ss_.L1[i] >> 4) & 0xf;
                if(val1 != 0)
                    val1 = change_val(val1,avg);

                uint val2 = ss_.L1[i] & 0xf;
                if(val2 != 0)
                    val2 = change_val(val2,avg);
                
                ss_.L1[i] = ((val1 << 4) | val2);
            }
           
            ss_.r0 /= 2;
        }
    }

    void Update(key_tp key, val_tp weight) {
        key_tp swap_key;
        uint32_t swap_val = 0;
        int result = HotPart_insert(key, swap_key, swap_val, weight);
        switch(result)
        {
            case 0: return;
            case 1:{
                if(ss_.r0 == 1) {
                    ColdPart_insert(swap_key, GetCounterVal(swap_val));
                }
                else {
                    ColdPart_insert_withR(swap_key, GetCounterVal(swap_val));
                }
                Judge_Rate();
                return;
            }
            case 2:  {
                if(ss_.r0 == 1) {
                    ColdPart_insert(key, 1);
                }
                else {
                    ColdPart_insert_withR(key, 1);
                }
                Judge_Rate();
                return;
            }
            default:
                printf("error return value !\n");
                exit(1);
        }
    }

    val_tp PointQuery(key_tp key) {
        int pos = CalculateFP(key);
        for(int i = 0; i < COUNTER_PER_BUCKET; i++){
            if(ss_.buckets[pos].key[i] == key){
                int res = ss_.buckets[pos].val[i];
                if(HIGHEST_BIT_IS_1(res)) {
                    if(ss_.r0 == 1)
                        res = (int)GetCounterVal(res) + ColdPart_query(key);
                    else
                        res = (int)GetCounterVal(res) + ColdPart_query_withR(key);
                }

                return res;
            }
        }

        uint result = 0;
        if(ss_.r0 == 1.0) {
            result = ColdPart_query(key);
        }
        else {
            result = ColdPart_query_withR(key);
        }
        
        if(result == 0)
            result = 1;

        return result;
    }

    void Query(val_tp thresh, myvector &results) {
        for (int i = 0; i < ss_.bucket_num; ++i) {
            for (int j = 0; j < MAX_VALID_COUNTER; ++j) 
            {
                key_tp key = ss_.buckets[i].key[j];
                uint res = ss_.buckets[i].val[j];
                if(HIGHEST_BIT_IS_1(res)) {
                    if(ss_.r0 == 1)
                        res = (int)GetCounterVal(res) + ColdPart_query(key);
                    else
                        res = (int)GetCounterVal(res) + ColdPart_query_withR(key);
                }
                if (res > thresh) {
                    results.push_back(std::make_pair(key, res));
                }
            }
        }
    }

    void Reset() {
        for(int i=0;i<ss_.filter_num1;i++)
            ss_.L1[i] = 0;
    
        for(int i=0;i<ss_.filter_num2;i++)
            ss_.L2[i] = 0;
        for(int i=0;i<ss_.bucket_num;i++) {
            for(int j=0;j<COUNTER_PER_BUCKET;j++) {
                ss_.buckets[i].key[j]=0;
                ss_.buckets[i].val[j]=0;
            }
        }
        for(int i=0;i<ss_.counter_num;i++)
            ss_.counters[i] = 0;
        for (int i = 0; i < ss_.depth; i++)
        {
            ss_.hash[i]=0;
        }
    }

    uint change_val(uint val, double avg) {
        uint res = val / 2;
        if(res >= ss_.thre)
            ss_.ones++;
    
        return res;
    }

    double GetBias(int val) {
        int sum_15 = 0, sum = 0;//, num_1 = 0;
        for(int i = 0; i < ss_.filter_num1; i++) {
            int value = (ss_.L1[i] >> 4) & 0xf;
            if(value >= val) {
                sum_15++;  
            }

            value = ss_.L1[i] & 0xf;
            if(value >= val) {
                sum_15++;
            }
                
            sum+=2;
        }
        std::cout<<sum_15<<" "<<sum<<" "<<sum_15*1.0/sum<<" "<<ss_.ones<<" "<<ss_.thre<<" "<<1.0/ss_.r0<<std::endl;

        return (1.0/ss_.r0);
    }

private:
    int CalculateFP(key_tp fp) {
        return CalculateBucketPos(fp) % ss_.bucket_num;
    }

    //Sketch data structure
    SS_type ss_;
    
};

#endif
