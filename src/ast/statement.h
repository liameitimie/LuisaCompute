//
// Created by Mike Smith on 2020/12/2.
//

#pragma once

#include <core/concepts.h>
#include <core/allocator.h>
#include <ast/variable.h>
#include <ast/expression.h>

namespace luisa::compute {

struct StmtVisitor;

class Statement : public concepts::Noncopyable {

public:
    enum struct Tag : uint32_t {
        BREAK,
        CONTINUE,
        RETURN,
        SCOPE,
        IF,
        LOOP,
        EXPR,
        SWITCH,
        SWITCH_CASE,
        SWITCH_DEFAULT,
        ASSIGN,
        FOR,
        COMMENT,
        META
    };

private:
    mutable uint64_t _hash{0u};
    mutable bool _hash_computed{false};
    Tag _tag;

private:
    [[nodiscard]] virtual uint64_t _compute_hash() const noexcept = 0;

public:
    explicit Statement(Tag tag) noexcept : _tag{tag} {}
    [[nodiscard]] auto tag() const noexcept { return _tag; }
    virtual void accept(StmtVisitor &) const = 0;
    virtual ~Statement() noexcept = default;
    [[nodiscard]] uint64_t hash() const noexcept;
};

struct BreakStmt;
struct ContinueStmt;

class ReturnStmt;

class ScopeStmt;
class IfStmt;
class LoopStmt;
class ExprStmt;
class SwitchStmt;
class SwitchCaseStmt;
class SwitchDefaultStmt;
class AssignStmt;
class ForStmt;
class CommentStmt;
class MetaStmt;

struct StmtVisitor {
    virtual void visit(const BreakStmt *) = 0;
    virtual void visit(const ContinueStmt *) = 0;
    virtual void visit(const ReturnStmt *) = 0;
    virtual void visit(const ScopeStmt *) = 0;
    virtual void visit(const IfStmt *) = 0;
    virtual void visit(const LoopStmt *) = 0;
    virtual void visit(const ExprStmt *) = 0;
    virtual void visit(const SwitchStmt *) = 0;
    virtual void visit(const SwitchCaseStmt *) = 0;
    virtual void visit(const SwitchDefaultStmt *) = 0;
    virtual void visit(const AssignStmt *) = 0;
    virtual void visit(const ForStmt *) = 0;
    virtual void visit(const CommentStmt *) = 0;
    virtual void visit(const MetaStmt *) = 0;
};

#define LUISA_MAKE_STATEMENT_ACCEPT_VISITOR() \
    void accept(StmtVisitor &visitor) const override { visitor.visit(this); }

class BreakStmt final : public Statement {

private:
    uint64_t _compute_hash() const noexcept override {
        return Hash64::default_seed;
    }

public:
    BreakStmt() noexcept : Statement{Tag::BREAK} {}
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class ContinueStmt : public Statement {

private:
    uint64_t _compute_hash() const noexcept override {
        return Hash64::default_seed;
    }

public:
    ContinueStmt() noexcept : Statement{Tag::CONTINUE} {}
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class ReturnStmt : public Statement {

private:
    const Expression *_expr;

private:
    uint64_t _compute_hash() const noexcept override {
        return hash64(_expr == nullptr ? 0ull : _expr->hash());
    }

public:
    explicit ReturnStmt(const Expression *expr) noexcept
        : Statement{Tag::RETURN}, _expr{expr} {
        if (_expr != nullptr) { _expr->mark(Usage::READ); }
    }
    [[nodiscard]] auto expression() const noexcept { return _expr; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class ScopeStmt : public Statement {

private:
    vector<const Statement *> _statements;

private:
    uint64_t _compute_hash() const noexcept override {
        return std::accumulate(
            _statements.cbegin(), _statements.cend(), Hash64::default_seed,
            [](auto seed, auto p) noexcept { return hash64(p->hash(), seed); });
    }

public:
    ScopeStmt() noexcept : Statement{Tag::SCOPE} {}
    [[nodiscard]] auto statements() const noexcept { return std::span{_statements}; }
    void append(const Statement *stmt) noexcept { _statements.emplace_back(stmt); }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class AssignStmt : public Statement {

private:
    const Expression *_lhs;
    const Expression *_rhs;
    AssignOp _op;

private:
    uint64_t _compute_hash() const noexcept override {
        return hash64(_op, hash64(_lhs->hash(), _rhs->hash()));
    }

public:
    AssignStmt(AssignOp op, const Expression *lhs, const Expression *rhs) noexcept
        : Statement{Tag::ASSIGN}, _lhs{lhs}, _rhs{rhs}, _op{op} {
        _lhs->mark(Usage::WRITE);
        _rhs->mark(Usage::READ);
    }

    [[nodiscard]] auto lhs() const noexcept { return _lhs; }
    [[nodiscard]] auto rhs() const noexcept { return _rhs; }
    [[nodiscard]] auto op() const noexcept { return _op; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class IfStmt : public Statement {

private:
    const Expression *_condition;
    ScopeStmt _true_branch;
    ScopeStmt _false_branch;

private:
    uint64_t _compute_hash() const noexcept override {
        return hash64(
            _condition->hash(),
            hash64(_true_branch.hash(),
                   _false_branch.hash()));
    }

public:
    explicit IfStmt(const Expression *cond) noexcept
        : Statement{Tag::IF},
          _condition{cond} {
        _condition->mark(Usage::READ);
    }
    [[nodiscard]] auto condition() const noexcept { return _condition; }
    [[nodiscard]] auto true_branch() noexcept { return &_true_branch; }
    [[nodiscard]] auto false_branch() noexcept { return &_false_branch; }
    [[nodiscard]] auto true_branch() const noexcept { return &_true_branch; }
    [[nodiscard]] auto false_branch() const noexcept { return &_false_branch; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class LoopStmt : public Statement {

private:
    ScopeStmt _body;

private:
    uint64_t _compute_hash() const noexcept override {
        return _body.hash();
    }

public:
    LoopStmt() noexcept : Statement{Tag::LOOP} {}
    [[nodiscard]] auto body() noexcept { return &_body; }
    [[nodiscard]] auto body() const noexcept { return &_body; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class ExprStmt : public Statement {

private:
    const Expression *_expr;

private:
    uint64_t _compute_hash() const noexcept override {
        return _expr->hash();
    }

public:
    explicit ExprStmt(const Expression *expr) noexcept
        : Statement{Tag::EXPR}, _expr{expr} {
        _expr->mark(Usage::READ);
    }
    [[nodiscard]] auto expression() const noexcept { return _expr; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class SwitchStmt : public Statement {

private:
    const Expression *_expr;
    ScopeStmt _body;

private:
    uint64_t _compute_hash() const noexcept override {
        return hash64(_body.hash(), _expr->hash());
    }

public:
    explicit SwitchStmt(const Expression *expr) noexcept
        : Statement{Tag::SWITCH},
          _expr{expr} {
        _expr->mark(Usage::READ);
    }
    [[nodiscard]] auto expression() const noexcept { return _expr; }
    [[nodiscard]] auto body() noexcept { return &_body; }
    [[nodiscard]] auto body() const noexcept { return &_body; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class SwitchCaseStmt : public Statement {

private:
    const Expression *_expr;
    ScopeStmt _body;

private:
    uint64_t _compute_hash() const noexcept override {
        return hash64(_body.hash(), _expr->hash());
    }

public:
    explicit SwitchCaseStmt(const Expression *expr) noexcept
        : Statement{Tag::SWITCH_CASE},
          _expr{expr} {
        _expr->mark(Usage::READ);
    }
    [[nodiscard]] auto expression() const noexcept { return _expr; }
    [[nodiscard]] auto body() noexcept { return &_body; }
    [[nodiscard]] auto body() const noexcept { return &_body; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class SwitchDefaultStmt : public Statement {

private:
    ScopeStmt _body;

private:
    uint64_t _compute_hash() const noexcept override {
        return _body.hash();
    }

public:
    SwitchDefaultStmt() noexcept : Statement{Tag::SWITCH_DEFAULT} {}
    [[nodiscard]] auto body() noexcept { return &_body; }
    [[nodiscard]] auto body() const noexcept { return &_body; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class ForStmt : public Statement {

private:
    const Expression *_var;
    const Expression *_cond;
    const Expression *_step;
    ScopeStmt _body;

private:
    uint64_t _compute_hash() const noexcept override {
        return hash64(_body.hash(), hash64(_var->hash(), hash64(_cond->hash(), _step->hash())));
    }

public:
    ForStmt(const Expression *var,
            const Expression *cond,
            const Expression *step) noexcept
        : Statement{Tag::FOR},
          _var{var}, _cond{cond}, _step{step} {
        _var->mark(Usage::READ_WRITE);
        _cond->mark(Usage::READ);
        _step->mark(Usage::READ);
    }
    [[nodiscard]] auto variable() const noexcept { return _var; }
    [[nodiscard]] auto condition() const noexcept { return _cond; }
    [[nodiscard]] auto step() const noexcept { return _step; }
    [[nodiscard]] auto body() noexcept { return &_body; }
    [[nodiscard]] auto body() const noexcept { return &_body; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class CommentStmt : public Statement {

private:
    luisa::string _comment;

private:
    uint64_t _compute_hash() const noexcept override {
        return hash64(_comment);
    }

public:
    explicit CommentStmt(luisa::string comment) noexcept
        : Statement{Tag::COMMENT},
          _comment{std::move(comment)} {}
    [[nodiscard]] auto comment() const noexcept { return std::string_view{_comment}; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

class MetaStmt : public Statement {

private:
    luisa::string _info;
    ScopeStmt _scope;
    vector<const MetaStmt *> _children;
    vector<Variable> _variables;

private:
    uint64_t _compute_hash() const noexcept override {
        auto h = std::accumulate(
            _children.cbegin(), _children.cend(),
            hash64(_info, _scope.hash()),
            [](auto seed, auto p) noexcept { return hash64(p->hash(), seed); });
        return std::accumulate(
            _variables.cbegin(), _variables.cend(), h,
            [](auto seed, auto v) noexcept { return hash64(v.hash(), seed); });
    }

public:
    explicit MetaStmt(luisa::string info) noexcept
        : Statement{Tag::META},
          _info{std::move(info)} {}
    [[nodiscard]] auto info() const noexcept { return std::string_view{_info}; }
    [[nodiscard]] auto scope() noexcept { return &_scope; }
    [[nodiscard]] auto scope() const noexcept { return &_scope; }
    [[nodiscard]] auto add(const MetaStmt *child) noexcept { _children.emplace_back(child); }
    [[nodiscard]] auto add(Variable v) noexcept { _variables.emplace_back(v); }
    [[nodiscard]] auto children() const noexcept { return std::span{_children}; }
    [[nodiscard]] auto variables() const noexcept { return std::span{_variables}; }
    LUISA_MAKE_STATEMENT_ACCEPT_VISITOR()
};

#undef LUISA_MAKE_STATEMENT_ACCEPT_VISITOR

}// namespace luisa::compute
