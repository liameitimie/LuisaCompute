#include <runtime/sparse_texture.h>
#include <runtime/rhi/device_interface.h>

namespace luisa::compute {

void SparseTexture::UpdateTiles::operator()(DeviceInterface *device, uint64_t stream_handle) && noexcept {
    device->update_sparse_texture(stream_handle, handle, std::move(tiles));
}

SparseTexture::SparseTexture(DeviceInterface *device,const ResourceCreationInfo &info) noexcept
    : Resource{device, Tag::SPARSE_TEXTURE, info} {
}

SparseTexture::UpdateTiles SparseTexture::update() noexcept {
    return {handle(), std::move(_tiles)};
}

SparseTexture::~SparseTexture() noexcept {
    if (*this) { device()->destroy_sparse_texture(handle()); }
}

}// namespace luisa::compute
