#pragma once

namespace pangu {
namespace pgcodes {
enum ECodeType {
    If = 0,
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
