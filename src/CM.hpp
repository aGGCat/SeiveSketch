#ifndef COUNTMIN_H
#define COUNTMIN_H
#include <utility>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <unordered_map>
#include "../lib/datatypes.hpp"
#include "sketch_base.hpp"
extern "C"
{
#include "../lib/hash.h"
}
using namespace std;
class CountMin : public SketchBase {

    struct CCM_type {

        //Sketch depth
        int depth;

        //Sketch width
        int  width;

        //Counter table
        val_tp * counts;
        unordered_map<key_tp,val_tp> *map_;
        unsigned long * hash;

        int thre;

        //# key bits
        int lgn;
    };

public:
    CountMin(int depth, int width, int threshold) {
        ccm_.depth = depth;
        ccm_.width = width;
        ccm_.thre = threshold;
        ccm_.lgn = sizeof(key_tp);
        ccm_.counts = new val_tp[ccm_.depth*ccm_.width]();
        ccm_.map_ = new unordered_map<key_tp,val_tp>[ccm_.depth*ccm_.width]();
        ccm_.hash = new unsigned long[ccm_.depth];
        char name[] = "CountMin";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        for (int i = 0; i < ccm_.depth; i++) {
            ccm_.hash[i] = GenHashSeed(seed++);
        }
    }

    ~CountMin() {
        delete [] ccm_.hash;
        delete [] ccm_.counts;
    }

    void Update(key_tp key, val_tp weight) {
        val_tp result = 0;
        for (int i = 0; i < ccm_.depth; i++) {
            // hash_num++;
            uint64_t bucket = MurmurHash64A((unsigned char *)&key, ccm_.lgn, ccm_.hash[i]);
            int index =  i*ccm_.width + bucket  % (uint32_t)ccm_.width;
            ccm_.map_[index][key] += weight;
            if(ccm_.counts[index] + weight < UINT32_MAX)
                ccm_.counts[index] += weight;
            else
                ccm_.counts[index] = UINT32_MAX;
            if (i == 0) result = ccm_.counts[index];
            else result = std::min(result, ccm_.counts[index]);
        }
        // std::cout << "threshold = " << threshold << "\t result = " << result << std::endl;
        if (ccm_.thre != 0 && result > ccm_.thre) {
            std::pair<key_tp, val_tp> node(key, result);
            //If heap is empty
            if (heap_.size() == 0) {
                heap_.push_back(node);
                std::make_heap(heap_.begin(), heap_.end(), less_comparer_desc);
            } else {
                //Check if node is in the heap
                auto it = heap_.begin();
                for ( ; it != heap_.end(); ++it) {
                    // std::cout<<it->second<<std::endl;
                    if (key == it->first) break;
                }
                // std::cout<<"it->second"<<std::endl;
                //Node is not in the heap, insert it
                if (it == heap_.end()) {
                    if(heap_.size() < ccm_.thre) {
                        heap_.push_back(node);
                        std::push_heap(heap_.begin(), heap_.end(), less_comparer_desc);
                        return;
                    }
                    else {
                        std::sort_heap(heap_.begin(), heap_.end(), less_comparer_desc);
                        auto tmp  = heap_.at(heap_.size()-1);
                        if(result > tmp.second) {
                            tmp.first = key;
                            tmp.second = result;
                            std::make_heap(heap_.begin(), heap_.end(), less_comparer_desc);
                        }
                    }
                    
                } else {
                    it->second = result;
                    std::make_heap(heap_.begin(), heap_.end(), less_comparer_desc);
                    return;
                }
            }
            //Check whether heap size is bigger than threshold
            //std::cout<<sizeof(myvector)<<std::endl;
            if (heap_.size() >= 128) {
                // std::cout<<(heap_.capacity() * sizeof(std::pair<key_tp,val_tp>))<<std::endl;
                std::sort_heap(heap_.begin(), heap_.end(), less_comparer_desc);
                heap_.erase(heap_.end());
                std::make_heap(heap_.begin(), heap_.end(), less_comparer_desc);
            }
        }
    }

    val_tp PointQuery(key_tp key) {
        val_tp ret=0;
        val_tp degree=0;
        for (int i=0; i<ccm_.depth; i++) {
            uint bucket = MurmurHash64A((unsigned char*)&key, ccm_.lgn, ccm_.hash[i]) % (uint32_t)ccm_.width;
            if (i==0) {
                ret = ccm_.counts[i*ccm_.width+bucket];
            }
            degree = ccm_.counts[i*ccm_.width+bucket];
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
        std::fill(ccm_.counts, ccm_.counts+ccm_.depth*ccm_.width, 0);
    }

private:
    static bool less_comparer_desc(const std::pair<key_tp, val_tp> &a, const std::pair<key_tp, val_tp> &b)
    {
        return (a.second > b.second);
    }
    //Sketch data structure
    CCM_type ccm_;
    myvector heap_;

    // int hash_num = 0;
    // int access = 0;
};

#endif
