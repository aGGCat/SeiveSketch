#ifndef SF_H
#define SF_H

#include "../lib/datatypes.hpp"
#include "../lib/param.h"
#include "sketch_base.hpp"
#include <cstring>
#include <algorithm>

using namespace std;

uint32_t getFP(key_tp key, int key_len)
{
    return MurmurHash2((const char *)&key, sizeof(key_tp), 100) % 0xFFFF + 1;
}

class Bucket
{
public:
    uint32_t *fp;
    uint32_t *counter;

    Bucket() {}

    Bucket(int cols)
    {
        fp = new uint32_t[cols];
        counter = new uint32_t[cols];
        memset(fp, 0, sizeof(uint32_t) * cols);
        memset(counter, 0, sizeof(uint32_t) * cols);
    }

    void permutation(int p) // permute the p-th item to the first
    {
        for (int i = p; i > 0; --i)
        {
            swap(fp[i], fp[i - 1]);
            swap(counter[i], counter[i - 1]);
        }
    }

    void Reset(int cols) {
        memset(fp, 0, sizeof(uint32_t) * cols);
        memset(counter, 0, sizeof(uint32_t) * cols);
    }
};

class LadderFilter
{
public:
    Bucket *Bucket1;
    Bucket *Bucket2;
    uint64_t l1Hash;
    uint64_t l2Hash;
    int bucket_num1, bucket_num2;
    int cols, counter_len;
    int thres2;

    LadderFilter() {}
    LadderFilter(int _bucket_num1, int _bucket_num2,
                 int _cols, int _counter_len,
                 int _thres1, int _thres2)
        : bucket_num1(_bucket_num1), bucket_num2(_bucket_num2),
          cols(_cols), counter_len(_counter_len),
          thres2(_thres2)
    {
        Bucket1 = new Bucket[bucket_num1];
        for (int i = 0; i < bucket_num1; ++i)
            Bucket1[i] = Bucket(_cols);
        Bucket2 = new Bucket[bucket_num2];
        for (int i = 0; i < bucket_num2; ++i)
            Bucket2[i] = Bucket(_cols);
        char name[] = "Ladder Filter";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        l1Hash = GenHashSeed(seed++);
        l2Hash = GenHashSeed(seed++);
    }

    void Reset() {
        for (int i = 0; i < bucket_num1; ++i)
            Bucket1[i].Reset(cols);
        for (int i = 0; i < bucket_num2; ++i)
            Bucket2[i].Reset(cols);
    }

    int insert(key_tp key, int count = 1) // return dequeued item
    {
        auto keyfp = getFP(key, sizeof(key_tp));

        /* if the item is in B1 */
        uint index1 = MurmurHash64A((unsigned char*)&key, sizeof(key_tp), l1Hash) % (uint)bucket_num1; 
        auto &B1 = Bucket1[index1];//Bucket1[l1Hash->run((const char *)&key, 4) % bucket_num1];
        for (int i = 0; i < cols; ++i)
            if (B1.fp[i] == keyfp)
            {
                B1.permutation(i);
                B1.counter[0] = min(B1.counter[0] + count, (1u << counter_len) - 1);
                return B1.counter[0];
            }
        
        /* if the item is in B2 */
        uint index2 = MurmurHash64A((unsigned char*)&key, sizeof(key_tp), l2Hash) % (uint)bucket_num2; 
        auto &B2 = Bucket2[index2];//Bucket2[l2Hash->run((const char *)&key, 4) % bucket_num2];
        for (int i = 0; i < cols; ++i)
            if (B2.fp[i] == keyfp)
            {
                B2.permutation(i);
                B2.counter[0] = min(B2.counter[0] + count, (1u << counter_len) - 1);
                return B2.counter[0];
            }
        

        /* if B1 is not full */
        for (int i = 0; i < cols; ++i)
            if (B1.fp[i] == 0)
            {
                B1.permutation(i);
                B1.fp[0] = keyfp;
                B1.counter[0] = count;
                return B1.counter[0];
            }

        /* dequeue the LRU item in B1, if is promising item, insert to B2 */
        if (B1.counter[cols - 1] >= (uint)thres2)
        {
            /* if Bucket2 is not full*/
            bool isFull = true;
            for (int i = 0; i < cols; ++i)
                if (B2.fp[i] == 0)
                {
                    B2.permutation(i);
                    B2.fp[0] = B1.fp[cols - 1];
                    B2.counter[0] = B1.counter[cols - 1];
                    isFull = false;
                    break;
                }

            /* dequeue the LRU item in B2 */
            if (isFull)
            {
                B2.permutation(cols - 1);
                B2.fp[0] = B1.fp[cols - 1];
                B2.counter[0] = B1.counter[cols - 1];
            }
        }

        /* insert the item to the empty cell */
        B1.permutation(cols - 1);
        B1.fp[0] = keyfp;
        B1.counter[0] = count;
        return B1.counter[0];
    }

    int pointQuery(uint key) {
        auto keyfp = getFP(key, sizeof(key_tp));
        uint index1 = MurmurHash64A((unsigned char*)&key, sizeof(key_tp), l1Hash) % (uint)bucket_num1; 
        auto &B1 = Bucket1[index1];
        for (int i = 0; i < cols; ++i)
            if (B1.fp[i] == keyfp)
            {
                return B1.counter[i];
            }

        uint index2 = MurmurHash64A((unsigned char*)&key, sizeof(key_tp), l2Hash) % (uint)bucket_num2; 
        auto &B2 = Bucket2[index2];//Bucket2[l2Hash->run((const char *)&key, 4) % bucket_num2];
        for (int i = 0; i < cols; ++i)
            if (B2.fp[i] == keyfp)
            {
                return B2.counter[i];
            }
        return 0;
    }
};

#endif