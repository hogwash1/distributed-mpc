// expr_parser.cpp — 递归下降 JSON 解析器，零外部依赖

#include "expr_parser.h"
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <set>

namespace mpc {

// 极简 JSON 词法分析器
struct JsonTok {
    enum Type { OBJ_START, OBJ_END, STRING, NUMBER, COLON, COMMA, END };
    Type type; std::string str_val; double num_val = 0;
};

class JsonScanner {
    const char* p, *end; JsonTok next; bool has_next = false;
public:
    JsonScanner(const std::string& s) : p(s.c_str()), end(p + s.size()) {}
    JsonTok peek() { if (!has_next) { next = read(); has_next = true; } return next; }
    JsonTok advance() { auto t = peek(); has_next = false; return t; }
    void expect(JsonTok::Type t) { auto tok = advance(); if (tok.type != t) throw ExprParseError("JSON token 类型不匹配"); }
private:
    void skip_ws() { while (p < end && (*p==' '||*p=='\t'||*p=='\n'||*p=='\r')) ++p; }
    JsonTok read() {
        skip_ws();
        if (p >= end) return {JsonTok::END};
        switch (*p) {
        case '{': ++p; return {JsonTok::OBJ_START};
        case '}': ++p; return {JsonTok::OBJ_END};
        case ':': ++p; return {JsonTok::COLON};
        case ',': ++p; return {JsonTok::COMMA};
        case '"': { ++p; const char* s = p;
            while (p < end && *p != '"') { if (*p == '\\') ++p; ++p; }
            std::string val(s, p - s); if (p < end) ++p; return {JsonTok::STRING, val}; }
        default: {
            if ((*p>='0'&&*p<='9') || *p=='-') {
                char* ep; double v = std::strtod(p, &ep);
                JsonTok tok{JsonTok::NUMBER}; tok.num_val = v; p = ep; return tok;
            }
            while (p < end && *p!=',' && *p!='}' && *p!=' ') ++p;
            return {JsonTok::END};
        }}
    }
};

// JSON 对象 → AST 节点
static std::shared_ptr<AstNode> parse_obj(JsonScanner& s) {
    s.expect(JsonTok::OBJ_START);
    if (s.peek().type == JsonTok::OBJ_END) { s.advance(); return nullptr; }
    std::string op_str; double val = 0; bool has_val = false;
    int party = 0; bool has_party = false;
    std::shared_ptr<AstNode> lhs, rhs;
    for (bool first = true; ; first = false) {
        if (s.peek().type == JsonTok::OBJ_END) { s.advance(); break; }
        if (!first) s.expect(JsonTok::COMMA);
        std::string key = s.advance().str_val; s.expect(JsonTok::COLON);
        if (key == "op") op_str = s.advance().str_val;
        else if (key == "value" || key == "const_value") {
            auto tok = s.advance();
            if (tok.type == JsonTok::STRING) val = std::strtod(tok.str_val.c_str(), nullptr);
            else if (tok.type == JsonTok::NUMBER) val = tok.num_val;
            else throw ExprParseError("键 '" + key + "' 期望数字");
            has_val = true;
        } else if (key == "party") {
            auto tok = s.advance();
            if (tok.type == JsonTok::NUMBER) party = (int)tok.num_val;
            else if (tok.type == JsonTok::STRING) party = (int)std::strtod(tok.str_val.c_str(), nullptr);
            else throw ExprParseError("键 'party' 期望数字");
            has_party = true;
        } else if (key == "lhs") lhs = parse_obj(s);
        else if (key == "rhs") rhs = parse_obj(s);
        else { auto tok = s.advance(); if (tok.type == JsonTok::OBJ_START) { int d = 1; while (d > 0) { auto t = s.advance(); if (t.type == JsonTok::OBJ_START) d++; if (t.type == JsonTok::OBJ_END) d--; if (t.type == JsonTok::END) break; }}}
    }
    if (op_str.empty()) throw ExprParseError("缺少 'op' 字段");
    if (op_str == "add") { if (!lhs || !rhs) throw ExprParseError("'add' 需要 lhs 和 rhs"); return AstNode::make_binary(OpType::ADD, lhs, rhs); }
    if (op_str == "sub") { if (!lhs || !rhs) throw ExprParseError("'sub' 需要 lhs 和 rhs"); return AstNode::make_binary(OpType::SUB, lhs, rhs); }
    if (op_str == "mul") { if (!lhs || !rhs) throw ExprParseError("'mul' 需要 lhs 和 rhs"); return AstNode::make_binary(OpType::MUL, lhs, rhs); }
    if (op_str == "negate") { if (!lhs) throw ExprParseError("'negate' 需要 lhs"); return AstNode::make_unary(OpType::NEGATE, lhs); }
    if (op_str == "const") { if (!has_val) throw ExprParseError("'const' 需要 value"); return AstNode::make_const(val); }
    if (op_str == "var") { if (!has_party) throw ExprParseError("'var' 需要 party"); return AstNode::make_var(party); }
    if (op_str == "div_const") { if (!lhs) throw ExprParseError("'div_const' 需要 lhs"); if (!has_val) throw ExprParseError("'div_const' 需要 value"); return AstNode::make_div_const(lhs, val); }
    throw ExprParseError("不支持的操作: '" + op_str + "'");
}

std::shared_ptr<AstNode> ExprParser::parse_json(const std::string& json) {
    if (json.empty()) throw ExprParseError("JSON 字符串为空");
    JsonScanner s(json); auto node = parse_obj(s);
    if (!node) throw ExprParseError("解析 JSON 得到空 AST");
    return node;
}

std::string ExprParser::to_json(const std::shared_ptr<AstNode>& node) {
    if (!node) return "null";
    std::ostringstream oss; oss << "{";
    switch (node->op) {
    case OpType::ADD: oss<<"\"op\":\"add\",\"lhs\":"<<to_json(node->lhs)<<",\"rhs\":"<<to_json(node->rhs); break;
    case OpType::SUB: oss<<"\"op\":\"sub\",\"lhs\":"<<to_json(node->lhs)<<",\"rhs\":"<<to_json(node->rhs); break;
    case OpType::MUL: oss<<"\"op\":\"mul\",\"lhs\":"<<to_json(node->lhs)<<",\"rhs\":"<<to_json(node->rhs); break;
    case OpType::NEGATE: oss<<"\"op\":\"negate\",\"lhs\":"<<to_json(node->lhs); break;
    case OpType::CONST: oss<<"\"op\":\"const\",\"value\":"<<node->const_value; break;
    case OpType::VAR: oss<<"\"op\":\"var\",\"party\":"<<node->var_party_id; break;
    case OpType::DIV_CONST: oss<<"\"op\":\"div_const\",\"lhs\":"<<to_json(node->lhs)<<",\"value\":"<<node->const_value; break;
    default: break;
    } oss << "}"; return oss.str();
}

static std::string validate_impl(const std::shared_ptr<AstNode>& node, int num_parties) {
    if (!node) return "空 AST 节点";
    switch (node->op) {
    case OpType::ADD: case OpType::SUB: case OpType::MUL:
        if (!node->lhs || !node->rhs) return "二元操作缺少子节点";
        { auto e = validate_impl(node->lhs, num_parties); if (!e.empty()) return e; return validate_impl(node->rhs, num_parties); }
    case OpType::NEGATE: case OpType::DIV_CONST:
        if (!node->lhs) return "一元操作缺少 lhs"; return validate_impl(node->lhs, num_parties);
    case OpType::CONST: return "";
    case OpType::VAR:
        if (node->var_party_id < 1 || node->var_party_id > num_parties) {
            std::ostringstream oss; oss << "var 参与方编号=" << node->var_party_id << " 超出范围 [1," << num_parties << "]"; return oss.str();
        } return "";
    default: return "不支持的 OpType";
    }
}
std::string ExprParser::validate(const std::shared_ptr<AstNode>& node, int num_parties) { return validate_impl(node, num_parties); }

int ExprParser::compute_depth(const std::shared_ptr<AstNode>& node) {
    if (!node) return 0;
    switch (node->op) {
    case OpType::ADD: case OpType::SUB: return std::max(compute_depth(node->lhs), compute_depth(node->rhs));
    case OpType::MUL: return 1 + std::max(compute_depth(node->lhs), compute_depth(node->rhs));
    case OpType::NEGATE: case OpType::DIV_CONST: return compute_depth(node->lhs);
    default: return 0;
    }
}

static void collect_vars_impl(const std::shared_ptr<AstNode>& node, std::set<int32_t>& out) {
    if (!node) return;
    if (node->op == OpType::VAR) out.insert(node->var_party_id);
    if (node->lhs) collect_vars_impl(node->lhs, out);
    if (node->rhs) collect_vars_impl(node->rhs, out);
}
std::vector<int32_t> ExprParser::collect_vars(const std::shared_ptr<AstNode>& node) {
    std::set<int32_t> s; collect_vars_impl(node, s); return std::vector<int32_t>(s.begin(), s.end());
}

} // namespace mpc
