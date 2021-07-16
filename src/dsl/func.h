//
// Created by Mike Smith on 2021/2/28.
//

#pragma once

#include <runtime/command.h>
#include <runtime/device.h>
#include <runtime/shader.h>
#include <dsl/arg.h>
#include <dsl/expr.h>

namespace luisa::compute {

namespace detail {

template<typename T>
struct definition_to_prototype {
    static_assert(always_false_v<T>, "Invalid type in function definition.");
};

template<typename T>
struct definition_to_prototype<Var<T>> {
    using type = T;
};

template<typename T>
struct prototype_to_creation {
    using type = Var<T>;
};

template<typename T>
struct prototype_to_callable_invocation {
    using type = Expr<T>;
};

template<typename T>
using definition_to_prototype_t = typename definition_to_prototype<T>::type;

template<typename T>
using prototype_to_creation_t = typename prototype_to_creation<T>::type;

template<typename T>
using prototype_to_callable_invocation_t = typename prototype_to_callable_invocation<T>::type;

template<size_t N>
[[nodiscard]] constexpr auto kernel_default_block_size() {
    if constexpr (N == 1) {
        return uint3{256u, 1u, 1u};
    } else if constexpr (N == 2) {
        return uint3{16u, 16u, 1u};
    } else if constexpr (N == 3) {
        return uint3{8u, 8u, 8u};
    } else {
        static_assert(always_false_v<std::integral_constant<size_t, N>>);
    }
}

}// namespace detail

template<typename T>
struct is_kernel : std::false_type {};

template<typename T>
struct is_callable : std::false_type {};

template<size_t N, typename... Args>
class Kernel;

template<size_t N, typename... Args>
struct is_kernel<Kernel<N, Args...>> : std::true_type {};

template<size_t N, typename... Args>
class Kernel {

    static_assert(
        N == 1u || N == 2u || N == 3u
            || std::negation_v<std::disjunction<is_atomic<Args>...>>);

public:
    using shader_type = Shader<N, Args...>;

private:
    std::shared_ptr<const detail::FunctionBuilder> _builder{nullptr};

public:
    template<typename Def,
             std::enable_if_t<
                 std::conjunction_v<
                     std::negation<is_callable<std::remove_cvref_t<Def>>>,
                     std::negation<is_kernel<std::remove_cvref_t<Def>>>>,
                 int> = 0>
    requires concepts::invocable_with_return<void, Def, detail::prototype_to_creation_t<Args>...>
    Kernel(Def &&def)
    noexcept {
        _builder = detail::FunctionBuilder::define_kernel([&def] {
            detail::FunctionBuilder::current()->set_block_size(detail::kernel_default_block_size<N>());
            std::apply(
                std::forward<Def>(def),
                std::tuple{detail::prototype_to_creation_t<Args>{detail::ArgumentCreation{}}...});
        });
    }
    Kernel(Kernel &&) noexcept = default;
    Kernel(const Kernel &) noexcept = default;
    Kernel &operator=(Kernel &&) noexcept = default;
    Kernel &operator=(const Kernel &) noexcept = default;
    [[nodiscard]] const auto &function() const noexcept { return _builder; }
};

template<size_t N, typename... Args>
class Kernel<N, void(Args...)> : public Kernel<N, Args...> {
    using Kernel<N, Args...>::Kernel;
};

template<typename ...Args>
class Kernel1D : public Kernel<1u, Args...> {
    using Kernel<1u, Args...>::Kernel;
};

template<typename ...Args>
class Kernel2D : public Kernel<2u, Args...> {
    using Kernel<2u, Args...>::Kernel;
};

template<typename ...Args>
class Kernel3D : public Kernel<3u, Args...> {
    using Kernel<3u, Args...>::Kernel;
};

template<typename ...Args>
class Kernel1D<void(Args...)> : public Kernel1D<Args...> {
    using Kernel1D<Args...>::Kernel1D;
};

template<typename ...Args>
class Kernel2D<void(Args...)> : public Kernel2D<Args...> {
    using Kernel2D<Args...>::Kernel2D;
};

template<typename ...Args>
class Kernel3D<void(Args...)> : public Kernel3D<Args...> {
    using Kernel3D<Args...>::Kernel3D;
};

// see declarations in runtime/device.h
template<size_t N, typename... Args>
auto Device::compile(const Kernel<N, Args...> &kernel) noexcept
    -> typename Kernel<N, Args...>::shader_type {
    return {this->_impl, kernel.function()};
}

namespace detail {

template<typename T>
struct is_tuple : std::false_type {};

template<typename... T>
struct is_tuple<std::tuple<T...>> : std::true_type {};

template<typename T>
constexpr auto is_tuple_v = is_tuple<T>::value;

template<typename... T, size_t... i>
[[nodiscard]] inline auto tuple_to_var_impl(std::tuple<T...> tuple, std::index_sequence<i...>) noexcept {
    return Var<std::tuple<expr_value_t<T>...>>{std::get<i>(tuple)...};
}

template<typename... T>
[[nodiscard]] inline auto tuple_to_var(std::tuple<T...> tuple) noexcept {
    return tuple_to_var_impl(std::move(tuple), std::index_sequence_for<T...>{});
}

template<typename... T, size_t... i>
[[nodiscard]] inline auto var_to_tuple_impl(Expr<std::tuple<T...>> v, std::index_sequence<i...>) noexcept {
    return std::tuple<Expr<T>...>{v.template member<i>()...};
}

template<typename... T>
[[nodiscard]] inline auto var_to_tuple(Expr<std::tuple<T...>> v) noexcept {
    return var_to_tuple_impl(v, std::index_sequence_for<T...>{});
}

}// namespace detail

template<typename T>
class Callable {
    static_assert(always_false_v<T>);
};

template<typename T>
struct is_callable<Callable<T>> : std::true_type {};

template<typename Ret, typename... Args>
class Callable<Ret(Args...)> {

