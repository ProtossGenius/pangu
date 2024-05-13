#pragma once
#include <cstddef>
namespace pangu {
template <typename T> inline T read(char *bytes, size_t &ptr) {
    throw "unimplement";
}
template <> inline int read<int>(char *bytes, size_t &ptr) {
    int val = *(int *) (bytes + ptr);
    ptr += 4;
    return val;
}

} // namespace pangu