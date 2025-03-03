#ifndef COUNTMIN_H
#define COUNTMIN_H
#include <utility>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include "../lib/datatypes.hpp"
#include "sketch_base.hpp"
extern "C"
{
#include "../lib/hash.h"
}

class CU : public SketchBase {

    struct CU_type {

        //Sketch depth
        int depth;

        //Sketch width
        int  width;

        //Counter table
        val_tp * counts;
        unsigned long * hash;

        //# key bits
        int lgn;
    };

public:
    CU(int depth, int width) {
        cu_.depth = depth;
        cu_.width = width;
        cu_.lgn = sizeof(key_tp);
        cu_.counts = new val_tp[cu_.depth*cu_.width]();
        cu_.hash = new unsigned long[cu_.depth];
        char name[] = "CountMin_Conservative";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        for (int i = 0; i < cu_.depth; i++) {
            cu_.hash[i] = GenHashSeed(seed++);
        }
    }

    ~CU() {
        delete [] cu_.hash;
        delete [] cu_.counts;
    }

    void Update(key_tp key, val_tp weight) {
        val_tp min_v = UINT32_MAX;
        uint *index = new uint[cu_.depth]; 
        for (int i = 0; i < cu_.depth; i++) {
            uint64_t bucket = MurmurHash64A((unsigned char *)&key, cu_.lgn, cu_.hash[i]);
            index[i] =  i*cu_.width + bucket  % (uint32_t)cu_.width;
            min_v = std::min(min_v,cu_.counts[index[i]]);
        }
        uint temp = min_v;
        if(min_v != UINT32_MAX)
            temp += weight;
        for (int i = 0; i < cu_.depth; i++)
        {
            cu_.counts[index[i]] = std::max(temp,cu_.counts[index[i]]);
        }
        delete [] index;
        // double threshold = 20000;
        // // std::cout << "threshold = " << threshold << "\t result = " << result << std::endl;
        // if (result >= threshold) {
        //     std::pair<key_tp, val_tp> node(key, result);
        //     //If heap is empty
        //     if (heap_.size() == 0) {
        //         heap_.push_back(node);
        //         std::make_heap(heap_.begin(), heap_.end(), less_comparer_desc);
        //     } else {
        //         //Check if node is in the heap
        //         auto it = heap_.begin();
        //         for ( ; it != heap_.end(); ++it) {
        //             if (key == it->first) break;
        //         }
        //         //Node is not in the heap, insert it
        //         if (it == heap_.end()) {
        //             heap_.push_back(node);
        //             std::push_heap(heap_.begin(), heap_.end(), less_comparer_desc);
        //         } else {
        //             it->second = result;
        //             // std::make_heap(heap_.begin(), heap_.end(), less_comparer_desc);
        //         }
        //     }
        //     //Check whether heap size is bigger than threshold
        //     //std::cout<<sizeof(myvector)<<std::endl;
        //     if (heap_.size() >= 512) {
        //         std::sort_heap(heap_.begin(), heap_.end(), less_comparer_desc);
        //         heap_.erase(heap_.end());
        //         std::make_heap(heap_.begin(), heap_.end(), less_comparer_desc);
        //     }
        // }
    }

    val_tp PointQuery(key_tp key) {
        val_tp ret=0;
        val_tp degree=0;
        for (int i=0; i<cu_.depth; i++) {
            uint bucket = MurmurHash64A((unsigned char*)&key, cu_.lgn, cu_.hash[i]) % (uint32_t)cu_.width;
            if (i==0) {
                ret = cu_.counts[i*cu_.width+bucket];
            }
            degree = cu_.counts[i*cu_.width+bucket];
            ret = ret > degree ? degree : ret;
        }
        return ret;
    }

    void Query(val_tp thresh, myvector &results) {
        // std::cout<<hash_num<<" "<<access<<std::endl;
        // std::cout<<ccm_.visit<<std::endl;
        for (auto it = heap_.begin(); it != heap_.end(); it++) {
            std::pair<key_tp, val_tp> node;
            node.first = it->first;
            // printf("result: %016lx  %u\n",it->first,it->second);
            node.second = it->second;
            if (node.second > thresh) {
                results.push_back(node);
            }
        }
    }

    void Reset() {
        std::fill(cu_.counts, cu_.counts+cu_.depth*cu_.width, 0);
    }

private:
    static bool less_comparer_desc(const std::pair<key_tp, val_tp> &a, const std::pair<key_tp, val_tp> &b)
    {
        return (b.second > a.second);
    }
    //Sketch data structure
    CU_type cu_;
    myvector heap_;
};

#endif
