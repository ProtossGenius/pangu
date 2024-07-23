#pragma once 

#define assert(e, msg) if (!e) throw std::runtime_error(msg);
#define assert_empty()