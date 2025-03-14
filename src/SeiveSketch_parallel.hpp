#ifndef SEIVE_H
#define SEIVE_H

#include <utility>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "../lib/datatypes.hpp"
#include "../lib/param.h"
#include "../lib/concurrentqueue.h"
#include "sketch_base.hpp"
#include <set>
#include <immintrin.h>
#include <emmintrin.h>
#include <smmintrin.h>
#include <omp.h>
#include <thread>
#include <pthread.h>
#include <atomic>
extern "C"
{
#include "../lib/hash.h"
}

using Data = std::pair<key_tp, val_tp>;


class SeiveSketch : public SketchBase {
    public:
    struct Bucket
    {
        key_tp key[COUNTERS_PER_BUCKET];
        uint32_t val[COUNTERS_PER_BUCKET];
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
        // each array contain (COUNTERS_PER_BUCKET+1) buckets, the last bucket for negative vote
        ss_.bucket_num = memory * h_ratio / (8 * COUNTERS_PER_BUCKET + 4);
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
        
        Finish = 0;
        t2 = std::thread(&SeiveSketch::ColdPart, this);
        t3 = std::thread(&SeiveSketch::WarmPart, this);
        set_thread_affinity(t2, 2); // ColdPart binds CPU 2
        set_thread_affinity(t3, 3); // WarmPart binds CPU 3

        // std::cout<<ss_.bucket_num<<" "<<(ss_.filter_num1*2)<<" "<<" "<<ss_.filter_num2<<" "<<ss_.width<<std::endl; 

        ss_.lgn = sizeof(key_tp);
        ss_.hash = new unsigned long[ss_.depth*3]();
        char name[] = "Our sketch";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        for (int i = 0; i < ss_.depth*3; ++i)
        {
            ss_.hash[i] = GenHashSeed(seed++);
        }
    }

    ~SeiveSketch() {
        delete[] ss_.hash;
        delete[] ss_.counters;
        delete[] ss_.buckets;
        Finish = 1;
        if (t2.joinable()) t2.join();
        if (t3.joinable()) t3.join();
    }

    int HotPart_insert(key_tp key, key_tp &swap_key, uint32_t &swap_val, uint32_t f = 1)
    {
        // std::lock_guard<std::mutex> lock(hot_mutex);
        int pos = CalculateFP(key);
        uint32_t min_counter_val;
        int min_counter;
        
        /* find if there has matched bucket */
        const __m256i item = _mm256_set1_epi32(key);
        __m256i *keys_p = (__m256i *)(ss_.buckets[pos].key);
        int matched = 0;

        __m256i a_comp = _mm256_cmpeq_epi32(item, keys_p[0]);
        matched = _mm256_movemask_ps((__m256)a_comp);

        /* if matched */
        if (matched != 0){
            int matched_index = _tzcnt_u32((uint32_t)matched);
            ss_.buckets[pos].val[matched_index] += f;
            return 0;
        }
        
        /* find the minimal bucket */
        const uint32_t mask_base = 0x7FFFFFFF;
        const __m256i *counters = (__m256i *)(ss_.buckets[pos].val);
        __m256 masks = (__m256)_mm256_set1_epi32(mask_base);
        __m256 results = (_mm256_and_ps(*(__m256*)counters, masks));
        __m256 mask2 = (__m256)_mm256_set_epi32(mask_base, 0, 0, 0, 0, 0, 0, 0);
        results = _mm256_or_ps(results, mask2);

        __m128i low_part = _mm_castps_si128(_mm256_extractf128_ps(results, 0));
        __m128i high_part = _mm_castps_si128(_mm256_extractf128_ps(results, 1));

        __m128i x = _mm_min_epi32(low_part, high_part);
        __m128i min1 = _mm_shuffle_epi32(x, _MM_SHUFFLE(0,0,3,2));
        __m128i min2 = _mm_min_epi32(x,min1);
        __m128i min3 = _mm_shuffle_epi32(min2, _MM_SHUFFLE(0,0,0,1));
        __m128i min4 = _mm_min_epi32(min2,min3);
        min_counter_val = _mm_cvtsi128_si32(min4);

        const __m256i ct_item = _mm256_set1_epi32(min_counter_val);

        __m256i ct_a_comp = _mm256_cmpeq_epi32(ct_item, (__m256i)results);
        matched = _mm256_movemask_ps((__m256)ct_a_comp);
        min_counter = _tzcnt_u32((uint32_t)matched);

        /* if there has empty bucket */
        if(min_counter_val == 0){		// empty counter
            ss_.buckets[pos].key[min_counter] = key;
            ss_.buckets[pos].val[min_counter] = f;
            return 0;
        }

        /* update guard val and comparison */
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
        int index[4] = {0};

        uint32_t onehash[4];
        MurmurHash3_x64_128((unsigned char*)&key, 4, ss_.hash[0],onehash);
        for(int i = 0; i < 4; i++) {
            index[i] = onehash[i] % ss_.filter_num1;
        }

        __m256i vhash = _mm256_set_epi32(0,onehash[0],0,onehash[1],0,onehash[2],0,onehash[3]);
        __m256i offset_vec = _mm256_srli_epi64(vhash, 4);

        union {__m256i voffset; unsigned long offset[4];};
        voffset = _mm256_and_si256(offset_vec, _mm256_set1_epi64x(1));

        __m256i offset_shift = _mm256_slli_epi64(voffset, 2);

        __m256i l1_data = _mm256_set_epi32(0, ss_.L1[index[0]], 0, ss_.L1[index[1]],0, ss_.L1[index[2]], 0, ss_.L1[index[3]]);
        __m256i shifted_data = _mm256_srlv_epi32(l1_data, offset_shift);

        union {__m256i value_vec; unsigned long value[4];};
        value_vec = _mm256_and_si256(shifted_data, _mm256_set1_epi64x(15));

        uint v1 = std::min({value[1],value[2],value[3]});

        uint tmp = v1 + weight;
        if(tmp <= ss_.T1) {
            for (int i = 0; i < ss_.depth; i++){
                uint val = (ss_.L1[index[i]] >> (offset[ss_.depth-i] * 4)) & 0xf;
                if(val < tmp) {
                    ss_.L1[index[i]] += (tmp - val) << (offset[ss_.depth-i] * 4);
                    if(val == 0) {
                        ss_.ones++;
                    }
                        
                }
            }
            return 0;
        }

        for (int i = 0; i < ss_.depth; i++)
        {
            uint val = (ss_.L1[index[i]] >> (offset[ss_.depth-i] * 4)) & 0xf;
            if(val == 0)
                ss_.ones++;
            ss_.L1[index[i]] |= 0xf << (offset[ss_.depth-i] * 4);
        }
        

        int delta1 = ss_.T1 - v1;
        weight -= delta1;

    //update filter2; flow size <=255
        uint64_t h2= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            index[i] = h2 % ss_.filter_num2;
            value[i] = (uint)ss_.L2[index[i]];
            v2 = std::min(v2,(uint)value[i]);

            h2 >>= 4;
        }
        
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
        int index[4] = {0};

