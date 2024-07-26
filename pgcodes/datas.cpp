#include "pgcodes/datas.h"

namespace pangu {
namespace pgcodes {

void GCode::write_string(std::ostream &os) {
    os << "(";
    if (auto left = _left.get()) {
        left->write_string(os);
    }
    os << " _" << _value << "_ ";
    if (";" == _value) {
        os << std::endl;
    }
    if (auto right = _right.get()) {
        right->write_string(os);
    }
    os << ")";
}
} // namespace pgcodes
} // namespace pangu