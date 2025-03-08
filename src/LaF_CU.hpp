#ifndef _SF_CU_SKETCH_H
#define _SF_CU_SKETCH_H

#include "CU.hpp"
#include "LadderFilter.hpp"

class CUSketchWithSF
{
public:
    LadderFilter sf;
    // CUSketch<int((total_memory_in_bytes) * (100 - filter_memory_percent) / 100), 3> cu;
    CU *cu;
    int thres;

    CUSketchWithSF(uint64_t total_memory_in_bytes, int filter_memory_percent,int _bucket_num1, int _bucket_num2,
                   int _cols, int _counter_len,
                   int _thres1, int _thres2)
    {
        thres = _thres1;
        sf = LadderFilter(_bucket_num1, _bucket_num2,
                          _cols,  _counter_len,
                          _thres1, _thres2);
        cu = new CU(3,int((total_memory_in_bytes) * (100 - filter_memory_percent) / 100) / (3*4));
        // std::cout<<(int((total_memory_in_bytes) * (100 - filter_memory_percent) / 100) / (3*4))<<" "<<_bucket_num1<<" "<<_bucket_num2<<std::endl;
    }

    inline void insert(key_tp item)
    {
        auto res = sf.insert(item);
        if (res >= thres)
            cu->Update(item, (res == thres) ? thres : 1);
    }

    inline void build(uint32_t *items, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            sf.insert(items[i]);
        }
    }

    inline int query(key_tp item)
    {
        int ret = cu->PointQuery(item);
        if(ret == 0)
            ret = sf.pointQuery(item);
        return ret;
    }

    inline int batch_query(uint32_t items[], int n)
    {
        int ret = 0;
        for (int i = 0; i < n; ++i)
        {
            ret += CUSketchWithSF::query(items[i]);
        }

        return ret;
    }

    inline void Reset() {
        cu->Reset();
        sf.Reset();
    }
};

#endif // _SFCUSKETCH_H