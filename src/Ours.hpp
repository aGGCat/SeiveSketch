#ifndef ELASTIC_H
#define ELASTIC_H

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


class ESketch : public SketchBase {
    public:
    int num_k = 0;
    struct Bucket
    {
        key_tp key[COUNTER_PER_BUCKET];
        uint32_t val[COUNTER_PER_BUCKET];
    };
    
    struct ES_type {
        //heavy part
        int bucket_num;
        Bucket *buckets;
        
        //layer 1 in small part
        int filter_num1;
        uint8_t* L1;

        //layer 2 in small part
        int filter_num2;
        uint8_t* L2;

       //middle part
        int counter_num;
        int depth;
        int width;
        uint16_t * counters;

        int ones; // record the number of counter whose value > thre
        uint thre;// value > thre
        double r0;// sampling rate
        uint T1,T2;// T1: Layer1 MAX; T2: Layer2 MAX; 
        double frac; // the propotion of the number of counter > frac
 
        unsigned long * hash;

        //# key bits
        int lgn;
    };

    ESketch(int memory, double h_ratio, double s_ratio, int depth, double frac) {
        es_.bucket_num = memory * h_ratio / (8 * COUNTER_PER_BUCKET);
        es_.buckets = new Bucket[es_.bucket_num]();
        
        //filter setting
        es_.filter_num1 =  memory * s_ratio * 0.65;
        es_.r0 = 1;
        es_.ones = 0;
        es_.thre = 1;

        es_.filter_num2 = memory * s_ratio - es_.filter_num1;
        es_.L1 = new uint8_t[es_.filter_num1];
        es_.L2 = new uint8_t[es_.filter_num2];
        es_.T1 = 15;
        es_.T2 = 241; 
        es_.frac = frac;
        
        //count setting
        es_.counter_num = memory * (1-h_ratio-s_ratio) / 2;
        es_.depth = depth;
        es_.width = es_.counter_num ;
        es_.counters = new uint16_t[es_.counter_num]();

        std::cout<<es_.bucket_num<<" "<<(es_.filter_num1*2)<<" "<<" "<<es_.filter_num2<<" "<<es_.width<<std::endl; 

        es_.lgn = sizeof(key_tp);
        es_.hash = new unsigned long[es_.depth]();
        char name[] = "Our sketch";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        for (int i = 0; i < es_.depth; ++i)
        {
            es_.hash[i] = GenHashSeed(seed++);
        }
    }

    ~ESketch() {
        delete[] es_.hash;
        delete[] es_.counters;
        delete[] es_.buckets;
    }

    int HeavyPart_insert(key_tp key, key_tp &swap_key, uint32_t &swap_val, uint32_t f = 1) {
        int pos = CalculateFP(key);
        uint32_t min_counter_val = GetCounterVal(es_.buckets[pos].val[0]);
        int min_counter = 0;
        int empty = -1;
        for(int i = 0; i < COUNTER_PER_BUCKET - 1; i++){
            if(es_.buckets[pos].key[i] == key){
                es_.buckets[pos].val[i] += f;
                return 0;
            }
            if(es_.buckets[pos].key[i] == 0 && empty == -1)
                empty = i;
            if(min_counter_val > GetCounterVal(es_.buckets[pos].val[i])){
                min_counter = i;
                min_counter_val = GetCounterVal(es_.buckets[pos].val[i]);
            }
        }

        /* if there has empty bucket */
        if(empty != -1){
            es_.buckets[pos].key[empty] = key;
            es_.buckets[pos].val[empty] = f;
            return 0;
        }

        uint32_t guard_val = es_.buckets[pos].val[MAX_VALID_COUNTER];
        guard_val = UPDATE_GUARD_VAL(guard_val);

        if(!JUDGE_IF_SWAP(GetCounterVal(min_counter_val), guard_val)){
            es_.buckets[pos].val[MAX_VALID_COUNTER] = guard_val;
            return 2;
        }

        swap_key = es_.buckets[pos].key[min_counter];
        swap_val = es_.buckets[pos].val[min_counter];

        es_.buckets[pos].val[MAX_VALID_COUNTER] = 0;
        es_.buckets[pos].key[min_counter] = key;
        es_.buckets[pos].val[min_counter] = 0x80000001;
        // k++;
        return 1;
    }

