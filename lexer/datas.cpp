#include "lexer/datas.h"
#include <stdexcept>
#include <vector>

namespace pangu {
namespace lexer {

const std::set<std::string> symbols{
    "!",  "!=",  "@", "%", "%=", "^",  "^=", "&",  "&=", "&&", "&&=",
    "*",  "*=",  "(", ")", "-",  "-=", "+",  "+=", "=",  "==", "|",
    "||", "||=", "[", "]", "{",  "}",  "<",  "<=", "<<", ">",  "->",
    ">=", ">>",  ",", ".", "?",  "/",  "/=", ";",  ":",  "::"};
std::map<std::string, int> getSymbolPowerMap() {
    static std::vector<std::vector<std::string>> power_vec{
        {"::"},
        {".", "->", "[", "]", "(", ")", "{", "}"},
        {"++", "--"},
        {"!"},
        {"*", "/", "%"},
        {"+", "-"},
        {"<<", ">>"},
        {">", "<", ">=", "<="},
        {"==", "!="},
        {"&"},
        {"^"},
        {"|"},
        {"&&"},
        {"||"},
        {":"},
        {"?"},
        {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "&&=", "||=", "^="},
        {","},
        {";"},
        // not in use
        {"@"},
    };
    std::map<std::string, int> mmp;
    int                        i = 0;
    for (auto &it : power_vec) {
        for (auto &symbol : it) {
            mmp[ symbol ] = -i;
        }
        ++i;
    }
    for (auto &symb : lexer::symbols) {
        if (mmp.count(symb) == 0) {
            throw std::runtime_error("getSymbolPowerMap symbol '" + symb +
                                     "' not exist in power map");
        }
    }
    return mmp;
}
int symbol_power(const std::string &s) {
    static std::map<std::string, int> power = getSymbolPowerMap();
    if (power.count(s) == 0) {
        throw std::runtime_error("symbol_power fail, no such symbol: " + s);
    }
    return power[ s ];
}

} // namespace lexer

} // namespace pangu