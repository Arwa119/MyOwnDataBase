#ifndef TOKENIZER_H
#define TOKENIZER_H

#include <string>
#include <vector>
#include <unordered_map>

enum class TokenType {
    KEYWORD,      // SQL keywords (SELECT, INSERT, CREATE, etc.)
    IDENTIFIER,   // Table/column names
    STRING_LITERAL, // String values in quotes
    NUMBER_LITERAL, // Numeric values
    OPERATOR,     // =, <, >, etc.
    PUNCTUATION,  // Comma, parentheses, semicolon
    END_OF_INPUT, // End of the input string
    UNKNOWN       // Unrecognized token
};

struct Token {
    TokenType type;
    std::string value;
    size_t position;  // Position in the input string

    Token(TokenType t, const std::string& v, size_t pos)
        : type(t), value(v), position(pos) {}
};

class Tokenizer {
public:
    Tokenizer();
    std::vector<Token> tokenize(const std::string& input);

private:
    std::unordered_map<std::string, TokenType> keywords;
    std::unordered_map<char, TokenType> operators;
    std::unordered_map<char, TokenType> punctuation;

    void initKeywords();
    void initOperators();
    void initPunctuation();
    bool isWhitespace(char c) const;
    bool isAlpha(char c) const;
    bool isDigit(char c) const;
    bool isAlphaNumeric(char c) const;

    Token extractKeywordOrIdentifier(const std::string& input, size_t& pos);
    Token extractNumber(const std::string& input, size_t& pos);
    Token extractString(const std::string& input, size_t& pos);
    Token extractOperator(const std::string& input, size_t& pos);
    Token extractPunctuation(const std::string& input, size_t& pos);
};

#endif // TOKENIZER_H