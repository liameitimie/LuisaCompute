//
// Created by Mike Smith on 2020/12/2.
//

#pragma once

#include <ast/type.h>
#include <ast/usage.h>

namespace luisa::compute {

namespace detail {
class FunctionBuilder;
}
class AstSerializer;
class Variable {
    friend class AstSerializer;

public:
    enum struct Tag : uint32_t {

        // data
        LOCAL,
        SHARED,

        // reference
        REFERENCE,

        // resources
        BUFFER,
        TEXTURE,
        HEAP,
        ACCEL,

        // builtins
        THREAD_ID,
        BLOCK_ID,
        DISPATCH_ID,
        DISPATCH_SIZE
    };

private:
    const Type *_type;
    uint32_t _uid;
    Tag _tag;

private:
    friend class detail::FunctionBuilder;
    constexpr Variable(const Type *type, Tag tag, uint32_t uid) noexcept
        : _type{type}, _uid{uid}, _tag{tag} {}

public:
    [[nodiscard]] auto type() const noexcept { return _type; }
    [[nodiscard]] auto uid() const noexcept { return _uid; }
    [[nodiscard]] auto tag() const noexcept { return _tag; }
    [[nodiscard]] auto hash() const noexcept {
        auto u0 = static_cast<uint64_t>(_uid);
        auto u1 = static_cast<uint64_t>(_tag);
        return hash64(u0 | (u1 << 32u), _type->hash());
    }
};

}// namespace luisa::compute
