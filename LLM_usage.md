lexer.cpp related
```
bool Lexer::tokenize_punct_delimeter() {

}

enum class TokenType : uint32_t {

INVALID,



// special

UNKNOWN = static_cast<uint32_t>(TokenTypeCat::SPECIAL) + 1,

END_OF_FILE,



// keyword

AS = static_cast<uint32_t>(TokenTypeCat::KEYWORD) + 1,

BREAK, CONST, CONTINUE, CRATE, ELSE, ENUM, EXTERN, FALSE, FN, FOR, IF, IMPL, IN, LET, LOOP, MATCH, MOD,

MOVE, MUT, PUB, REF, RETURN, SELF_OBJECT, SELF_TYPE, STATIC, STRUCT, SUPER, TRAIT, TRUE, TYPE, UNSAFE,

USE, WHERE, WHILE, ASYNC, AWAIT, DYN,



// identifier

IDENTIFIER = static_cast<uint32_t>(TokenTypeCat::IDENTIFIER) + 1,



// literal

LITERAL = static_cast<uint32_t>(TokenTypeCat::LITERAL) + 1,

INTEGER_LITERAL, FLOAT_LITERAL, STRING_LITERAL,



// lifetime



// LIFETIME = static_cast<uint32_t>(TokenTypeCat::LIFETIME) + 1,



// punctuation

PLUS = static_cast<uint32_t>(TokenTypeCat::PUNCTUATION) + 1,

MINUS, STAR, SLASH, PERCENT, CARET, NOT, AND, OR, AND_AND, OR_OR, SHL, SHR, PLUS_EQ, MINUS_EQ, STAR_EQ,

SLASH_EQ, PERCENT_EQ, CARET_EQ, AND_EQ, OR_EQ, SHL_EQ, SHR_EQ,EQ, EQ_EQ, NE, GT, LT, GE, LE, AT, UNDERSCORE,

DOT, DOT_DOT, DOT_DOT_DOT, DOT_DOT_EQ, COMMA, SEMI, COLON, PATH_SEP, R_ARROW, FAT_ARROW, L_ARROW, POUND, DOLLAR,

QUESTION, TILDE,



// delimiter

L_CURLY_BRACE = static_cast<uint32_t>(TokenTypeCat::DELIMITER) + 1,

R_CURLY_BRACE, L_SQUARE_BRACKET, R_SQUARE_BRACKET, L_PARENTHESES, R_PARENTHESES,

};

这一个检查部分怎么写



要编写 tokenize_punct_delimeter() 函数，你应该采取一种基于最长匹配的策略。这意味着你需要优先检查那些由多个字符组成的标点符号，如 => 或 ::，然后再检查单个字符的标点符号，如 + 或 -。

标点和分隔符的优先级匹配
在编写这个函数时，遵循以下步骤可以确保正确性：

多字符匹配优先：首先检查由两个或三个字符组成的标点符号。例如，在看到 > 字符时，你必须先检查它后面是否跟着另一个 > 以构成 >>，或者跟着 = 以构成 >=，而不是立即将其标记为 GT。

单字符匹配兜底：如果多字符的模式都匹配失败，再将当前字符作为单个标点符号来匹配。

使用 switch 语句：switch 语句非常适合这种基于字符的、分支众多的匹配逻辑。它比一长串 if/else if 语句更清晰、更高效。

按长度分组：在 switch 语句中，你可以根据可能出现的字符，将多字符的匹配逻辑分组，这有助于提高代码的可读性。

函数实现
以下是 tokenize_punct_delimeter() 函数的实现，它遵循上述原则，并覆盖了你提供的所有标点和分隔符。

C++

bool Lexer::tokenize_punct_delimeter() {
if (_pos >= _src_code.length()) {
return false;
}

    char ch = _src_code[_pos];
    TokenType type = TokenType::INVALID;
    std::size_t advance_count = 1;

    switch (ch) {
        case '+':
            if (_pos + 1 < _src_code.length() && _src_code[_pos + 1] == '=') {
                type = TokenType::PLUS_EQ;
                advance_count = 2;
            } else {
                type = TokenType::PLUS;
            }
            break;
        case '-':
            if (_pos + 1 < _src_code.length()) {
                char next_ch = _src_code[_pos + 1];
                if (next_ch == '=') {
                    type = TokenType::MINUS_EQ;
                    advance_count = 2;
                } else if (next_ch == '>') {
                    type = TokenType::R_ARROW;
                    advance_count = 2;
                }
            } else {
                type = TokenType::MINUS;
            }
            break;
        case '*':
            if (_pos + 1 < _src_code.length() && _src_code[_pos + 1] == '=') {
                type = TokenType::STAR_EQ;
                advance_count = 2;
            } else {
                type = TokenType::STAR;
            }
            break;
        case '/':
            if (_pos + 1 < _src_code.length() && _src_code[_pos + 1] == '=') {
                type = TokenType::SLASH_EQ;
                advance_count = 2;
            } else {
                type = TokenType::SLASH;
            }
            break;
        case '%':
            if (_pos + 1 < _src_code.length() && _src_code[_pos + 1] == '=') {
                type = TokenType::PERCENT_EQ;
                advance_count = 2;
            } else {
                type = TokenType::PERCENT;
            }
            break;
        case '^':
            if (_pos + 1 < _src_code.length() && _src_code[_pos + 1] == '=') {
                type = TokenType::CARET_EQ;
                advance_count = 2;
            } else {
                type = TokenType::CARET;
            }
            break;
        case '!':
            if (_pos + 1 < _src_code.length() && _src_code[_pos + 1] == '=') {
                type = TokenType::NE;
                advance_count = 2;
            } else {
                type = TokenType::NOT;
            }
            break;
        case '&':
            if (_pos + 1 < _src_code.length()) {
                char next_ch = _src_code[_pos + 1];
                if (next_ch == '&') {
                    type = TokenType::AND_AND;
                    advance_count = 2;
                } else if (next_ch == '=') {
                    type = TokenType::AND_EQ;
                    advance_count = 2;
                }
            } else {
                type = TokenType::AND;
            }
            break;
        case '|':
            if (_pos + 1 < _src_code.length()) {
                char next_ch = _src_code[_pos + 1];
                if (next_ch == '|') {
                    type = TokenType::OR_OR;
                    advance_count = 2;
                } else if (next_ch == '=') {
                    type = TokenType::OR_EQ;
                    advance_count = 2;
                }
            } else {
                type = TokenType::OR;
            }
            break;
        case '<':
            if (_pos + 1 < _src_code.length()) {
                char next_ch = _src_code[_pos + 1];
                if (next_ch == '<') {
                    if (_pos + 2 < _src_code.length() && _src_code[_pos + 2] == '=') {
                        type = TokenType::SHL_EQ;
                        advance_count = 3;
                    } else {
                        type = TokenType::SHL;
                        advance_count = 2;
                    }
                } else if (next_ch == '=') {
                    type = TokenType::LE;
                    advance_count = 2;
                }
            } else {
                type = TokenType::LT;
            }
            break;
        case '>':
            if (_pos + 1 < _src_code.length()) {
                char next_ch = _src_code[_pos + 1];
                if (next_ch == '>') {
                    if (_pos + 2 < _src_code.length() && _src_code[_pos + 2] == '=') {
                        type = TokenType::SHR_EQ;
                        advance_count = 3;
                    } else {
                        type = TokenType::SHR;
                        advance_count = 2;
                    }
                } else if (next_ch == '=') {
                    type = TokenType::GE;
                    advance_count = 2;
                }
            } else {
                type = TokenType::GT;
            }
            break;
        case '=':
            if (_pos + 1 < _src_code.length()) {
                char next_ch = _src_code[_pos + 1];
                if (next_ch == '=') {
                    type = TokenType::EQ_EQ;
                    advance_count = 2;
                } else if (next_ch == '>') {
                    type = TokenType::FAT_ARROW;
                    advance_count = 2;
                }
            } else {
                type = TokenType::EQ;
            }
            break;
        case '.':
            if (_pos + 1 < _src_code.length()) {
                char next_ch = _src_code[_pos + 1];
                if (next_ch == '.') {
                    if (_pos + 2 < _src_code.length() && _src_code[_pos + 2] == '.') {
                        type = TokenType::DOT_DOT_DOT;
                        advance_count = 3;
                    } else if (_pos + 2 < _src_code.length() && _src_code[_pos + 2] == '=') {
                        type = TokenType::DOT_DOT_EQ;
                        advance_count = 3;
                    } else {
                        type = TokenType::DOT_DOT;
                        advance_count = 2;
                    }
                }
            } else {
                type = TokenType::DOT;
            }
            break;
        case ':':
            if (_pos + 1 < _src_code.length() && _src_code[_pos + 1] == ':') {
                type = TokenType::PATH_SEP;
                advance_count = 2;
            } else {
                type = TokenType::COLON;
            }
            break;
        case '#': type = TokenType::POUND; break;
        case '$': type = TokenType::DOLLAR; break;
        case '?': type = TokenType::QUESTION; break;
        case '~': type = TokenType::TILDE; break;
        case '@': type = TokenType::AT; break;
        case ',': type = TokenType::COMMA; break;
        case ';': type = TokenType::SEMI; break;
        case '_': type = TokenType::UNDERSCORE; break;
        case '(': type = TokenType::L_PARENTHESES; break;
        case ')': type = TokenType::R_PARENTHESES; break;
        case '[': type = TokenType::L_SQUARE_BRACKET; break;
        case ']': type = TokenType::R_SQUARE_BRACKET; break;
        case '{': type = TokenType::L_CURLY_BRACE; break;
        case '}': type = TokenType::R_CURLY_BRACE; break;
    }

    if (type != TokenType::INVALID) {
        std::string_view lexeme = _src_code.substr(_pos, advance_count);
        _tokens.emplace_back(type, lexeme, _row, _col);
        
        for (std::size_t i = 0; i < advance_count; ++i) {
            advance_one();
        }
        
        return true;
    }

    return false;
}
```

