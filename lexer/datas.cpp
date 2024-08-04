#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include <fstream>
#include <map>
#include <stdexcept>
#include <vector>

namespace pangu {
namespace lexer {

const std::set<std::string> symbols{
    "!",  "!=",  "@", "%", "%=", "^",  "^=", "&",  "&=", "&&", "&&=", "*",
    "*=", "(",   ")", "-", "--", "-=", "+",  "++", "+=", "=",  "==",  "|",
    "||", "||=", "[", "]", "{",  "}",  "<",  "<=", "<<", ">",  "->",  ">=",
    ">>", ",",   ".", "?", "/",  "/=", ";",  ":",  "::", ":="};
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
        {"=",
         "+=", "-=", "*=", "/=", "%=", "&=", "|=", "&&=", "||=", "^=", ":="},
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
    pgassert_msg(power.count(s) != 0,
                 "symbol_power fail, no such symbol: " + s);
    return power[ s ];
}

const std::set<std::string> keywords{
    "if",     "for",    "while",  "do",    "type", "func",
    "struct", "class",  "switch", "goto",  "try",  "catch",
    "public", "static", "const",  "final", "var",
};
bool is_keywords(DLex *lex) {

    return isIdentifier(lex) && (keywords.count(lex->get()) > 0);
}
} // namespace lexer

} // namespace pangu
