#include <utility>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "../lib/datatypes.hpp"
#include "sketch_base.hpp"
#include "SpaceSaving.hpp"
extern "C"
{
#include "../lib/hash.h"
}

class ColdFilter : public SketchBase {

    struct CF {
        int depth1;
        int depth2;

        int width1;
        int width2;
        int w_word;

        uint64_t* L1;
        uint16_t* L2;
        // uint16_t* card;

        unsigned long * hash;

        int lgn;

    };

    public:
        ColdFilter(int memory, int ss_k) {
            cf_.depth1 = 3;
            cf_.depth2 = 3;
            int m1_in_bytes = int(memory * l1_ratio / 100.0);
            int m2_in_bytes = int(memory * (100 - l1_ratio) / 100.0);
            cf_.width1 = m1_in_bytes * 8 / 4;
            cf_.w_word = cf_.width1 / 16;
            cf_.width2 = m2_in_bytes * 8 /16;   
            cf_.L1 = new uint64_t[cf_.w_word]();
            cf_.L2 = new uint16_t[cf_.width2]();

            thresh_ = 241;
            ss_ = new SpaceSaving(ss_k);

            cf_.hash = new unsigned long[4];
            char name[] = "ColdFilter";
            unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
            for (int i = 0; i < 4; i++) {
                cf_.hash[i] = GenHashSeed(seed++);
            }
            ss_->setBias(256);
        }

        ~ColdFilter() {
            delete[] cf_.hash;
            delete[] cf_.L1;
            delete[] cf_.L2;
        }

        void Update(key_tp key, val_tp weight) {
            int v1 = 1 << 30;
            int value[4];
            int index[4];
            int offset[4];
            key_tp kick_ID = key;
            uint64_t hash_value = MurmurHash64A((unsigned char*)&kick_ID,sizeof(key_tp),cf_.hash[0]);
            int word_index = hash_value % cf_.w_word;
            hash_value >>= 16;

            uint64_t temp = cf_.L1[word_index];
            for (int i = 0; i < cf_.depth1; i++) {
                offset[i] = (hash_value & 0xF);
                value[i] = (temp >> (offset[i] << 2)) & 0xF;
                v1 = std::min(v1, value[i]);
                hash_value >>= 4;
            }

            int temp2 = v1 + weight;
            if (temp2 <= 15) { // maybe optimized use SIMD?
                for (int i = 0; i < cf_.depth1; i++) {
                    int temp3 = ((temp >> (offset[i] << 2)) & 0xF);
                    if (temp3 < temp2) {
                        temp += ((uint64_t)(temp2 - temp3) << (offset[i] << 2));
                    }
                }
                cf_.L1[word_index] = temp;
                return;
            }

            for (int i = 0; i < cf_.depth1; i++) {
                temp |= ((uint64_t)0xF << (offset[i] << 2));
            }
            cf_.L1[word_index] = temp;

            int delta1 = 15 - v1;
            weight -= delta1;

            int v2 = 1 << 30;
            for (int i = 0; i < cf_.depth2; i++) {
                uint hv = MurmurHash2((unsigned char*)&kick_ID,sizeof(key_tp),cf_.hash[i]);
                index[i] = hv % cf_.width2;
                value[i] = cf_.L2[index[i]];
                v2 = std::min(value[i], v2);
            }

            temp2 = v2 + weight;
            if (temp2 <= thresh_) {
                for (int i = 0; i < cf_.depth2; i++) {
                    cf_.L2[index[i]] = (cf_.L2[index[i]] > temp2) ? cf_.L2[index[i]] : temp2;
                }
                return;
            }

            for (int i = 0; i < cf_.depth2; i++) {
                cf_.L2[index[i]] = thresh_;
            }

            int delta2 = thresh_ - v2;
            weight -= delta2;
            if(weight)
                ss_->Update(key,weight);
        }

        val_tp PointQuery(key_tp key) {
            int v1 = 1 << 30;
            key_tp kick_ID = key;
            uint64_t hash_value = MurmurHash64A((unsigned char*)&kick_ID,sizeof(key_tp),cf_.hash[0]);
            int word_index = hash_value % cf_.w_word;
            hash_value >>= 16;

            uint64_t temp = cf_.L1[word_index];
            for (int i = 0; i < cf_.depth1; i++) {
                int of, val;
                of = (hash_value & 0xF);
                val = (temp >> (of << 2)) & 0xF;
                v1 = std::min(val, v1);
                hash_value >>= 4;
            }

            if (v1 != 15)
                return v1;

            int v2 = 1 << 30;
            for (int i = 0; i < cf_.depth2; i++) {
                uint hv = MurmurHash2((unsigned char*)&kick_ID,sizeof(key_tp),cf_.hash[i]);
                int index = hv % cf_.width2;
                v2 = std::min((int)cf_.L2[index], v2);
            }
            if(v2 != thresh_)
                return v1 + v2;

            int v3 = ss_->PointQuery(key);
            return v1 + v2 + v3;
        }

        void Query(val_tp thresh, myvector &results) {
            ss_->Query(thresh,results);
        }

        void Reset() {
            memset(cf_.L1, 0, cf_.w_word);
            memset(cf_.L2, 0, cf_.width2);
            ss_->Reset();
        }

    
    private:
        //Update threshold
        int thresh_;

        //Sketch data structure
        CF cf_;
        SpaceSaving* ss_;

        int l1_ratio = 65;
};