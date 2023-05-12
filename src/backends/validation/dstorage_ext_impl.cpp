#include "dstorage_ext_impl.h"
#include "rw_resource.h"
#include "device.h"
#include "stream.h"
#include <runtime/rhi/command.h>

namespace lc::validation {
DStorageExtImpl::File DStorageExtImpl::open_file_handle(luisa::string_view path) noexcept {
    auto file = _impl->open_file_handle(path);
    if (file.valid())
        new RWResource(file.handle, RWResource::Tag::DSTORAGE_FILE, false);
    return file;
}
void DStorageExtImpl::close_file_handle(uint64_t handle) noexcept {
    _impl->close_file_handle(handle);
    RWResource::dispose(handle);
}
std::pair<DeviceInterface *, ResourceCreationInfo> DStorageExtImpl::create_stream_handle() noexcept {
    auto p = _impl->create_stream_handle();
    if (!p.second.valid()) return p;
    new Stream(p.second.handle, StreamTag::CUSTOM);
    StreamOption opt;
    opt.func = static_cast<StreamFunc>(
        luisa::to_underlying(StreamFunc::Custom) |
        luisa::to_underlying(StreamFunc::Signal) |
        luisa::to_underlying(StreamFunc::Sync));
    opt.supported_custom.emplace(dstorage_command_uuid);
    Device::add_custom_stream(p.second.handle, std::move(opt));
    return p;
}
DStorageExtImpl::DStorageExtImpl(DStorageExt *ext) : _impl{ext} {}
}// namespace lc::validation