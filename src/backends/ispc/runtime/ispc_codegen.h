#pragma once

#include <vstl/Common.h>
#include <vstl/functional.h>
#include <ast/function.h>
#include <ast/expression.h>
#include <ast/statement.h>
using namespace luisa;
using namespace luisa::compute;
namespace lc::ispc {
class CodegenUtility {
public:
    static void GetCodegen(Function func, std::string &str, vstd::HashMap<uint, size_t> &varOffsets, size_t &cbufferSize);
    static void GetVariableName(Variable const &type, std::string &str);
    static void GetVariableName(Variable::Tag type, uint id, std::string &str);
    static void GetVariableName(Type::Tag type, uint id, std::string &str);
    static void GetTypeName(Type const &type, std::string &str, bool isWritable = false);
    static void GetBasicTypeName(size_t typeIndex, std::string& str);
    static std::string GetBasicTypeName(size_t typeIndex) {
        std::string s;
        GetBasicTypeName(typeIndex, s);
        return s;
    }
    static void GetFunctionDecl(Function func, std::string &str);
    static void GetFunctionName(CallExpr const *expr, std::string &result);

    static void ClearStructType();
    static void RegistStructType(Type const *type);
};
class StringExprVisitor final : public ExprVisitor {

public:
    void visit(const UnaryExpr *expr) override;
    void visit(const BinaryExpr *expr) override;
    void visit(const MemberExpr *expr) override;
    void visit(const AccessExpr *expr) override;
    void visit(const LiteralExpr *expr) override;
    void visit(const RefExpr *expr) override;
    void visit(const CallExpr *expr) override;
    void visit(const CastExpr *expr) override;
    void visit(const ConstantExpr *expr) override;
    StringExprVisitor(std::string &str);
    ~StringExprVisitor();

protected:
    std::string &str;
    size_t constCount = 0;
};
class StringStateVisitor final : public StmtVisitor {
public:
    void visit(const BreakStmt *) override;
    void visit(const ContinueStmt *) override;
    void visit(const ReturnStmt *) override;
    void visit(const ScopeStmt *) override;
    void visit(const IfStmt *) override;
    void visit(const LoopStmt *) override;
    void visit(const ExprStmt *) override;
    void visit(const SwitchStmt *) override;
    void visit(const SwitchCaseStmt *) override;
    void visit(const SwitchDefaultStmt *) override;
    void visit(const AssignStmt *) override;
    void visit(const ForStmt *) override;
    void visit(const CommentStmt *) override;
    StringStateVisitor(std::string &str);
    ~StringStateVisitor();

protected:
    std::string &str;
};
}// namespace lc::ispc