```
bool Lexer::tokenize_keyword_identifier() {

}

这个又该怎么写？


在 Lexer 中解析关键字 (keyword) 和标识符 (identifier) 需要一个高效且准确的策略。由于关键字实际上是预先定义的标识符，最好的方法是：

先解析标识符：用一个简单的规则来匹配任何符合标识符语法的字符串（例如，以字母或下划线开头，后面跟着字母、数字或下划线）。

再检查是否为关键字：将匹配到的字符串与一个预定义的关键字列表进行比较。如果字符串在列表中，它就是一个关键字；否则，它就是一个标识符。

这种方法避免了为每个关键字编写单独的规则，并能有效地处理关键字和标识符之间的模糊性。

实现策略
匹配标识符：使用 isalpha() 和 isdigit() 等函数，或者一个简单的正则表达式，来逐个字符地扫描，直到遇到不符合标识符规则的字符。

关键字列表：使用一个 std::unordered_map 或 std::map 来存储关键字，这能提供快速的查找。unordered_map 通常具有更好的平均性能。

代码实现
以下是 tokenize_keyword_identifier() 函数的完整实现，它结合了上述策略，并使用了你提供的 TokenType 枚举。

C++

bool Lexer::tokenize_keyword_identifier() {
    if (_pos >= _src_code.length() || !isalpha(_src_code[_pos]) && _src_code[_pos] != '_') {
        // 如果当前字符不是字母或下划线，则不可能是标识符或关键字
        return false;
    }
    
    auto start_pos = _pos;
    
    // 1. 匹配整个标识符
    //    标识符由字母、数字或下划线组成
    while (_pos < _src_code.length() && (isalnum(_src_code[_pos]) || _src_code[_pos] == '_')) {
        advance_one();
    }
    
    // 2. 提取匹配到的字符串
    std::string_view lexeme = _src_code.substr(start_pos, _pos - start_pos);
    
    // 3. 检查它是否是一个关键字
    //    使用一个静态的unordered_map来存储关键字，只初始化一次
    static const std::unordered_map<std::string_view, TokenType> keywords = {
        {"as", TokenType::AS}, {"break", TokenType::BREAK}, {"const", TokenType::CONST},
        {"continue", TokenType::CONTINUE}, {"crate", TokenType::CRATE}, {"else", TokenType::ELSE},
        {"enum", TokenType::ENUM}, {"extern", TokenType::EXTERN}, {"false", TokenType::FALSE},
        {"fn", TokenType::FN}, {"for", TokenType::FOR}, {"if", TokenType::IF},
        {"impl", TokenType::IMPL}, {"in", TokenType::IN}, {"let", TokenType::LET},
        {"loop", TokenType::LOOP}, {"match", TokenType::MATCH}, {"mod", TokenType::MOD},
        {"move", TokenType::MOVE}, {"mut", TokenType::MUT}, {"pub", TokenType::PUB},
        {"ref", TokenType::REF}, {"return", TokenType::RETURN}, {"self", TokenType::SELF_OBJECT},
        {"Self", TokenType::SELF_TYPE}, {"static", TokenType::STATIC}, {"struct", TokenType::STRUCT},
        {"super", TokenType::SUPER}, {"trait", TokenType::TRAIT}, {"true", TokenType::TRUE},
        {"type", TokenType::TYPE}, {"unsafe", TokenType::UNSAFE}, {"use", TokenType::USE},
        {"where", TokenType::WHERE}, {"while", TokenType::WHILE}, {"async", TokenType::ASYNC},
        {"await", TokenType::AWAIT}, {"dyn", TokenType::DYN},
    };

    // 4. 在关键字列表中查找
    if (auto it = keywords.find(lexeme); it != keywords.end()) {
        // 如果找到，它是一个关键字
        _tokens.emplace_back(it->second, lexeme, _row, _col);
    } else {
        // 如果没找到，它是一个普通标识符
        _tokens.emplace_back(TokenType::IDENTIFIER, lexeme, _row, _col);
    }
    
    return true;
}
代码分析
入口检查：函数首先检查当前字符是否是标识符的合法开头，如果不是，就立即返回 false，从而避免不必要的处理。

扫描逻辑：一个简单的 while 循环就能高效地扫描整个标识符。

std::unordered_map：static const 修饰符确保 keywords 映射只被初始化一次，这在多次调用函数时非常高效。我们使用 std::string_view 作为键，这避免了在查找时创建新的字符串对象，进一步提升了性能。

查找与分类：keywords.find(lexeme) 是一个高效的查找操作。如果查找成功，it->second 会给出正确的关键字 TokenType；如果失败，则默认为 IDENTIFIER。

Token 创建：_tokens.emplace_back() 负责将新 Token 添加到列表中。

这个函数实现了清晰、高效且正确的关键字和标识符解析逻辑，是 Lexer 的核心组成部分之一。
```