        uint32_t onehash[4];
        MurmurHash3_x64_128((unsigned char*)&key, 4, ss_.hash[0],onehash);
        for(int i = 0; i < 4; i++) {
            index[i] = onehash[i] % ss_.filter_num1;
        }

        __m256i vhash = _mm256_set_epi32(0,onehash[0],0,onehash[1],0,onehash[2],0,onehash[3]);
        __m256i offset_vec = _mm256_srli_epi64(vhash, 4);

        union {__m256i voffset; unsigned long offset[4];};
        voffset = _mm256_and_si256(offset_vec, _mm256_set1_epi64x(1));

        __m256i offset_shift = _mm256_slli_epi64(voffset, 2);

        __m256i l1_data = _mm256_set_epi32(0, ss_.L1[index[0]], 0, ss_.L1[index[1]],0, ss_.L1[index[2]], 0, ss_.L1[index[3]]);
        __m256i shifted_data = _mm256_srlv_epi32(l1_data, offset_shift);

        union {__m256i value_vec; unsigned long value[4];};
        value_vec = _mm256_and_si256(shifted_data, _mm256_set1_epi64x(15));
        uint v1 = std::min({value[1],value[2],value[3]});

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
                            uint val = (ss_.L1[index[i]] >> (offset[ss_.depth-i] * 4)) & 0xf;
                            if(val < temp) {
                                if(val == 0)
                                    ss_.ones++;
                                ss_.L1[index[i]] += (temp - val) << (offset[ss_.depth-i] * 4);
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
            uint val = (ss_.L1[index[i]] >> (offset[ss_.depth-i] * 4)) & 0xf;
            if(val == 0)
                ss_.ones++;
            ss_.L1[index[i]] |= 0xf << (offset[ss_.depth-i] * 4);
        }

        weight = tmp_w;

