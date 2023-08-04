#pragma once
#include <cstdint>
#include "dense_storage_view.h"
#include <luisa/core/stl/vector.h>

namespace luisa::compute::tensor {
class DenseVectorDesc {
public:
    int offset = 0;// start
    int inc = 1;   // stride
};

class DenseVectorView {
public:
    int n = 0; // logical size
    vector<DenseStorageView> storage;
    DenseVectorDesc desc;
};
}// namespace luisa::compute::tensor