#ifndef _ASKETCH_H
#define _ASKETCH_H

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
using namespace std;

class ASketch: public SketchBase 
{
    typedef struct bucket
    {
        key_tp key;
        int new_count;
        int old_count;
    } Bucket;

	
public:
    struct As_type
    {
        int k;
        Bucket **filter;

        int depth;
        int width;
        val_tp *counter;

        uint64_t *hash;
    };

	ASketch(int depth, int width, int k)
	{

		as_.depth = depth;
        as_.width = width;
        as_.counter = new val_tp[as_.depth * as_.width]();

        as_.k = k;
		as_.filter = new Bucket*[as_.k]();
        for (int i = 0; i < as_.k; i++) {
            as_.filter[i] = (Bucket*)calloc(1, sizeof(Bucket));
            memset( as_.filter[i], 0, sizeof(Bucket));
        }

        as_.hash = new unsigned long[as_.depth]();
        char name[] = "Augmented sketch";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        for (int i = 0; i < as_.depth; ++i)
        {
            as_.hash[i] = GenHashSeed(seed++);
        }
	}

    ~ASketch() {
        delete [] as_.counter;
        delete [] as_.hash;

        for (int i = 0; i < as_.k; i++) {
            free(as_.filter[i]);
        }
        delete [] as_.filter;
    }

	void Update(key_tp key, val_tp val)
	{	
        int empty = -1;
        int min_v = INT32_MAX, min_i = 0;
        for(int i = 0; i < as_.k; i++)
		{
			if(as_.filter[i]->key == key) {
                as_.filter[i]->new_count += val;
                return;
            }
            if(as_.filter[i]->key == 0) {
                empty = i;
            }
            if(min_v > as_.filter[i]->new_count) {
                min_v = as_.filter[i]->new_count;
                min_i = i;
            }
		}
        if(empty != -1) {
            as_.filter[empty]->key = key;
            as_.filter[empty]->new_count = val;
            as_.filter[empty]->old_count = 0;
            return;
        }

        uint est;
        for (int i = 0; i < as_.depth; i++)
        {
            uint off = MurmurHash64A((unsigned char*)(&key),sizeof(key_tp),as_.hash[i]) % (uint)as_.width;
            uint index = i * as_.width + off;
            as_.counter[index] += val;
            if(i == 0) est = as_.counter[index];
            else est = min(est,as_.counter[index]);
        }

        if(est > (uint)min_v) {
            val_tp tmp_val = as_.filter[min_i]->new_count - as_.filter[min_i]->old_count;
            if( tmp_val > 0) {
                key_tp tmp_key = as_.filter[min_i]->key;
                for (int i = 0; i < as_.depth; i++)
                {
                    uint off = MurmurHash64A((unsigned char*)(&tmp_key),sizeof(key_tp),as_.hash[i]) % (uint)as_.width;
                    uint index = i * as_.width + off;
                    as_.counter[index] += tmp_val;
                }
            }
            as_.filter[min_i]->key = key;
            as_.filter[min_i]->new_count = est;
            as_.filter[min_i]->old_count = est;
        }
	}

	void Query(val_tp thresh, myvector &results)
	{
		for (int i = 0; i < as_.k; i++)
        {
            if((uint)as_.filter[i]->new_count > thresh) {
                key_tp key = as_.filter[i]->key;
                val_tp val = as_.filter[i]->new_count;
                results.push_back(std::make_pair(key,val));
            }
        }
	}

    val_tp PointQuery(key_tp key) {
        for (int i = 0; i < as_.k; i++)
        {
            if(as_.filter[i]->key == key) {
                return as_.filter[i]->new_count;
            }
        }

        uint est;
        for (int i = 0; i < as_.depth; i++)
        {
            uint off = MurmurHash64A((unsigned char*)(&key),sizeof(key_tp),as_.hash[i]) % (uint)as_.width;
            uint index = i * as_.width + off;
            if(i == 0) est = as_.counter[index];
            else est = min(est,as_.counter[index]);
        }

        return est;
    }

    void Reset() {
        for (int i = 0; i < as_.k; i++) {
            memset( as_.filter[i], 0, sizeof(Bucket));
        }
        for (int i = 0; i < as_.depth * as_.width; i++)
        {
            as_.counter[i] = 0;
        }
        
    }
        
private:
    As_type as_;
};

#endif//_ASKETCH_H