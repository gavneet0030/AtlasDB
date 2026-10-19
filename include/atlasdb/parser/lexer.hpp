#pragma once

#include "atlasdb/parser/token.hpp"

#include <string_view>
#include <vector>

namespace atlasdb {

class Lexer {
public:
    explicit Lexer(std::string_view input);
    [[nodiscard]] auto tokenize() -> std::vector<Token>;

private:
    [[nodiscard]] auto scan_token() -> Token;
    void skip_whitespace();
    [[nodiscard]] auto peek() const -> char;
    [[nodiscard]] auto advance() -> char;
    [[nodiscard]] auto match(char expected) -> bool;

    std::string_view input_;
    std::size_t position_{0};
};

}  // namespace atlasdb
