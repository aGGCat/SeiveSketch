#ifndef SS
#define SS
#include <utility>
#include <algorithm>
#include <cstring>
#include <iostream>
#include "../lib/param.h"
#include "sketch_base.hpp"
// #include "lossycount.h"
#include <set>


extern "C"
{
#include "../lib/hash.h"
}

#define LCLweight_t int
#define LCLitem_t uint32_t
#define LCL_NULLITEM 0x7FFFFFFF
	// 2^31 -1 as a special character

typedef struct lclcounter_t LCLCounter;

struct lclcounter_t
{
#ifdef TESTLOSSY
	int intable;
#endif
  key_tp item; // item identifier
  int hash; // its hash value
  LCLweight_t count; // (upper bound on) count for the item
  LCLweight_t delta; // max possible error in count for the value
  LCLCounter *prev, *next; // pointers in doubly linked list for hashtable
}; // 32 bytes

#define LCL_HASHMULT 3  // how big to make the hashtable of elements:
  // multiply 1/eps by this amount
  // about 3 seems to work well

typedef struct LCL_type
{
  int hashsize;
  int size;
  LCLCounter *root;
  LCLCounter *counters;
  LCLCounter ** hashtable; // array of pointers to items in 'counters'
} LCL_type;

class SpaceSaving : public SketchBase {
    public:
    struct SS_type {
        int width; // the number of counters

        LCL_type* counter;
        
        unsigned long hash;

        //# key bits
        int lgn;
    };

    SpaceSaving(int width) {
        ss_.counter = (LCL_type *) calloc(1,sizeof(LCL_type));
        // needs to be odd so that the heap always has either both children or
        // no children present in the data structure

        ss_.counter->size = width; // ensure that size is odd
        //fprintf(stderr, "phi=%f, size=%d\n", fPhi, result->size);
        ss_.counter->hashsize = LCL_HASHMULT*ss_.counter->size;
        ss_.counter->hashtable=(LCLCounter **) calloc(ss_.counter->hashsize,sizeof(LCLCounter*));
        ss_.counter->counters=(LCLCounter*) calloc(1+ss_.counter->size,sizeof(LCLCounter));
        // indexed from 1, so add 1

        for (int i=1; i<=ss_.counter->size;i++)
        {
            ss_.counter->counters[i].next=NULL;
            ss_.counter->counters[i].prev=NULL;
            ss_.counter->counters[i].item=LCL_NULLITEM;
            ss_.counter->counters[i].hash=-1;
            // initialize items and counters to zero
        }
        ss_.counter->root=&ss_.counter->counters[1]; // put in a pointer to the top of the heap

        char name[] = "Space-Saving";
        unsigned long seed = AwareHash((unsigned char*)name, strlen(name), 13091204281, 228204732751, 6620830889);
        ss_.hash = GenHashSeed(seed);
    }

    ~SpaceSaving() {
        free(ss_.counter->hashtable);
        free(ss_.counter->counters);
        free(ss_.counter);
    }