    static_assert(
        std::negation_v<std::disjunction<
            is_buffer_or_view<Ret>,
            is_image_or_view<Ret>,
            is_volume_or_view<Ret>>>,
        "Callables may not return buffers, "
        "images or volumes (or their views).");

    static_assert(
        std::negation_v<std::disjunction<is_atomic<Args>...>>,
        "Callables are not allowed to have atomic arguments.");

private:
    const detail::FunctionBuilder *_builder;

public:
    template<typename Def,
             std::enable_if_t<
                 std::conjunction_v<
                     std::negation<is_callable<std::remove_cvref_t<Def>>>,
                     std::negation<is_kernel<std::remove_cvref_t<Def>>>,
                     std::is_invocable<Def, detail::prototype_to_creation_t<Args>...>>,
                 int> = 0>
    Callable(Def &&def) noexcept
        : _builder{detail::FunctionBuilder::define_callable([&def] {
              if constexpr (std::is_same_v<Ret, void>) {
                  std::apply(
                      std::forward<Def>(def),
                      std::tuple{detail::prototype_to_creation_t<Args>{detail::ArgumentCreation{}}...});
              } else if constexpr (detail::is_tuple_v<Ret>) {
                  auto ret = detail::tuple_to_var(
                      std::apply(
                          std::forward<Def>(def),
                          std::tuple{detail::prototype_to_creation_t<Args>{detail::ArgumentCreation{}}...}));
                  detail::FunctionBuilder::current()->return_(detail::extract_expression(ret));
              } else {
                  auto ret = std::apply(
                      std::forward<Def>(def),
                      std::tuple{detail::prototype_to_creation_t<Args>{detail::ArgumentCreation{}}...});
                  detail::FunctionBuilder::current()->return_(detail::extract_expression(ret));
              }
          })} {}