    uint32_t HeavyPart_query(key_tp key) {
        key_tp fp = key;
        int pos = CalculateFP(fp);
        for(int i = 0; i < MAX_VALID_COUNTER; ++i)
            if(es_.buckets[pos].key[i] == fp)
                return es_.buckets[pos].val[i];
        return 0; 
    }

    int LightPart_insert(key_tp key, int weight) {
        uint value[4];
        int index[4] = {0};
        int offset[4] = {0};
        
        uint v1 = UINT32_MAX;
        uint64_t h1= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[0]);
        for(int i = 0; i < es_.depth; i++) {
            index[i] = h1 % es_.filter_num1;
            h1 >>= 4;
            offset[i] = h1 & 1;
            value[i] = (es_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            v1 = std::min(v1,value[i]);
            h1 >>= 4;
        }

        uint tmp = v1 + weight;
        if(tmp <= es_.T1) {
            for (int i = 0; i < es_.depth; i++){
                uint val = (es_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
                if(val < tmp) {
                    es_.L1[index[i]] += (tmp - val) << (offset[i] * 4);
                    if(val == 0) {
                        es_.ones++;
                    }
                        
                }
            }
            return 0;
        }

        for (int i = 0; i < es_.depth; i++)
        {
            uint val = (es_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            if(val == 0)
                es_.ones++;
            es_.L1[index[i]] |= 0xf << (offset[i] * 4);
        }
        

        int delta1 = es_.T1 - v1;
        weight -= delta1;

    //update filter2; flow size <=255
        uint64_t h2= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < es_.depth; i++) {
            index[i] = h2 % es_.filter_num2;
            value[i] = (uint)es_.L2[index[i]];
            v2 = std::min(v2,value[i]);

            h2 >>= 4;
        }
        // if(key == 3585041257)
        //     std::cout<<nn<<" "<<v2<<" "<<weight<<std::endl;
        tmp = v2 + weight;
        if(tmp <= es_.T2) {
            for (int i = 0; i < es_.depth; i++){
                uint val = (uint)es_.L2[index[i]];
                if(val < tmp) {
                    es_.L2[index[i]] += (tmp - val);
                }
            }
            
            return 0;
        }

        for (int i = 0; i < es_.depth; i++)
        {
            es_.L2[index[i]] = es_.T2;
        }

        int delta2 = es_.T2 - v2;
        weight -= delta2;
        if(weight)
            MiddlePart_insert(key,weight);
        return weight;
    }

    int LightPart_insert_withR(key_tp key, int weight) {
        uint value[4];
        int index[4] = {0};
        int offset[4] = {0};

        uint64_t h1= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[0]);
        uint v1 = UINT32_MAX;
        
        for(int i = 0; i < es_.depth; i++) {
            index[i] = h1 % es_.filter_num1;
            h1 >>= 4;
            offset[i] = h1 & 1;
            value[i] = (es_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            v1 = std::min(v1,value[i]);
            h1 >>= 4;
        }

        int tmp_w = weight;
        if(v1 < es_.T1) {
            for (int i = 0; i < weight; i++) {
                double p = rand() * 1.0 / RAND_MAX;
                if(p < es_.r0) {
                    uint temp = v1;
                    if(tmp_w > 1.0/es_.r0) {
                        temp += (tmp_w * es_.r0);
                    }
                    else 
                        temp ++;
                    if(temp <= es_.T1) {
                        for (int i = 0; i < es_.depth; i++)
                        {
                            uint val = (es_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
                            if(val < temp) {
                                if(val == 0)
                                    es_.ones++;
                                es_.L1[index[i]] += (temp - val) << (offset[i] * 4);
                            }
                        }  
                        return 0;
                    }
                    else{
                        int delta1 = es_.T1 - v1;
                        tmp_w -= delta1 / es_.r0;
                        break;
                    }
                }
                else
                    tmp_w--;
            }
            if(tmp_w == 0)
                return 0;
        }

        for (int i = 0; i < es_.depth; i++)
        {
            uint val = (es_.L1[index[i]] >> (offset[i] * 4)) & 0xf;
            if(val == 0)
                es_.ones++;
            es_.L1[index[i]] |= 0xf << (offset[i] * 4);
        }

        // int delta1 = es_.T1 - v1;
        // weight -= delta1 / es_.r0;
        weight = tmp_w;

        uint64_t h2= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < es_.depth; i++) {
            index[i] = h2 % es_.filter_num2;
            value[i] = (uint)es_.L2[index[i]];
            v2 = std::min(v2,value[i]);

            h2 >>= 4;
        }
        uint tmp = v2 + weight;
        if(tmp <= es_.T2) {
            for (int i = 0; i < es_.depth; i++){
                uint val = (uint)es_.L2[index[i]];
                if(val < tmp) {
                    es_.L2[index[i]] += (tmp - val);
                }
            }
            
            return 0;
        }

        if(tmp > es_.T2 && v2 < es_.T2) {
            for (int i = 0; i < es_.depth; i++){
                es_.L2[index[i]] = es_.T2;
            }
        }
        int delta2 = es_.T2 - v2;
        weight -= delta2;
        if(weight)
            MiddlePart_insert(key,weight);
        return weight;
    }

	uint32_t LightPart_query(key_tp key) {
        //query filter1 
        uint v1 = UINT32_MAX;
        uint64_t h1= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[0]);
        for(int i = 0; i < es_.depth; i++) {
            uint index = h1 % es_.filter_num1;
            h1 >>= 4;
            uint offset = h1 & 1;
            uint value = (es_.L1[index] >> (offset * 4)) & 0xf;
            v1 = std::min(v1,value);

            h1 >>= 4;
        }
        if(v1 < es_.T1)
            return v1;
    //query filter2; flow size <=255
        uint64_t h2= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < es_.depth; i++) {
            uint index = h2 % es_.filter_num2;
            uint value = (uint)es_.L2[index];
            v2 = std::min(v2,value);

            h2 >>= 4;
        }

