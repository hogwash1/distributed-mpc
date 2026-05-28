#include "expr_parser.h"
#include "nlohmann/json.hpp" // 引入 JSON 库
#include <set>
#include <algorithm>

using json = nlohmann::json;

namespace mpc {

// ======================= Day 1: 解析与反序列化 =======================

static std::shared_ptr<AstNode> parse_node(const json& j) {
    if (!j.contains("op")) {
        throw ExprParseError("Missing 'op' field in JSON node.");
    }

    std::string op = j["op"];
    auto node = std::make_shared<AstNode>();

    if (op == "add") {
        node->type = OpType::ADD;
        node->left = parse_node(j.at("lhs"));
        node->right = parse_node(j.at("rhs"));
    } else if (op == "sub") {
        node->type = OpType::SUB;
        node->left = parse_node(j.at("lhs"));
        node->right = parse_node(j.at("rhs"));
    } else if (op == "mul") {
        node->type = OpType::MUL;
        node->left = parse_node(j.at("lhs"));
        node->right = parse_node(j.at("rhs"));
    } else if (op == "negate") {
        node->type = OpType::NEGATE;
        node->left = parse_node(j.at("lhs"));
    } else if (op == "const") {
        node->type = OpType::CONST;
        node->value = j.at("value");
    } else if (op == "var") {
        node->type = OpType::VAR;
        node->party_id = j.at("party");
    } else if (op == "div_const") {
        node->type = OpType::DIV_CONST;
        node->left = parse_node(j.at("lhs"));
        // 根据 README 伪代码，把常量作为一个右子节点挂载
        auto const_node = std::make_shared<AstNode>();
        const_node->type = OpType::CONST;
        const_node->value = j.at("value");
        node->right = const_node;
    } else {
        throw ExprParseError("Unsupported op type: " + op);
    }

    return node;
}

std::shared_ptr<AstNode> ExprParser::parse_json(const std::string& json_str) {
    try {
        json j = json::parse(json_str);
        return parse_node(j);
    } catch (const json::exception& e) {
        throw ExprParseError(std::string("JSON parse error: ") + e.what());
    }
}

static json node_to_json(const std::shared_ptr<AstNode>& node) {
    if (!node) return json{};

    json j;
    switch (node->type) {
        case OpType::ADD:
            j["op"] = "add";
            j["lhs"] = node_to_json(node->left);
            j["rhs"] = node_to_json(node->right);
            break;
        case OpType::SUB:
            j["op"] = "sub";
            j["lhs"] = node_to_json(node->left);
            j["rhs"] = node_to_json(node->right);
            break;
        case OpType::MUL:
            j["op"] = "mul";
            j["lhs"] = node_to_json(node->left);
            j["rhs"] = node_to_json(node->right);
            break;
        case OpType::NEGATE:
            j["op"] = "negate";
            j["lhs"] = node_to_json(node->left);
            break;
        case OpType::CONST:
            j["op"] = "const";
            j["value"] = node->value;
            break;
        case OpType::VAR:
            j["op"] = "var";
            j["party"] = node->party_id;
            break;
        case OpType::DIV_CONST:
            j["op"] = "div_const";
            j["lhs"] = node_to_json(node->left);
            if (node->right) j["value"] = node->right->value;
            break;
    }
    return j;
}

std::string ExprParser::to_json(const std::shared_ptr<AstNode>& node) {
    if (!node) return "{}";
    return node_to_json(node).dump(); // 紧凑输出，无缩进
}

// ======================= Day 2: 验证与属性计算 =======================

bool ExprParser::validate(const std::shared_ptr<AstNode>& node, int num_parties) {
    if (!node) return false;

    switch (node->type) {
        case OpType::ADD:
        case OpType::SUB:
        case OpType::MUL:
            return node->left && node->right &&
                   validate(node->left, num_parties) &&
                   validate(node->right, num_parties);
        case OpType::NEGATE:
            return node->left && validate(node->left, num_parties);
        case OpType::DIV_CONST:
            // 必须有左子树，且除数不能为0（右子树必须是常量节点且值非0）
            return node->left && node->right &&
                   validate(node->left, num_parties) &&
                   node->right->type == OpType::CONST &&
                   node->right->value != 0.0;
        case OpType::CONST:
            return true;
        case OpType::VAR:
            return node->party_id >= 1 && node->party_id <= num_parties;
        default:
            return false;
    }
}

int ExprParser::compute_depth(const std::shared_ptr<AstNode>& node) {
    if (!node) return 0;

    int left_depth = compute_depth(node->left);
    int right_depth = compute_depth(node->right);
    int max_child_depth = std::max(left_depth, right_depth);

    // 同态加密中，密文与密文相乘才会增加乘法深度。
    // 如果是与明文常量相乘 (CONST)，深度不变。
    if (node->type == OpType::MUL) {
        bool left_is_const = (node->left && node->left->type == OpType::CONST);
        bool right_is_const = (node->right && node->right->type == OpType::CONST);
        if (!left_is_const && !right_is_const) {
            return max_child_depth + 1;
        }
    }
    return max_child_depth;
}

static void collect_vars_recursive(const std::shared_ptr<AstNode>& node, std::set<int32_t>& vars) {
    if (!node) return;
    if (node->type == OpType::VAR) {
        vars.insert(node->party_id);
    }
    collect_vars_recursive(node->left, vars);
    collect_vars_recursive(node->right, vars);
}

std::vector<int32_t> ExprParser::collect_vars(const std::shared_ptr<AstNode>& node) {
    std::set<int32_t> vars_set;
    collect_vars_recursive(node, vars_set);
    return std::vector<int32_t>(vars_set.begin(), vars_set.end());
}

} // namespace mpc