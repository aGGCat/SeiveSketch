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

static const int COUNT[2] = {1, -1};

class WavingSketch : public SketchBase {
    public:
    
    struct Waving_type {
        int slot_num;
        int counter_num;
        int BUCKET_NUM;

        //bucket
        key_tp **items;
        int **counters;
        int16_t **incast;

        unsigned long * hash;

        //# key bits
        int lgn = 4;
    };

    WavingSketch(int bucket,int slot, int counter){
        ws_.BUCKET_NUM = bucket;
        ws_.slot_num = slot;
        ws_.counter_num = counter;
        ws_.lgn = sizeof(key_tp);

        ws_.items = new key_tp*[bucket]();
        ws_.counters = new int*[bucket]();
        ws_.incast = new int16_t*[bucket]();

        for (int i = 0; i < bucket; i++)
        {
            ws_.items[i] = new key_tp[slot]();
            ws_.counters[i] = new int[slot]();
            ws_.incast[i] = new int16_t[counter]();
        }
        // std::cout<<ws_.incast[791][7]<<std::endl;
        

        ws_.hash = new uint64_t[4]();
        char name[] = "WavingSketch";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        for (int i = 0; i < 4; i++) {
            ws_.hash[i] = GenHashSeed(seed++);
        }
    }

    ~WavingSketch() {
        delete [] ws_.hash;
        for (int i = 0; i < ws_.BUCKET_NUM; i++)
        {
            delete [] ws_.items[i];
            delete [] ws_.counters[i];
            delete [] ws_.incast[i];
        }
        
        delete [] ws_.items;
        delete [] ws_.counters;
        delete [] ws_.incast;
    }

    void Update(key_tp key, val_tp weight) {
        uint32_t pos = MurmurHash64A((unsigned char*)&key,ws_.lgn,ws_.hash[0]) % ws_.BUCKET_NUM;
        uint32_t choice = MurmurHash64A((unsigned char*)&key,ws_.lgn,ws_.hash[1]) & 1;
        uint32_t whichcast = MurmurHash64A((unsigned char*)&key,ws_.lgn,ws_.hash[2]) % ws_.counter_num;
        int min_num = INT32_MAX;
        uint32_t min_pos = -1;
        int empty = -1;
        for (int i = 0; i < ws_.slot_num; ++i) {
            if (ws_.counters[pos][i] == 0) {
                // The error free item's counter is negative, which is a trick to 
                // be differentiated from items which are not error free.
                empty = i;
            }
            else if (ws_.items[pos][i] == key) {
                if (ws_.counters[pos][i] < 0)
                    ws_.counters[pos][i]--;
                else {
                    ws_.counters[pos][i]++;
                    ws_.incast[pos][whichcast] += COUNT[choice];
                }
                return;
            }

            int counter_val = std::abs(ws_.counters[pos][i]);
            if (counter_val < min_num) {
                min_num = counter_val;
                min_pos = i;
            }
        }
        if(empty != -1) {
            ws_.items[pos][empty] = key;
            ws_.counters[pos][empty] = -1;
            return;
        }

        if (ws_.incast[pos][whichcast] * COUNT[choice] >= int(min_num * 1)) {
            if (ws_.counters[pos][min_pos] < 0) {
                uint32_t min_choice = MurmurHash64A((unsigned char*)&(ws_.items[pos][min_pos]),ws_.lgn,ws_.hash[1]) & 1;
                int index = MurmurHash64A((unsigned char*)&(ws_.items[pos][min_pos]),ws_.lgn,ws_.hash[2]) % ws_.counter_num;
                ws_.incast[pos][index] -= COUNT[min_choice] * ws_.counters[pos][min_pos];
            }
            ws_.items[pos][min_pos] = key;
            ws_.counters[pos][min_pos] = min_num + 1;
        }
        

        ws_.incast[pos][whichcast] += COUNT[choice];
    }

    val_tp PointQuery(key_tp key) {
        uint32_t pos = MurmurHash64A((unsigned char*)&key,ws_.lgn,ws_.hash[0]) % ws_.BUCKET_NUM;
        // uint32_t choice = MurmurHash2((unsigned char*)&key,ws_.lgn/8,ws_.hash[1]) & 1;
        // uint32_t whichcast = MurmurHash64A((unsigned char*)&key,ws_.lgn,ws_.hash[2]) % ws_.counter_num;
        int retv = 0;

        for (int i = 0; i < ws_.slot_num; ++i) {
            if (ws_.items[pos][i] == key) {
                return std::abs(ws_.counters[pos][i]);
            }
        }
        return retv;
    }

    void Query(val_tp thresh, myvector &results) {
        for (int i = 0; i < ws_.BUCKET_NUM; i++)
        {
            for (int j = 0; j < ws_.slot_num; j++)
            {
                uint32_t key = ws_.items[i][j];
                int val = std::abs(ws_.counters[i][j]);
                if(val > (int)thresh) {
                    results.push_back(std::make_pair(key,val));
                }
            }
            
        }
    }

    void Reset() {
        for (int i = 0; i < ws_.BUCKET_NUM; i++)
        {
            for(int j = 0; j < ws_.slot_num; j++){
                ws_.items[i][j] = 0;
                ws_.counters[i][j] = 0;
            }
            for (int j = 0; j < ws_.counter_num; j++)
            {
                ws_.incast[i][j] = 0;
            } 
        }
    }

private:
    //Sketch data structure
    Waving_type ws_;
};

