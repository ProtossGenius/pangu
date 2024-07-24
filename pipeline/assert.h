#pragma once
#include <sstream>
#include <string>
inline std::string merge_string() { return ""; }
inline std::string merge_string(std::stringstream &ss) { return ss.str(); }

template <typename T, typename... Args>
inline std::string merge_string(std::stringstream &ss, T head, Args... args) {
    ss << head;
    merge_string(ss, args...);
    return ss.str();
}
template <typename... Args> inline std::string merge_string(Args... args) {
    std::stringstream ss;
    return merge_string(ss, args...);
}

#define pgassert_msg(e, msg)                                                   \
    if (!(e))                                                                  \
        throw std::runtime_error(merge_string(__FILE__, ":", __LINE__,         \
                                              " Assertion `", #e,              \
                                              "` failed. detail: ", msg));

#define pgassert(e) pgassert_msg(e, "assert fail.")