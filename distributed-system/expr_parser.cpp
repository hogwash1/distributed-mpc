// expr_parser.cpp -- Expression Parser Implementation
// Task 02: recursive-descent JSON parser + AST helpers
// Uses a minimal built-in JSON tokenizer — no external dependencies.

#include "expr_parser.h"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <set>

namespace mpc {

// ===================================================================
// Minimal JSON Scanner
// ===================================================================
struct JsonTok {
    enum Type { OBJECT_START, OBJECT_END, STRING, NUMBER, COLON, COMMA, END };
    Type type;
    std::string str_val;
    double num_val = 0;
};

class JsonScanner {
    const char* p;
    const char* end;
    JsonTok next;
    bool has_next = false;
public:
    explicit JsonScanner(const std::string& s) : p(s.c_str()), end(p + s.size()) {}

    JsonTok peek() {
        if (!has_next) { next = read(); has_next = true; }
        return next;
    }
    JsonTok advance() {
        auto t = peek(); has_next = false; return t;
    }
    void expect(JsonTok::Type t) {
        auto tok = advance();
        if (tok.type != t) throw ExprParseError("expected token type " + std::to_string(t));
    }
private:
    void skip_ws() { while (p < end && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) ++p; }
    JsonTok read() {
        skip_ws();
        if (p >= end) return {JsonTok::END};
        switch (*p) {
        case '{': ++p; return {JsonTok::OBJECT_START};
        case '}': ++p; return {JsonTok::OBJECT_END};
        case ':': ++p; return {JsonTok::COLON};
        case ',': ++p; return {JsonTok::COMMA};
        case '"': {  // string
            ++p; const char* s = p;
            while (p < end && *p != '"') {
                if (*p == '\\') ++p;
                ++p;
            }
            std::string val(s, p - s);
            if (p < end) ++p; // skip closing quote
            return {JsonTok::STRING, val};
        }
        default: {   // number (or null/true/false)
            if ((*p>='0'&&*p<='9') || *p=='-') {
                char* ep;
                double v = std::strtod(p, &ep);
                JsonTok tok{JsonTok::NUMBER};
                tok.num_val = v;
                p = ep;
                return tok;
            }
            // skip unknown tokens (true/false/null — not needed)
            while (p < end && *p!=',' && *p!='}' && *p!=' ') ++p;
            return {JsonTok::END};
        }
        }
    }
};

// ===================================================================
// Recursive JSON → AST parser
// ===================================================================
static std::shared_ptr<AstNode> parse_obj(JsonScanner& s) {
    s.expect(JsonTok::OBJECT_START);
    auto tok = s.peek();
    if (tok.type == JsonTok::OBJECT_END) {
        s.advance();
        return nullptr; // empty object
    }
    if (tok.type != JsonTok::STRING)
        throw ExprParseError("expected key string in JSON object");

    // Read key-value pairs
    std::string op_str;
    double val = 0; bool has_val = false;
    int party = 0; bool has_party = false;
    std::shared_ptr<AstNode> lhs, rhs;

    bool first = true;
    while (true) {
        tok = s.peek();
        if (tok.type == JsonTok::OBJECT_END) { s.advance(); break; }
        if (!first) { s.expect(JsonTok::COMMA); }
        first = false;

        std::string key = s.advance().str_val;
        s.expect(JsonTok::COLON);

        if (key == "op") {
            op_str = s.advance().str_val;
        } else if (key == "value" || key == "const_value") {
            tok = s.advance();
            // value can be string that looks like a number? or number
            if (tok.type == JsonTok::STRING) {
                val = std::strtod(tok.str_val.c_str(), nullptr);
            } else if (tok.type == JsonTok::NUMBER) {
                val = tok.num_val;
            } else {
                throw ExprParseError("expected number for key '" + key + "'");
            }
            has_val = true;
        } else if (key == "party") {
            tok = s.advance();
            if (tok.type == JsonTok::NUMBER) {
                party = (int)tok.num_val;
            } else if (tok.type == JsonTok::STRING) {
                party = (int)std::strtod(tok.str_val.c_str(), nullptr);
            } else {
                throw ExprParseError("expected number for key 'party'");
            }
            has_party = true;
        } else if (key == "lhs") {
            lhs = parse_obj(s);
        } else if (key == "rhs") {
            rhs = parse_obj(s);
        } else {
            // unknown key - skip value
            tok = s.advance();
            if (tok.type == JsonTok::OBJECT_START) {
                int depth = 1;
                while (depth > 0) {
                    auto t = s.advance();
                    if (t.type == JsonTok::OBJECT_START) depth++;
                    if (t.type == JsonTok::OBJECT_END) depth--;
                    if (t.type == JsonTok::END) break;
                }
            }
        }
    }

    // Construct AST node based on op
    if (op_str.empty()) throw ExprParseError("missing 'op' field in JSON expression");

    if (op_str == "add") {
        if (!lhs || !rhs) throw ExprParseError("'add' requires lhs and rhs");
        return AstNode::make_binary(OpType::ADD, lhs, rhs);
    } else if (op_str == "sub") {
        if (!lhs || !rhs) throw ExprParseError("'sub' requires lhs and rhs");
        return AstNode::make_binary(OpType::SUB, lhs, rhs);
    } else if (op_str == "mul") {
        if (!lhs || !rhs) throw ExprParseError("'mul' requires lhs and rhs");
        return AstNode::make_binary(OpType::MUL, lhs, rhs);
    } else if (op_str == "negate") {
        if (!lhs) throw ExprParseError("'negate' requires lhs");
        return AstNode::make_unary(OpType::NEGATE, lhs);
    } else if (op_str == "const") {
        if (!has_val) throw ExprParseError("'const' requires value/const_value");
        return AstNode::make_const(val);
    } else if (op_str == "var") {
        if (!has_party) throw ExprParseError("'var' requires party field");
        return AstNode::make_var(party);
    } else if (op_str == "div_const") {
        if (!lhs) throw ExprParseError("'div_const' requires lhs");
        if (!has_val) throw ExprParseError("'div_const' requires value/const_value");
        return AstNode::make_div_const(lhs, val);
    } else {
        throw ExprParseError("Unknown op: '" + op_str + "'");
    }
}

// ===================================================================
// Public API
// ===================================================================
std::shared_ptr<AstNode> ExprParser::parse_json(const std::string& json) {
    if (json.empty()) throw ExprParseError("empty JSON string");
    JsonScanner s(json);
    auto node = parse_obj(s);
    if (!node) throw ExprParseError("parsed null AST from JSON");
    return node;
}

// ===================================================================
// AST → JSON serializer
// ===================================================================
std::string ExprParser::to_json(const std::shared_ptr<AstNode>& node) {
    if (!node) return "null";

    std::ostringstream oss;
    oss << "{";
    switch (node->op) {
    case OpType::ADD:       oss << "\"op\":\"add\",\"lhs\":" << to_json(node->lhs) << ",\"rhs\":" << to_json(node->rhs); break;
    case OpType::SUB:       oss << "\"op\":\"sub\",\"lhs\":" << to_json(node->lhs) << ",\"rhs\":" << to_json(node->rhs); break;
    case OpType::MUL:       oss << "\"op\":\"mul\",\"lhs\":" << to_json(node->lhs) << ",\"rhs\":" << to_json(node->rhs); break;
    case OpType::NEGATE:    oss << "\"op\":\"negate\",\"lhs\":" << to_json(node->lhs); break;
    case OpType::CONST:     oss << "\"op\":\"const\",\"value\":" << node->const_value; break;
    case OpType::VAR:       oss << "\"op\":\"var\",\"party\":" << node->var_party_id; break;
    case OpType::DIV_CONST: oss << "\"op\":\"div_const\",\"lhs\":" << to_json(node->lhs) << ",\"value\":" << node->const_value; break;
    default: break;
    }
    oss << "}";
    return oss.str();
}

// ===================================================================
// Validation
// ===================================================================
static std::string validate_impl(const std::shared_ptr<AstNode>& node, int num_parties) {
    if (!node) return "null AST node";

    switch (node->op) {
    case OpType::ADD:
    case OpType::SUB:
    case OpType::MUL:
        if (!node->lhs || !node->rhs) return "binary op missing child node";
        {
            auto e = validate_impl(node->lhs, num_parties);
            if (!e.empty()) return e;
            return validate_impl(node->rhs, num_parties);
        }
    case OpType::NEGATE:
    case OpType::DIV_CONST:
        if (!node->lhs) return "unary op missing lhs";
        return validate_impl(node->lhs, num_parties);
    case OpType::CONST:
        return "";
    case OpType::VAR:
        if (node->var_party_id < 1 || node->var_party_id > num_parties) {
            std::ostringstream oss;
            oss << "var party_id=" << node->var_party_id
                << " out of range [1," << num_parties << "]";
            return oss.str();
        }
        return "";
    default:
        return "unknown OpType";
    }
}

std::string ExprParser::validate(const std::shared_ptr<AstNode>& node, int num_parties) {
    return validate_impl(node, num_parties);
}

// ===================================================================
// Compute multiplication depth
// ===================================================================
int ExprParser::compute_depth(const std::shared_ptr<AstNode>& node) {
    if (!node) return 0;
    switch (node->op) {
    case OpType::ADD:
    case OpType::SUB:
        return std::max(compute_depth(node->lhs), compute_depth(node->rhs));
    case OpType::MUL:
        return 1 + std::max(compute_depth(node->lhs), compute_depth(node->rhs));
    case OpType::NEGATE:
    case OpType::DIV_CONST:
        return compute_depth(node->lhs);
    case OpType::CONST:
    case OpType::VAR:
        return 0;
    default:
        return 0;
    }
}

// ===================================================================
// Collect unique variable IDs
// ===================================================================
static void collect_vars_impl(const std::shared_ptr<AstNode>& node, std::set<int32_t>& out) {
    if (!node) return;
    if (node->op == OpType::VAR) {
        out.insert(node->var_party_id);
    }
    if (node->lhs) collect_vars_impl(node->lhs, out);
    if (node->rhs) collect_vars_impl(node->rhs, out);
}

std::vector<int32_t> ExprParser::collect_vars(const std::shared_ptr<AstNode>& node) {
    std::set<int32_t> s;
    collect_vars_impl(node, s);
    return std::vector<int32_t>(s.begin(), s.end());
}

} // namespace mpc
