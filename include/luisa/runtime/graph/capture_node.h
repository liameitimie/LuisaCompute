#pragma once
#include <luisa/runtime/graph/graph_node.h>
#include <luisa/vstl/functional.h>
#include <luisa/runtime/graph/utils.h>
namespace luisa::compute::graph {

class LC_RUNTIME_API CaptureNodeBase : public GraphNode {
public:
    CaptureNodeBase(GraphBuilder *builder) noexcept;
    auto capture_id() const noexcept { return _capture_id; }
    virtual void capture(GraphBuilder *builder, uint64_t stream_handle) const noexcept = 0;
private:
    uint64_t _capture_id = invalid_node_id();
};

template<typename FuncCapture, typename... Args>
class CaptureNode : public CaptureNodeBase {
public:
    CaptureNode(GraphBuilder *builder,
                span<Usage, sizeof...(Args)> usages,
                const FuncCapture &capture,
                const GraphVar<Args> &...args) noexcept
        : CaptureNodeBase{builder},
          _capture{capture} {
        detail::for_each_arg_with_index([&](size_t I, const GraphVarBase &arg) noexcept {
            add_arg_usage(arg.arg_id(), usages[I]);
        },
                                        args...);
    }
protected:
    void capture(GraphBuilder *builder, uint64_t stream_handle) const noexcept override {
        auto arg_view = [&]<typename Arg>(size_t I) {
            auto arg = builder->graph_var(arg_id(I));
            auto drived = arg->cast<GraphVar<Arg>>()->cast<GraphVar<Arg>>();
            auto view = drived->view();
            return view;
        };

        auto expand = [&]<typename... Args, size_t... I>(std::index_sequence<I...>) noexcept {
            _capture(stream_handle, arg_view.operator()<Args>(I)...);
        };

        expand.operator()<Args...>(std::index_sequence_for<Args...>{});
    }
private:
    FuncCapture _capture;
};
}// namespace luisa::compute::graph