        uint64_t h2= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[1]);
        uint v2 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            index[i] = h2 % ss_.filter_num2;
            value[i] = (uint)ss_.L2[index[i]];
            v2 = std::min(v2,(uint)value[i]);

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
        uint32_t h1[4];
        MurmurHash3_x64_128((unsigned char*)&key, 4, ss_.hash[0],h1);
        for(int i = 0; i < ss_.depth; i++) {
            int index = h1[i] % ss_.filter_num1;
            h1[i] >>= 4;
            int offset = h1[i] & 1;
            uint value = (ss_.L1[index] >> (offset * 4)) & 0xf;
            v1 = std::min(v1,value);
            h1[i] >>= 4;
        }
        if(v1 < ss_.T1)
            return v1*1.0/ss_.r0;
        v1 = v1*1.0/ss_.r0;
    //query filter2; flow size <=255
        uint v2 = UINT32_MAX;
        for(int i = 0; i < ss_.depth; i++) {
            uint64_t h2= MurmurHash64A((unsigned char*)&key,ss_.lgn,ss_.hash[i]);
            uint index = h2 % ss_.filter_num2;
            uint value = (uint)ss_.L2[index];
            v2 = std::min(v2,value);
        }

        if(v2 < ss_.T2)
            return (v1 + v2);
        
        uint v3 = WarmPart_query(key);
        return (v1 + v2 + v3);
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
        int result = HotPart_insert(key, swap_key, swap_val, weight), cold_res = 0;
        switch(result)
        {
            case 0: return;
            case 1:{
                if(ss_.r0 == 1) {
                    cold_res = ColdPart_insert(swap_key, GetCounterVal(swap_val));
                }
                else {
                    cold_res = ColdPart_insert_withR(swap_key, GetCounterVal(swap_val));
                }
                Judge_Rate();
                break;
            }
            case 2:  {
                if(ss_.r0 == 1) {
                    cold_res = ColdPart_insert(key, 1);
                }
                else {
                    cold_res = ColdPart_insert_withR(key, 1);
                }
                Judge_Rate();
                break;
            }
            default:
                printf("error return value !\n");
                exit(1);
        }
        
        if(cold_res) {
            WarmPart_insert(key,cold_res);
        }
    }

    void Update_parallel(key_tp key, val_tp weight, int finish) {
        HotPart(key,weight,finish);
    }

    

    void HotPart(key_tp key, val_tp weight, int flag) {
        if(flag) {
            Finish = 1;
            if (t2.joinable()) t2.join();
            if (t3.joinable()) t3.join();
            return;
        }

        key_tp swap_key;
        uint32_t swap_val = 0;
        int res = HotPart_insert(key, swap_key, swap_val, weight);

        if(res)
        {
            Data data = res == 1 ? Data{swap_key, GetCounterVal(swap_val)} : Data{key, weight};
            while (!cold_queue.try_enqueue(data)) {
                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }
        }
    }

    void ColdPart() {
        while (true) {
            int len = cold_queue.size_approx();
            if (Finish && len == 0) break;
            std::vector<Data> buffer(len);
            size_t count = cold_queue.try_dequeue_bulk(buffer.data(), len);
            if (count == 0) {
                // std::this_thread::yield();
                continue;
            }

            for (size_t i = 0; i < count; ++i) {
                Data& data = buffer[i];
                val_tp res = 0;
                if(ss_.r0 == 1)
                    ColdPart_insert(data.first, data.second);
                else
                    ColdPart_insert_withR(data.first, data.second);
                Judge_Rate();
                if (res) {
                    warm_queue.enqueue({data.first,res});
                }
            }
        }
    }

    void WarmPart() {
        while (true) {
            int len = warm_queue.size_approx();
            if (Finish && len == 0) break;
            std::vector<Data> buffer(len);
            size_t count = warm_queue.try_dequeue_bulk(buffer.data(), len);
            if (count == 0) {
                // std::this_thread::yield();
                continue;
            }

            for (size_t i = 0; i < count; ++i) {
                Data& data = buffer[i];
                WarmPart_insert(data.first, data.second);
            }
        }
    }

    void set_thread_affinity(std::thread& t, int cpu_id) {
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);

        pthread_t native_handle = t.native_handle();
        int result = pthread_setaffinity_np(native_handle, sizeof(cpu_set_t), &cpuset);
        if (result != 0) {
            std::cerr << "Failed to set affinity for CPU " << cpu_id << ": " << result << std::endl;
        }
    }

    
    
    
    
    val_tp PointQuery(key_tp key) {
        int pos = CalculateFP(key);
        for(int i = 0; i < COUNTERS_PER_BUCKET; i++){
            if(ss_.buckets[pos].key[i] == key){
                int res = ss_.buckets[pos].val[i];
                if(HIGHEST_BIT_IS_1(res)) {
                    res = (int)GetCounterVal(res) + ColdPart_query(key);
                }

                return res;
            }
        }

        uint result = ColdPart_query(key);
        
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
                    res = (int)GetCounterVal(res) + ColdPart_query(key);
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
            for(int j=0;j<COUNTERS_PER_BUCKET;j++) {
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
        std::cout<<" "<<sum_15<<" "<<sum<<" "<<sum_15*1.0/sum<<" "<<ss_.ones<<" "<<ss_.thre<<" "<<1.0/ss_.r0<<std::endl;

        return (1.0/ss_.r0);
    }

private:
    int CalculateFP(key_tp fp) {
        return CalculateBucketPos(fp) % ss_.bucket_num;
    }

    //Sketch data structure
    SS_type ss_;
    std::thread t1,t2,t3;
    moodycamel::ConcurrentQueue<Data> cold_queue; // lock-free queue
    moodycamel::ConcurrentQueue<Data> warm_queue; // lock-free queue
    int Finish = 0;
};
#endif