        if(v2 < es_.T2)
            return (v1 + v2);
        
        uint v3 = MiddlePart_query(key);
        return (v1 + v2 + v3);
    }

	uint32_t LightPart_query_withR(key_tp key) {
        uint64_t h1= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[0]);
        uint v1 = UINT32_MAX;
        for(int i = 0; i < es_.depth; i++) {
            uint index = h1 % es_.filter_num1;
            h1 >>= 4;
            uint offset = h1 & 1;
            uint value = (es_.L1[index] >> (offset * 4)) & 0xf;
            v1 = std::min(v1,value);
            h1 >>= 4;
        }

        // int num = es_.r0;
        uint est = v1*1.0/es_.r0;
        // if(key == 1596342368)
        //     std::cout<<est<<std::endl;
        if(v1 < es_.T1)
            return est;


    //query filter2; flow size <=255
        uint64_t h2= MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < es_.depth; i++) {
            uint index = h2 % es_.filter_num2;
            uint value = (uint)es_.L2[index];
            v2 = std::min(v2,value);

            h2 >>= 4;
        }
        
        if(v2 < es_.T2)
            return (est + v2);
        uint v3 = MiddlePart_query(key);
        return (est + v2 + v3);
    }

    void MiddlePart_insert(key_tp key, int weight) {
        uint64_t hash = MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[0]);
        uint pos = hash % es_.width;
        if( (es_.counters[pos]+weight) <= UINT16_MAX)
            es_.counters[pos] += weight;
        else
            es_.counters[pos] = UINT16_MAX;
    }

	uint32_t MiddlePart_query(key_tp key) {
        uint64_t hash = MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash[0]);
        uint pos = hash % es_.width;
        return es_.counters[pos];
    }

    void Judge_Rate() {
        while((es_.ones * 1.0 / (2 * es_.filter_num1)) >= es_.frac) { 
            int avg = 0;
            es_.ones = 0;
            for (int i = 0; i < es_.filter_num1; i++)
            {
                uint val1 = (es_.L1[i] >> 4) & 0xf;
                if(val1 != 0)
                    val1 = change_val(val1,avg);

                uint val2 = es_.L1[i] & 0xf;
                if(val2 != 0)
                    val2 = change_val(val2,avg);
                
                es_.L1[i] = ((val1 << 4) | val2);
            }
           
            es_.r0 /= 2;
            std::cout<<num_k<<std::endl;
            num_k = 0;
        }
    }

    void Update(key_tp key, val_tp weight) {
        key_tp swap_key;
        uint32_t swap_val = 0;
        int result = HeavyPart_insert(key, swap_key, swap_val, weight);
        switch(result)
        {
            case 0: return;
            case 1:{
                num_k+= GetCounterVal(swap_val);
                if(es_.r0 == 1) {
                    LightPart_insert(swap_key, GetCounterVal(swap_val));
                }
                else {
                    LightPart_insert_withR(swap_key, GetCounterVal(swap_val));
                }
                Judge_Rate();
                return;
            }
            case 2:  {
                num_k++;
                if(es_.r0 == 1) {
                    LightPart_insert(key, 1);
                }
                else {
                    LightPart_insert_withR(key, 1);
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
        for(int i = 0; i < COUNTER_PER_BUCKET - 1; i++){
            if(es_.buckets[pos].key[i] == key){
                int res = es_.buckets[pos].val[i];
                if(HIGHEST_BIT_IS_1(res)) {
                    if(es_.r0 == 1)
                        res = (int)GetCounterVal(res) + LightPart_query(key);
                    else
                        res = (int)GetCounterVal(res) + LightPart_query_withR(key);
                }

                return res;
            }
        }

        uint result = 0;
        if(es_.r0 == 1.0) {
            result = LightPart_query(key);
        }
        else {
            result = LightPart_query_withR(key);
        }
        
        if(result == 0)
            result = 1;

        return result;
    }

    void Query(val_tp thresh, myvector &results) {
        for (int i = 0; i < es_.bucket_num; ++i) {
            for (int j = 0; j < MAX_VALID_COUNTER; ++j) 
            {
                key_tp key = es_.buckets[i].key[j];
                uint res = es_.buckets[i].val[j];
                if(HIGHEST_BIT_IS_1(res)) {
                    if(es_.r0 == 1)
                        res = (int)GetCounterVal(res) + LightPart_query(key);
                    else
                        res = (int)GetCounterVal(res) + LightPart_query_withR(key);
                }
                if (res > thresh) {
                    results.push_back(std::make_pair(key, res));
                }
            }
        }
    }

    void Reset() {
        for(int i=0;i<es_.filter_num1;i++)
            es_.L1[i] = 0;
    
        for(int i=0;i<es_.filter_num2;i++)
            es_.L2[i] = 0;
        for(int i=0;i<es_.bucket_num;i++) {
            for(int j=0;j<COUNTER_PER_BUCKET;j++) {
                es_.buckets[i].key[j]=0;
                es_.buckets[i].val[j]=0;
            }
        }
        for(int i=0;i<es_.counter_num;i++)
            es_.counters[i] = 0;
        for (int i = 0; i < es_.depth; i++)
        {
            es_.hash[i]=0;
        }
    }

    uint change_val(uint val, double avg) {
        uint res = 0;
        double rate = 2;
        if(val < es_.T1) {
            res = val / rate;
        }
        else {
            uint est = val / rate;
            res = std::min(est, (uint)es_.T1); 
        }
        if(res >= es_.thre)
            es_.ones++;
    
        return res;
    }

    double GetBias(int val) {
        int sum_15 = 0, sum = 0;//, num_1 = 0;
        for(int i = 0; i < es_.filter_num1; i++) {
            int value = (es_.L1[i] >> 4) & 0xf;
            if(value >= val) {
                sum_15++;  
            }

            value = es_.L1[i] & 0xf;
            if(value >= val) {
                sum_15++;
            }
                
            sum+=2;
        }
        std::cout<<sum_15<<" "<<sum<<" "<<sum_15*1.0/sum<<" "<<es_.ones<<" "<<es_.thre<<" "<<1.0/es_.r0<<std::endl;

        return (1.0/es_.r0);
    }

private:
    int CalculateFP(key_tp fp) {
        return CalculateBucketPos(fp) % es_.bucket_num;
    }

    //Sketch data structure
    ES_type es_;
    
};

#endif