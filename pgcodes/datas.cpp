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

void GCode::adoptRightAsSelf() {
    pgassert(isPlaceholder());
    GCode *right = _right.release();
    _value       = right->_value;
    _value_type  = right->_value_type;
    _left        = std::move(right->_left);
    _right       = std::move(right->_right);
    delete right;
}
} // namespace pgcodes
} // namespace pangu