    auto operator()(detail::prototype_to_callable_invocation_t<Args>... args) const noexcept {
        if constexpr (std::is_same_v<Ret, void>) {
            detail::FunctionBuilder::current()->call(
                _builder->function(),
                {args.expression()...});
        } else if constexpr (detail::is_tuple_v<Ret>) {
            Var ret = detail::Expr<Ret>{detail::FunctionBuilder::current()->call(
                Type::of<Ret>(),
                _builder->function(),
                {args.expression()...})};
            return detail::var_to_tuple(ret);
        } else {
            return detail::Expr<Ret>{detail::FunctionBuilder::current()->call(
                Type::of<Ret>(),
                _builder->function(),
                {args.expression()...})};
        }
    }
};

namespace detail {

template<typename T>
struct function {
    using type = typename function<
        decltype(std::function{std::declval<T>()})>::type;
};

template<typename R, typename... A>
using function_signature_t = R(A...);

template<typename... Args>
struct function<std::function<void(Args...)>> {
    using type = function_signature_t<
        void,
        definition_to_prototype_t<Args>...>;
};

template<typename Ret, typename... Args>
struct function<std::function<Ret(Args...)>> {
    using type = function_signature_t<
        expr_value_t<Ret>,
        definition_to_prototype_t<Args>...>;
};

template<typename... Ret, typename... Args>
struct function<std::function<std::tuple<Ret...>(Args...)>> {
    using type = function_signature_t<
        std::tuple<expr_value_t<Ret>...>,
        definition_to_prototype_t<Args>...>;
};

template<typename RA, typename RB, typename... Args>
struct function<std::function<std::pair<RA, RB>(Args...)>> {
    using type = function_signature_t<
        std::tuple<expr_value_t<RA>, expr_value_t<RB>>,
        definition_to_prototype_t<Args>...>;
};

template<typename T>
struct function<Kernel1D<T>> {
    using type = T;
};

template<typename T>
struct function<Kernel2D<T>> {
    using type = T;
};

template<typename T>
struct function<Kernel3D<T>> {
    using type = T;
};

template<typename T>
struct function<Callable<T>> {
    using type = T;
};

template<typename T>
using function_t = typename function<T>::type;

}// namespace detail

template<typename T>
Kernel1D(T &&) -> Kernel1D<detail::function_t<std::remove_cvref_t<T>>>;

template<typename T>
Kernel2D(T &&) -> Kernel2D<detail::function_t<std::remove_cvref_t<T>>>;

template<typename T>
Kernel3D(T &&) -> Kernel3D<detail::function_t<std::remove_cvref_t<T>>>;

// Hurry up! Clang!!!
// template<typename ...Args>
// using K1D = Kernel1D<void(Args...)>;
//
// template<typename ...Args>
// using K2D = Kernel2D<void(Args...)>;
//
// template<typename ...Args>
// using K3D = Kernel3D<void(Args...)>;

template<typename T>
Callable(T &&) -> Callable<detail::function_t<std::remove_cvref_t<T>>>;

}// namespace luisa::compute

namespace luisa::compute::detail {

struct CallableBuilder {
    template<typename F>
    [[nodiscard]] auto operator%(F &&def) const noexcept {
        return Callable{std::forward<F>(def)};
    }
};

struct Kernel1DBuilder {
    template<typename F>
    [[nodiscard]] auto operator%(F &&def) const noexcept {
        return Kernel1D{std::forward<F>(def)};
    }
};

struct Kernel2DBuilder {
    template<typename F>
    [[nodiscard]] auto operator%(F &&def) const noexcept {
        return Kernel2D{std::forward<F>(def)};
    }
};

struct Kernel3DBuilder {
    template<typename F>
    [[nodiscard]] auto operator%(F &&def) const noexcept {
        return Kernel3D{std::forward<F>(def)};
    }
};

}// namespace luisa::compute::detail

#define LUISA_KERNEL1D ::luisa::compute::detail::Kernel1DBuilder{} % [&]
#define LUISA_KERNEL2D ::luisa::compute::detail::Kernel2DBuilder{} % [&]
#define LUISA_KERNEL3D ::luisa::compute::detail::Kernel3DBuilder{} % [&]
#define LUISA_CALLABLE ::luisa::compute::detail::CallableBuilder{} % [&]