    void Update(key_tp key, val_tp val) {
        uint32_t hashval;
        LCLCounter * hashptr;
        // find whether new item is already stored, if so store it and add one
        // update heap property if necessary

        // lcl->n+=value;
        // lcl->counters->item = 0; // mark data structure as 'dirty'
        hashval=MurmurHash64A((unsigned char*)(&key),sizeof(LCLitem_t),ss_.hash) % (uint32_t)ss_.counter->hashsize;
        hashptr=ss_.counter->hashtable[hashval];
        // compute the hash value of the item, and begin to look for it in
        // the hash table

        while (hashptr) {
            if (hashptr->item==key) {
                hashptr->count += val; // increment the count of the item
                Heapify(hashptr-ss_.counter->counters); // and fix up the heap
                return;
            }
            else hashptr=hashptr->next;
        }
        
        // if control reaches here, then we have failed to find the item
        // so, overwrite smallest heap item and reheapify if necessary
        // fix up linked list from hashtable
        if (!ss_.counter->root->prev) // if it is first in its list
        {
            if (ss_.counter->root->hash != -1) {
                ss_.counter->hashtable[ss_.counter->root->hash]=ss_.counter->root->next;
            }
        }
        else
            ss_.counter->root->prev->next=ss_.counter->root->next;
        if (ss_.counter->root->next) // if it is not last in the list
            ss_.counter->root->next->prev=ss_.counter->root->prev;
        // update the hash table appropriately to remove the old item

        // slot new item into hashtable
        hashptr=ss_.counter->hashtable[hashval];
        ss_.counter->root->next=hashptr;
        if (hashptr)
            hashptr->prev=ss_.counter->root;
        ss_.counter->hashtable[hashval]=ss_.counter->root;
        // we overwrite the smallest item stored, so we look in the root
        // printf("small = (%u,%d) -> (%u,%d)\n",lcl->root->item,lcl->root->count,item,(value+lcl->root->delta));
        ss_.counter->root->prev=NULL;
        ss_.counter->root->item=key;
        ss_.counter->root->hash=hashval;
        ss_.counter->root->delta=ss_.counter->root->count;
        // update the implicit lower bound on the items frequency
        //  value+=lcl->root->delta;
        // update the upper bound on the items frequency
        ss_.counter->root->count = val + ss_.counter->root->delta;
        Heapify(1);
    }

    void Query(val_tp thresh, myvector &results) {
        // uint sum = 0;
        for(int i = 1; i <= ss_.counter->size; i++) {
            if(ss_.counter->counters[i].item != LCL_NULLITEM) {
                //now compare to the threshold
                // sum += (val_tp)ss_.counter->counters[i].count;
                if((val_tp)ss_.counter->counters[i].count > thresh) {
                    key_tp key = ss_.counter->counters[i].item;
                    std::pair<key_tp, val_tp> node;
                    node.first = key;
                    // node.second = ss_.counter->counters[i].count;//upper bound
                    node.second = ss_.counter->counters[i].count - ss_.counter->counters[i].delta;//lower bound
                    // std::cout<<key<<" "<<ss_.counter->counters[i].count<<" "<<ss_.counter->counters[i].delta<<std::endl;
                    node.second += bias_;
                    if(node.second > thresh)
                        results.push_back(node);
                }
            }
        }
    }

    void Reset() {
        for (int i=0; i<ss_.counter->hashsize;i++)
		ss_.counter->hashtable[i]=0;

        for (int i=1; i<=ss_.counter->size;i++)
        {
            ss_.counter->counters[i].next=NULL;
            ss_.counter->counters[i].prev=NULL;
            ss_.counter->counters[i].item=LCL_NULLITEM;
            ss_.counter->counters[i].count = 0;
            ss_.counter->counters[i].hash=-1;
            // initialize items and counters to zero
        }
    }

    val_tp PointQuery(key_tp key) {
        int result = 0;
        LCLCounter * hashptr;
        uint32_t hashval = MurmurHash64A((unsigned char*)(&key),sizeof(LCLitem_t),ss_.hash) % (uint32_t)ss_.counter->hashsize;
        // hashval=(int) hash31(lcl->hasha, lcl->hashb,hashv) % lcl->hashsize;
        hashptr=ss_.counter->hashtable[hashval];
        // compute the hash value of the item, and begin to look for it in
        // the hash table
        while (hashptr != NULL) {
            if (hashptr->item == key)
                break;
            else hashptr=hashptr->next;
        }
        if(hashptr) {
            // printf("%u %u %u\n",item,hashptr->count,hashptr->delta);
            result = (hashptr->count - hashptr->delta);//low est
            // result = hashptr->count; //upp est
        }
        else {
            result = 0;//low
            // result = ss_.counter->root->count;//upp
        }
        return (val_tp)result;
    }

