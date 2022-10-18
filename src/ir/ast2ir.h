//
// Created by Mike Smith on 2022/10/17.
//

#include <bindings.hpp>

#include <ast/variable.h>
#include <ast/expression.h>
#include <ast/statement.h>
#include <ast/function.h>

namespace luisa::compute {

namespace detail {
class FunctionBuilder;
}

class AST2IR {

private:
    luisa::unordered_map<uint64_t, ir::NodeRef> _constants;
    luisa::unordered_map<uint64_t, ir::NodeRef> _variables;
    luisa::vector<luisa::unique_ptr<ir::IrBuilder>> _builder_stack;

private:
    [[nodiscard]] ir::IrBuilder *_current_builder() noexcept;
    [[nodiscard]] const ir::Type *_convert(const Type *type) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ConstantData &c) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const UnaryExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const BinaryExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const MemberExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const AccessExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const LiteralExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const RefExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ConstantExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const CallExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const CastExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const PhiExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const CpuCustomOpExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const GpuCustomOpExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ReplaceMemberExpr *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert_expr(const Expression *expr) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const BreakStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ContinueStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ReturnStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ScopeStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const IfStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const LoopStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ExprStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const SwitchStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const SwitchCaseStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const SwitchDefaultStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const AssignStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const ForStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert(const CommentStmt *stmt) noexcept;
    [[nodiscard]] ir::NodeRef _convert_stmt(const Statement *stmt) noexcept;

public:
    AST2IR() noexcept = default;
    [[nodiscard]] ir::Module convert(Function function) noexcept;
};

}// namespace luisa::compute
