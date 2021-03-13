//
// Created by Mike Smith on 2020/12/2.
//

#pragma once

#include <vector>
#include <mutex>

#include <fmt/format.h>

#include <core/memory.h>
#include <core/hash.h>

#include <ast/statement.h>
#include <ast/function.h>
#include <ast/variable.h>
#include <ast/expression.h>
#include <ast/type_registry.h>
#include <ast/usage.h>

namespace luisa::compute {

struct Statement;
struct Expression;

class FunctionBuilder {

private:
    class ScopeGuard {

    private:
        FunctionBuilder *_builder;
        ScopeStmt *_scope;

    public:
        explicit ScopeGuard(FunctionBuilder *builder, ScopeStmt *scope) noexcept
            : _builder{builder}, _scope{scope} { _builder->push_scope(_scope); }
        ~ScopeGuard() noexcept { _builder->pop_scope(_scope); }
    };

public:
    using Tag = Function::Tag;
    using ConstantBinding = Function::ConstantBinding;
    using BufferBinding = Function::BufferBinding;
    using TextureBinding = Function::TextureBinding;

private:
    Arena _arena;
    ScopeStmt *_body;
    const Type *_ret{nullptr};
    ArenaVector<ScopeStmt *> _scope_stack;
    ArenaVector<Variable> _builtin_variables;
    ArenaVector<Variable> _shared_variables;
    ArenaVector<ConstantBinding> _captured_constants;
    ArenaVector<BufferBinding> _captured_buffers;
    ArenaVector<TextureBinding> _captured_textures;
    ArenaVector<Variable> _arguments;
    ArenaVector<uint32_t> _used_custom_callables;
    ArenaVector<std::string_view> _used_builtin_callables;
    ArenaVector<Usage> _variable_usages;
    Tag _tag;
    uint32_t _uid;

protected:
    [[nodiscard]] static std::vector<FunctionBuilder *> &_function_stack() noexcept;
    [[nodiscard]] static std::recursive_mutex &_function_registry_mutex() noexcept;
    [[nodiscard]] static std::vector<std::unique_ptr<FunctionBuilder>> &_function_registry() noexcept;
    [[nodiscard]] uint32_t _next_variable_uid() noexcept;

    static void _push(FunctionBuilder *func) noexcept;
    static FunctionBuilder *_pop() noexcept;

    void _append(const Statement *statement) noexcept;

    [[nodiscard]] const LiteralExpr *_literal(const Type *type, LiteralExpr::Value value) noexcept;
    [[nodiscard]] const RefExpr *_builtin(Variable::Tag tag) noexcept;
    [[nodiscard]] const RefExpr *_texture_binding(const Type *type, uint64_t handle) noexcept;
    [[nodiscard]] const RefExpr *_ref(Variable v) noexcept;

    friend class ScopeGuard;

private:
    explicit FunctionBuilder(Tag tag, uint32_t uid) noexcept
        : _body{_arena.create<ScopeStmt>(ArenaVector<const Statement *>(_arena))},
          _scope_stack{_arena},
          _builtin_variables{_arena},
          _shared_variables{_arena},
          _captured_constants{_arena},
          _captured_buffers{_arena},
          _captured_textures{_arena},
          _arguments{_arena},
          _used_custom_callables{_arena},
          _used_builtin_callables{_arena},
          _variable_usages{_arena},
          _tag{tag},
          _uid{uid} {}

    template<typename Def>
    static auto _define(Function::Tag tag, Def &&def) noexcept {
        auto f = [tag] {
            std::scoped_lock lock{_function_registry_mutex()};
            auto f_uid = static_cast<uint32_t>(_function_registry().size());
            return _function_registry().emplace_back(new FunctionBuilder{tag, f_uid}).get();
        }();
        _push(f);
        f->with(f->_body, std::forward<Def>(def));
        if (_pop() != f) { LUISA_ERROR_WITH_LOCATION("Invalid function on stack top."); }
        return Function{*f};
    }

public:
    [[nodiscard]] static FunctionBuilder *current() noexcept;

    // interfaces for class Function
    [[nodiscard]] auto builtin_variables() const noexcept { return std::span{_builtin_variables.data(), _builtin_variables.size()}; }
    [[nodiscard]] auto shared_variables() const noexcept { return std::span{_shared_variables.data(), _shared_variables.size()}; }
    [[nodiscard]] auto constants() const noexcept { return std::span{_captured_constants.data(), _captured_constants.size()}; }
    [[nodiscard]] auto captured_buffers() const noexcept { return std::span{_captured_buffers.data(), _captured_buffers.size()}; }
    [[nodiscard]] auto captured_textures() const noexcept { return std::span{_captured_textures.data(), _captured_textures.size()}; }
    [[nodiscard]] auto arguments() const noexcept { return std::span{_arguments.data(), _arguments.size()}; }
    [[nodiscard]] auto custom_callables() const noexcept { return std::span{_used_custom_callables.data(), _used_custom_callables.size()}; }
    [[nodiscard]] auto builtin_callables() const noexcept { return std::span{_used_builtin_callables.data(), _used_builtin_callables.size()}; }
    [[nodiscard]] auto tag() const noexcept { return _tag; }
    [[nodiscard]] const auto *body() const noexcept { return _body; }
    [[nodiscard]] auto uid() const noexcept { return _uid; }
    [[nodiscard]] auto return_type() const noexcept { return _ret; }
    [[nodiscard]] auto variable_usage(uint32_t uid) const noexcept { return _variable_usages[uid]; }
    [[nodiscard]] static Function callable(uint32_t uid) noexcept;
    [[nodiscard]] static Function kernel(uint32_t uid) noexcept;

