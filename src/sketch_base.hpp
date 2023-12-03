#ifndef SKETCH_BASE_H
#define SKETCH_BASE_H
#include "../lib/datatypes.hpp"

class SketchBase {

public:
    virtual ~SketchBase(){};

    virtual void Update(key_tp key, val_tp val) = 0;

    // virtual void Query(val_tp thresh, myvector &results) = 0;

    virtual void Reset() = 0;

    virtual val_tp PointQuery(key_tp key) = 0;
};
#endif