    val_tp PointQuery_(key_tp key, int& flag) {
        int result = 0;
        LCLCounter * hashptr;
        uint32_t hashval = MurmurHash64A((unsigned char*)(&key),sizeof(LCLitem_t),ss_.hash) % (uint32_t)ss_.counter->hashsize;
        hashptr=ss_.counter->hashtable[hashval];
        // compute the hash value of the item, and begin to look for it in
        // the hash table
        while (hashptr != NULL) {
            if (hashptr->item == key)
                break;
            else hashptr=hashptr->next;
        }
        if(hashptr) {
            // printf("%u %u %u\n",item,hashptr->count,hashptr->delta);
            result = (hashptr->count - hashptr->delta);//low est
            // std::cout<<key<<" "<<result<<" "<<hashptr->count<<" "<<hashptr->delta<<std::endl;
            // result = (hashptr->count);//upp est
            if(hashptr->delta == 0)
                flag = 1;
            // result = hashptr->count; //upp est
        }
        else {
            result = 0;//low
            // result = ss_.counter->root->count;//upp
        }
        return (val_tp)result;
    }

    void Heapify(int ptr) {
        LCLCounter tmp;
        LCLCounter * cpt, *minchild;
        int mc;

        while(1)
        {
            if (((ptr<<1) + 1) > ss_.counter->size) break;
            // if the current node has no children

            cpt=&ss_.counter->counters[ptr]; // create a current pointer
            // mc=(ptr<<1)+
            // 	(((ptr<<1)+1==ss_.counter->size)
            // 	||((ss_.counter->counters[ptr<<1].count-ss_.counter->counters[ptr<<1].delta)
            // 	<(ss_.counter->counters[(ptr<<1)+1].count-ss_.counter->counters[(ptr<<1)+1].delta))? 0 : 1);
            mc=(ptr<<1)+
                (((ptr<<1)+1==ss_.counter->size)
                ||((ss_.counter->counters[ptr<<1].count)<(ss_.counter->counters[(ptr<<1)+1].count))? 0 : 1);
            minchild=&ss_.counter->counters[mc];
            // compute which child is the lesser of the two

            // if ((cpt->count-cpt->delta) < (minchild->count-minchild->delta)) break;
            if (cpt->count < minchild->count) break;
            // if the parent is less than the smallest child, we can stop

            //P("\tswapping %d[%d,%d] %d[%d,%d]\n", ptr, cpt->count, cpt->hash, mc, minchild->count, minchild->hash);
            tmp=*cpt;
            *cpt=*minchild;
            *minchild=tmp;
            // else, swap the parent and child in the heap

            if (cpt->hash==minchild->hash)
                // test if the hash value of a parent is the same as the
                // hash value of its child
            {
                // swap the prev and next pointers back.
                // if the two items are in the same linked list
                // this avoids some nasty buggy behaviour
                minchild->prev=cpt->prev;
                cpt->prev=tmp.prev;
                minchild->next=cpt->next;
                cpt->next=tmp.next;
            } else { // ensure that the pointers in the linked list are correct
                // check: hashtable has correct pointer (if prev ==0)
                if (!cpt->prev) { // if there is no previous pointer
                    if (cpt->item != LCL_NULLITEM)
                        ss_.counter->hashtable[cpt->hash]=cpt; // put in pointer from hashtable
                } else
                    cpt->prev->next=cpt;
                if (cpt->next)
                    cpt->next->prev=cpt; // place in linked list

                if (!minchild->prev) // also fix up the child
                    ss_.counter->hashtable[minchild->hash]=minchild;
                else
                    minchild->prev->next=minchild;
                if (minchild->next)
                    minchild->next->prev=minchild;
            }
            ptr=mc;
            // continue on with the heapify from the child position
        }
    }

    void setBias(int bias) {
        bias_ = bias;
    }

private:
    SS_type ss_;
    int bias_ = 0;
};

#endif