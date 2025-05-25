#include "tokenizer.h"
#include <cctype>
#include <algorithm>

Tokenizer::Tokenizer() {
    initKeywords();
    initOperators();
    initPunctuation();
}

void Tokenizer::initKeywords() {
    std::vector<std::string> keywordList = {
        "select", "insert", "delete", "update", "create", "drop", "alter", 
        "table", "database", "where", "from", "use", "into", "values",
        "set", "begin", "commit", "rollback", "transaction", "show",
        "tables", "rename", "to", "id", "int", "varchar"
    };
    
    for (const auto& keyword : keywordList) {
        keywords[keyword] = TokenType::KEYWORD;
    }
}

void Tokenizer::initOperators() {
    operators['='] = TokenType::OPERATOR;
    operators['<'] = TokenType::OPERATOR;
    operators['>'] = TokenType::OPERATOR;
    operators['!'] = TokenType::OPERATOR;
}

void Tokenizer::initPunctuation() {
    punctuation[','] = TokenType::PUNCTUATION;
    punctuation[';'] = TokenType::PUNCTUATION;
    punctuation['('] = TokenType::PUNCTUATION;
    punctuation[')'] = TokenType::PUNCTUATION;
    punctuation['.'] = TokenType::PUNCTUATION;
}

bool Tokenizer::isWhitespace(char c) const {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

bool Tokenizer::isAlpha(char c) const {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

bool Tokenizer::isDigit(char c) const {
    return c >= '0' && c <= '9';
}

bool Tokenizer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

Token Tokenizer::extractKeywordOrIdentifier(const std::string& input, size_t& pos) {
    size_t start = pos;
    while (pos < input.length() && isAlphaNumeric(input[pos])) {
        pos++;
    }
    
    std::string value = input.substr(start, pos - start);
    std::string lowerValue = value;
    std::transform(lowerValue.begin(), lowerValue.end(), lowerValue.begin(), ::tolower);
    
    if (keywords.find(lowerValue) != keywords.end()) {
        return Token(TokenType::KEYWORD, lowerValue, start);
    }
    
    return Token(TokenType::IDENTIFIER, value, start);
}

Token Tokenizer::extractNumber(const std::string& input, size_t& pos) {
    size_t start = pos;
    while (pos < input.length() && isDigit(input[pos])) {
        pos++;
    }
    
    // Handle decimal numbers
    if (pos < input.length() && input[pos] == '.' && pos + 1 < input.length() && isDigit(input[pos + 1])) {
        pos++; // Skip the decimal point
        
        while (pos < input.length() && isDigit(input[pos])) {
            pos++;
        }
    }
    
    return Token(TokenType::NUMBER_LITERAL, input.substr(start, pos - start), start);
}

Token Tokenizer::extractString(const std::string& input, size_t& pos) {
    size_t start = pos;
    char quoteChar = input[pos];
    pos++; // Skip the opening quote
    
    std::string value;
    while (pos < input.length() && input[pos] != quoteChar) {
        // Handle escaped characters
        if (input[pos] == '\\' && pos + 1 < input.length()) {
            pos++;
        }
        value += input[pos];
        pos++;
    }
    
    if (pos < input.length()) {
        pos++; // Skip the closing quote
    }
    
    return Token(TokenType::STRING_LITERAL, value, start);
}

Token Tokenizer::extractOperator(const std::string& input, size_t& pos) {
    size_t start = pos;
    char c = input[pos];
    pos++;
    
    // Handle two-character operators (e.g., <=, >=, !=)
    if (c == '<' || c == '>' || c == '!') {
        if (pos < input.length() && input[pos] == '=') {
            pos++;
            return Token(TokenType::OPERATOR, input.substr(start, 2), start);
        }
    }
    
    return Token(TokenType::OPERATOR, input.substr(start, 1), start);
}

Token Tokenizer::extractPunctuation(const std::string& input, size_t& pos) {
    size_t start = pos;
    pos++;
    return Token(TokenType::PUNCTUATION, input.substr(start, 1), start);
}

std::vector<Token> Tokenizer::tokenize(const std::string& input) {
    std::vector<Token> tokens;
    size_t pos = 0;
    
    while (pos < input.length()) {
        char c = input[pos];
        
        if (isWhitespace(c)) {
            // Skip whitespace
            pos++;
        }
        else if (isAlpha(c)) {
            // Keywords or identifiers
            tokens.push_back(extractKeywordOrIdentifier(input, pos));
        }
        else if (isDigit(c)) {
            // Numbers
            tokens.push_back(extractNumber(input, pos));
        }
        else if (c == '\'' || c == '"') {
            // String literals
            tokens.push_back(extractString(input, pos));
        }
        else if (operators.find(c) != operators.end()) {
            // Operators
            tokens.push_back(extractOperator(input, pos));
        }
        else if (punctuation.find(c) != punctuation.end()) {
            // Punctuation
            tokens.push_back(extractPunctuation(input, pos));
        }
        else {
            // Unrecognized character
            tokens.push_back(Token(TokenType::UNKNOWN, std::string(1, c), pos));
            pos++;
        }
    }
    
    // Add end-of-input token
    tokens.push_back(Token(TokenType::END_OF_INPUT, "", input.length()));
    
    return tokens;
}