    // build primitives
    template<typename Def>
    static auto define_kernel(Def &&def) noexcept {
        if (!_function_stack().empty()) {
            LUISA_ERROR_WITH_LOCATION("Kernel definitions cannot be nested.");
        }
        return _define(Function::Tag::KERNEL, std::forward<Def>(def));
    }

    template<typename Def>
    static auto define_callable(Def &&def) noexcept {
        auto f = _define(Function::Tag::CALLABLE, std::forward<Def>(def));
        if (!f.builtin_variables().empty()
            || !f.shared_variables().empty()
            || !f.captured_buffers().empty()
            || !f.captured_textures().empty()) {
            LUISA_ERROR_WITH_LOCATION(
                "Custom callables may not have builtin, "
                "shared or captured variables.");
        }
        return f;
    }

    [[nodiscard]] const RefExpr *thread_id() noexcept;
    [[nodiscard]] const RefExpr *block_id() noexcept;
    [[nodiscard]] const RefExpr *dispatch_id() noexcept;

    // variables
    [[nodiscard]] const RefExpr *local(const Type *type, std::span<const Expression *> init) noexcept;
    [[nodiscard]] const RefExpr *local(const Type *type, std::initializer_list<const Expression *> init) noexcept;
    [[nodiscard]] const RefExpr *shared(const Type *type) noexcept;

    [[nodiscard]] const ConstantExpr *constant(const Type *type, uint64_t hash) noexcept;
    [[nodiscard]] const RefExpr *buffer_binding(const Type *element_type, uint64_t handle, size_t offset_bytes) noexcept;
    [[nodiscard]] const RefExpr *texture_binding(const Type *type, uint64_t handle) noexcept;

    // explicit arguments
    [[nodiscard]] const RefExpr *uniform(const Type *type) noexcept;
    [[nodiscard]] const RefExpr *buffer(const Type *type) noexcept;
    [[nodiscard]] const RefExpr *texture(const Type *type) noexcept;

    // expressions
    template<concepts::Basic T>
    [[nodiscard]] auto literal(T value) noexcept { return _literal(Type::of(value), value); }
    [[nodiscard]] const Expression *unary(const Type *type, UnaryOp op, const Expression *expr) noexcept;
    [[nodiscard]] const Expression *binary(const Type *type, BinaryOp op, const Expression *lhs, const Expression *rhs) noexcept;
    [[nodiscard]] const Expression *member(const Type *type, const Expression *self, size_t member_index) noexcept;
    [[nodiscard]] const Expression *access(const Type *type, const Expression *range, const Expression *index) noexcept;
    [[nodiscard]] const Expression *call(const Type *type /* nullptr for void */, std::string_view func, std::initializer_list<const Expression *> args) noexcept;
    [[nodiscard]] const Expression *cast(const Type *type, CastOp op, const Expression *expr) noexcept;

    // statements
    void break_() noexcept;
    void continue_() noexcept;
    void return_(const Expression *expr = nullptr /* nullptr for void */) noexcept;
    void if_(const Expression *cond, const ScopeStmt *true_branch, const ScopeStmt *false_branch) noexcept;
    void while_(const Expression *cond, const ScopeStmt *body) noexcept;
    void void_(const Expression *expr) noexcept;
    void switch_(const Expression *expr, const ScopeStmt *body) noexcept;
    void case_(const Expression *expr, const ScopeStmt *body) noexcept;
    void default_(const ScopeStmt *body) noexcept;
    void for_(const Statement *init, const Expression *condition, const Statement *update, const ScopeStmt *body) noexcept;

    void assign(AssignOp op, const Expression *lhs, const Expression *rhs) noexcept;
    [[nodiscard]] ScopeStmt *scope() noexcept;

    template<typename Body>
    decltype(auto) with(ScopeStmt *s, Body &&body) noexcept {
        ScopeGuard guard{this, s};
        return body();
    }

    void push_scope(ScopeStmt *) noexcept;
    void pop_scope(const ScopeStmt *) noexcept;
    void mark_variable_usage(uint32_t uid, Usage usage) noexcept;
};

}// namespace luisa::compute
