#pragma once

namespace pangu {
namespace pgcodes {
enum ECodeType {
    If,
    Var,
    While,
    For,
    Switch,
    Goto,
    Do,
    Normal,
    Ignore,
    Block, // {...}
};
}
} // namespace pangu
