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
    struct Bucket
    {
        key_tp key[COUNTER_PER_BUCKET];
        val_tp val[COUNTER_PER_BUCKET];
    };
    
    struct ES_type {
        int bucket_num;

        Bucket *buckets;

        int counter_num;

        //Counter table
        uint8_t * counters;
        unsigned long hash;

        //# key bits
        int lgn;
    };

    ESketch(int bucket_num, int counter_num) {
        es_.bucket_num = bucket_num;
        es_.buckets = new Bucket[bucket_num]();
        es_.lgn = sizeof(key_tp);

        es_.counter_num = counter_num;
        es_.counters = new uint8_t[es_.counter_num]();
        char name[] = "Elastic sketch";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        es_.hash = GenHashSeed(seed++);
    }

    ~ESketch() {
        delete[] es_.counters;
        delete[] es_.buckets;
    }

    int HeavyPart_insert(key_tp key, key_tp &swap_key, uint32_t &swap_val, uint32_t f) {
        int pos = CalculateFP(key);//4.67993e+08
        uint32_t min_counter_val = GetCounterVal(es_.buckets[pos].val[0]);
        int min_counter = 0;
        int empty = -1;
        for(int i = 0; i < COUNTER_PER_BUCKET; i++){
            if(es_.buckets[pos].key[i] == key){
                if(es_.buckets[pos].val[i] < UINT32_MAX)
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

        if(!JUDGE_IF_SWAP(min_counter_val, guard_val)){
            es_.buckets[pos].val[MAX_VALID_COUNTER] = guard_val;
            return 2;
        }

        swap_key = es_.buckets[pos].key[min_counter];
        swap_val = es_.buckets[pos].val[min_counter];

        es_.buckets[pos].val[MAX_VALID_COUNTER] = 0;
        es_.buckets[pos].key[min_counter] = key;
        es_.buckets[pos].val[min_counter] = 0x80000001;
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

    void LightPart_insert(key_tp key, int f = 1) {
        uint32_t pos = MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash) % (uint32_t)es_.counter_num;
        /* insert */
        int new_val = (int)es_.counters[pos] + f;
        new_val = new_val < 255 ? new_val : 255;
        es_.counters[pos] = (uint8_t)new_val;
    }

	void LightPart_swap_insert(key_tp key, int f) {
        uint32_t pos = MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash) % (uint32_t)es_.counter_num;
        /* swap_insert */
        f = f < 255 ? f : 255;
        if (es_.counters[pos] < f) 
        {
            es_.counters[pos] = (uint8_t)f;
        }
    }

	uint32_t LightPart_query(key_tp key) {
        uint32_t pos = MurmurHash64A((unsigned char*)&key,es_.lgn,es_.hash) % (uint32_t)es_.counter_num;

        return (int)es_.counters[pos];
    }

    void Update(key_tp key, val_tp weight) {
        key_tp swap_key;
        uint32_t swap_val = 0;
        
        int result = HeavyPart_insert(key, swap_key, swap_val, weight);
        switch(result)
        {
            case 0: return;
            case 1:{
                if(HIGHEST_BIT_IS_1(swap_val)){
                    LightPart_insert(swap_key, GetCounterVal(swap_val));
                }
                else {
                    LightPart_swap_insert(swap_key, swap_val);
                }
                return;
            }
            case 2: {
                LightPart_insert(key, 1);  
                return;
            }
            default:
                printf("error return value !\n");
                exit(1);
        }
    }

    val_tp PointQuery(key_tp key) {
        uint32_t heavy_result = HeavyPart_query(key);
        if(heavy_result == 0 || HIGHEST_BIT_IS_1(heavy_result))
        {
            int light_result = LightPart_query(key);
            return (int)GetCounterVal(heavy_result) + light_result;
        }
        return heavy_result;
    }

    void Query(val_tp thresh, myvector &results) {
        
        for (int i = 0; i < es_.bucket_num; ++i) {
            for (int j = 0; j < MAX_VALID_COUNTER; ++j) 
            {
                key_tp key = es_.buckets[i].key[j];
                val_tp val = PointQuery(key);
                if (val > thresh) {
                    results.push_back(std::make_pair(key, val));
                }
            }
        }
    }

    void Reset() {
        memset(es_.buckets, 0, sizeof(Bucket) * es_.bucket_num);
        memset(es_.counters, 0, es_.counter_num);
    }

private:
    int CalculateFP(key_tp fp) {
        // return CalculateBucketPos(fp) % es_.bucket_num;
        return MurmurHash64A((unsigned char*)&fp,es_.lgn,es_.hash) % (uint32_t)es_.bucket_num;
    }

    //Sketch data structure
    ES_type es_;
};

#endif