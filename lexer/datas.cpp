#include "lexer/datas.h"
#include "lexer/pipelines.h"
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace pangu {
namespace lexer {

namespace {

std::string expandTabs(const std::string &line) {
    std::string expanded;
    size_t      column = 1;
    for (char ch : line) {
        if (ch == '\t') {
            const size_t spaces = 4 - ((column - 1) % 4);
            expanded.append(spaces, ' ');
            column += spaces;
            continue;
        }
        expanded.push_back(ch);
        ++column;
    }
    return expanded;
}

size_t displayColumn(const std::string &line, int column) {
    size_t display = 1;
    const int limit = std::max(1, column);
    for (int i = 1; i < limit && size_t(i - 1) < line.size(); ++i) {
        if (line[ size_t(i - 1) ] == '\t') {
            display += 4 - ((display - 1) % 4);
        } else {
            ++display;
        }
    }
    return display;
}

} // namespace

std::string stripInternalErrorPrefix(const std::string &message) {
    const std::string::size_type first = message.find(':');
    if (first == std::string::npos) {
        return message;
    }
    const std::string::size_type second = message.find(':', first + 1);
    if (second == std::string::npos) {
        return message;
    }
    return message.substr(second + 1);
}

std::string formatDiagnostic(const SourceLocation &location,
                             const std::string   &message,
                             size_t               highlight_width) {
    const std::string clean_message = stripInternalErrorPrefix(message);
    if (!location.valid()) {
        return "error: " + clean_message;
    }

    const std::string expanded_line = expandTabs(location.line_text);
    const size_t      caret_column =
        displayColumn(location.line_text, location.column);
    const size_t width = std::max<size_t>(1, highlight_width);

    std::stringstream ss;
    ss << location.file << ":" << location.line << ":" << location.column
       << ": error: " << clean_message << "\n";
    ss << expanded_line << "\n";
    ss << std::string(caret_column > 0 ? caret_column - 1 : 0, ' ') << "^";
    if (width > 1) {
        ss << std::string(width - 1, '~');
    }
    return ss.str();
}

const std::set<std::string> symbols{
    "!",  "!=",  "@", "%", "%=", "^",  "^=", "&",  "&=", "&&", "&&=", "*",
    "*=", "(",   ")", "-", "--", "-=", "+",  "++", "+=", "=",  "==",  "|",
    "||", "||=", "[", "]", "{",  "}",  "<",  "<=", "<<", ">",  "->",  ">=",
    ">>", ",",   ".", "?", "/",  "/=", ";",  ":",  "::", ":=", "=>"};
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
        {"=>"},
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
    "package", "import",  "as",      "if",     "else",   "for",
    "while",   "do",      "type",    "func",   "return", "struct",
    "class",   "switch",  "goto",    "try",    "catch",  "public",
    "static",  "const",   "final",   "var",    "pipeline",
    "impl",    "enum",    "interface","switcher","worker",
    "case",    "default", "match",
};
bool is_keywords(DLex *lex) {

    return isIdentifier(lex) && (keywords.count(lex->get()) > 0);
}
} // namespace lexer

} // namespace pangu
