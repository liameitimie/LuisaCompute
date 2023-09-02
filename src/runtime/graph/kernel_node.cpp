#include <luisa/runtime/graph/kernel_node.h>
#include <luisa/runtime/device.h>
#include <luisa/runtime/graph/graph_builder.h>
#include <luisa/runtime/graph/kernel_node_cmd_encoder.h>
namespace luisa::compute::graph {
KernelNode::KernelNode(GraphBuilder *builder, span<uint64_t> arg_ids, const Resource *shader_resource, size_t dimension, const uint3 &block_size) noexcept
    : GraphNode{builder, GraphNodeType::Kernel},
      _shader_resource{shader_resource},
      _kernel_id{builder->kernel_nodes().size()},
      _dimension{dimension},
      _block_size{block_size} {
    auto res = _shader_resource;
    auto device = res->device();
    for (size_t i = 0; i < arg_ids.size(); ++i) {
        auto var_id = arg_ids[i];
        auto var = builder->graph_var(var_id);
        // get the usage of the shader argument from DeviceInterface
        auto usage = device->shader_argument_usage(res->handle(), i);
        // add the usage to the kernel node
        this->add_arg_usage(var->arg_id(), usage);
    }
    _kernel_arg_count = arg_ids.size();
}

void KernelNode::add_dispatch_arg(uint64_t x_arg_id) noexcept {
    add_arg_usage(x_arg_id, Usage::NONE);
}

void KernelNode::add_dispatch_arg(uint64_t x_arg_id, uint64_t y_arg_id) noexcept {
    add_arg_usage(x_arg_id, Usage::NONE);
    add_arg_usage(y_arg_id, Usage::NONE);
}

void KernelNode::add_dispatch_arg(uint64_t x_arg_id, uint64_t y_arg_id, uint64_t z_arg_id) noexcept {
    add_arg_usage(x_arg_id, Usage::NONE);
    add_arg_usage(y_arg_id, Usage::NONE);
    add_arg_usage(z_arg_id, Usage::NONE);
}

}// namespace luisa::compute::graph
