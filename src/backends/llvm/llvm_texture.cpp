//
// Created by Mike Smith on 2022/6/8.
//

#include <backends/llvm/llvm_texture.h>

namespace luisa::compute::llvm {

namespace detail {

[[nodiscard]] inline auto decode_texture_view(uint64_t t0, uint64_t t1) noexcept {
    return luisa::bit_cast<LLVMTextureView>(ulong2{t0, t1});
}

// from tinyexr: https://github.com/syoyo/tinyexr/blob/master/tinyexr.h
uint float_to_half(float f) noexcept {
    auto bits = luisa::bit_cast<uint>(f);
    auto fp32_sign = bits >> 31u;
    auto fp32_exponent = (bits >> 23u) & 0xffu;
    auto fp32_mantissa = bits & ((1u << 23u) - 1u);
    auto make_fp16 = [](uint sign, uint exponent, uint mantissa) noexcept {
        return (sign << 15u) | (exponent << 10u) | mantissa;
    };
    // Signed zero/denormal (which will underflow)
    if (fp32_exponent == 0u) { return make_fp16(fp32_sign, 0u, 0u); }
    // Inf or NaN (all exponent bits set)
    if (fp32_exponent == 255u) {
        return make_fp16(
            fp32_sign, 31u,
            // NaN->qNaN and Inf->Inf
            fp32_mantissa ? 0x200u : 0u);
    }
    // Exponent unbias the single, then bias the halfp
    auto newexp = static_cast<int>(fp32_exponent - 127u + 15u);
    // Overflow, return signed infinity
    if (newexp >= 31) { return make_fp16(fp32_sign, 31u, 0u); }
    // Underflow
    if (newexp <= 0) {
        if ((14 - newexp) > 24) { return 0u; }
        // Mantissa might be non-zero
        unsigned int mant = fp32_mantissa | 0x800000u;// Hidden 1 bit
        auto fp16 = make_fp16(fp32_sign, 0u, mant >> (14u - newexp));
        if ((mant >> (13u - newexp)) & 1u) { fp16++; }// Check for rounding
        return fp16;
    }
    auto fp16 = make_fp16(fp32_sign, newexp, fp32_mantissa >> 13u);
    if (fp32_mantissa & 0x1000u) { fp16++; }// Check for rounding
    return fp16;
}

float half_to_float(uint h) noexcept {
    static_assert(std::endian::native == std::endian::little,
                  "Only little endian is supported");
    union FP32 {
        unsigned int u;
        float f;
        struct {// FIXME: assuming little endian here
            unsigned int Mantissa : 23;
            unsigned int Exponent : 8;
            unsigned int Sign : 1;
        } s;
    };
    constexpr auto magic = FP32{113u << 23u};
    constexpr auto shifted_exp = 0x7c00u << 13u;// exponent mask after shift
    auto o = FP32{(h & 0x7fffu) << 13u};        // exponent/mantissa bits
    auto exp_ = shifted_exp & o.u;              // just the exponent
    o.u += (127u - 15u) << 23u;                 // exponent adjust

    // handle exponent special cases
    if (exp_ == shifted_exp) {     // Inf/NaN?
        o.u += (128u - 16u) << 23u;// extra exp adjust
    } else if (exp_ == 0u) {       // Zero/Denormal?
        o.u += 1u << 23u;          // extra exp adjust
        o.f -= magic.f;            // renormalize
    }
    o.u |= (h & 0x8000u) << 16u;// sign bit
    return o.f;
}

// from PBRT-v4
static constexpr const std::array ewa_filter_weight_lut{
    // MIPMap EWA Lookup Table Values
    0.864664733f, 0.849040031f, 0.83365953f, 0.818519294f, 0.80361563f, 0.788944781f, 0.774503231f, 0.760287285f,
    0.746293485f, 0.732518315f, 0.718958378f, 0.705610275f, 0.692470789f, 0.679536581f, 0.666804492f, 0.654271305f,
    0.641933978f, 0.629789352f, 0.617834508f, 0.606066525f, 0.594482362f, 0.583079159f, 0.571854174f, 0.560804546f,
    0.549927592f, 0.539220572f, 0.528680861f, 0.518305838f, 0.50809288f, 0.498039544f, 0.488143265f, 0.478401601f,
    0.468812168f, 0.45937258f, 0.450080454f, 0.440933526f, 0.431929469f, 0.423066139f, 0.414341331f, 0.405752778f,
    0.397298455f, 0.388976216f, 0.380784035f, 0.372719884f, 0.364781618f, 0.356967449f, 0.34927541f, 0.341703475f,
    0.334249914f, 0.32691282f, 0.319690347f, 0.312580705f, 0.305582166f, 0.298692942f, 0.291911423f, 0.285235822f,
    0.278664529f, 0.272195935f, 0.265828371f, 0.259560347f, 0.253390193f, 0.247316495f, 0.241337672f, 0.235452279f,
    0.229658857f, 0.223955944f, 0.21834214f, 0.212816045f, 0.207376286f, 0.202021524f, 0.196750447f, 0.191561714f,
    0.186454013f, 0.181426153f, 0.176476851f, 0.171604887f, 0.166809067f, 0.162088141f, 0.157441005f, 0.152866468f,
    0.148363426f, 0.143930718f, 0.139567271f, 0.135272011f, 0.131043866f, 0.126881793f, 0.122784719f, 0.11875169f,
    0.114781633f, 0.11087364f, 0.107026696f, 0.103239879f, 0.0995122194f, 0.0958427936f, 0.0922307223f, 0.0886750817f,
    0.0851749927f, 0.0817295909f, 0.0783380121f, 0.0749994367f, 0.0717130303f, 0.0684779733f, 0.0652934611f, 0.0621587038f,
    0.0590728968f, 0.0560353249f, 0.0530452281f, 0.0501018465f, 0.0472044498f, 0.0443523228f, 0.0415447652f, 0.0387810767f,
    0.0360605568f, 0.0333825648f, 0.0307464004f, 0.0281514227f, 0.0255970061f, 0.0230824798f, 0.0206072628f, 0.0181707144f,
    0.0157722086f, 0.013411209f, 0.0110870898f, 0.0087992847f, 0.0065472275f, 0.00433036685f, 0.0021481365f, 0.f};

}// namespace detail

LLVMTexture::LLVMTexture(PixelStorage storage, uint3 size, uint levels) noexcept
    : _storage{storage}, _mip_levels{levels},
      _pixel_stride{static_cast<uint>(pixel_storage_size(storage))} {
    for (auto i = 0u; i < 3u; i++) { _size[i] = size[i]; }
    _mip_offsets[0] = 0u;
    for (auto i = 1u; i < levels; i++) {
        _mip_offsets[i] = _mip_offsets[i - 1u] +
                          _size[0] * _size[1] * _size[2] * _pixel_stride;
    }
    auto size_bytes = _mip_offsets[levels - 1u] +
                      _size[0] * _size[1] * _size[2] * _pixel_stride;
    _data = luisa::allocate<std::byte>(size_bytes);
}

LLVMTexture::~LLVMTexture() noexcept { luisa::deallocate(_data); }

LLVMTextureView LLVMTexture::view(uint level) const noexcept {
    auto size = luisa::max(make_uint3(_size[0], _size[1], _size[2]) >> level, 1u);
    return LLVMTextureView{_data + _mip_offsets[level],
                           size.x, size.y, size.z,
                           _storage, _pixel_stride};
}

float4 LLVMTexture::read2d(uint level, uint2 uv) const noexcept {
    return view(level).read2d<float>(uv);
}

float4 LLVMTexture::read3d(uint level, uint3 uvw) const noexcept {
    return view(level).read3d<float>(uvw);
}

template<typename T>
[[nodiscard]] inline auto texture_coord_point(Sampler::Address address, T uv, T s) noexcept {
    switch (address) {
        case Sampler::Address::EDGE: return luisa::clamp(uv, 0.0f, one_minus_epsilon) * s;
        case Sampler::Address::REPEAT: return luisa::fract(uv) * s;
        case Sampler::Address::MIRROR: {
            uv = luisa::fmod(luisa::abs(uv), T{2.0f});
            uv = select(2.f - uv, uv, uv < T{1.f});
            return luisa::min(uv, one_minus_epsilon) * s;
        }
        case Sampler::Address::ZERO: return luisa::select(uv * s, T{65536.f}, uv < 0.f || uv >= 1.f);
    }
    return T{65536.f};
}

[[nodiscard]] inline auto texture_coord_linear(Sampler::Address address, float2 uv, float2 size) noexcept {
    auto s = make_float2(size);
    auto inv_s = 1.f / s;
    auto c_min = texture_coord_point(address, uv - .5f * inv_s, s);
    auto c_max = texture_coord_point(address, uv + .5f * inv_s, s);
    return std::make_pair(luisa::min(c_min, c_max), luisa::max(c_min, c_max));
}

[[nodiscard]] inline auto texture_coord_linear(Sampler::Address address, float3 uv, float3 size) noexcept {
    auto s = make_float3(size);
    auto inv_s = 1.f / s;
    auto c_min = texture_coord_point(address, uv - .5f * inv_s, s);
    auto c_max = texture_coord_point(address, uv + .5f * inv_s, s);
    return std::make_pair(luisa::min(c_min, c_max), luisa::max(c_min, c_max));
}

[[nodiscard]] inline auto texture_sample_linear(LLVMTextureView view, Sampler::Address address, float2 uv) noexcept {
    auto size = make_float2(view.size2d());
    auto [st_min, st_max] = texture_coord_linear(address, uv, size);
    auto t = luisa::fract(st_max);
    auto c0 = make_uint2(st_min);
    auto c1 = make_uint2(st_max);
    auto v00 = view.read2d<float>(c0);
    auto v01 = view.read2d<float>(make_uint2(c1.x, c0.y));
    auto v10 = view.read2d<float>(make_uint2(c0.x, c1.y));
    auto v11 = view.read2d<float>(c1);
    return luisa::lerp(luisa::lerp(v00, v01, t.x),
                       luisa::lerp(v10, v11, t.x), t.y);
}

[[nodiscard]] inline auto texture_sample_linear(LLVMTextureView view, Sampler::Address address, float3 uvw) noexcept {
    auto size = make_float3(view.size3d());
    auto [st_min, st_max] = texture_coord_linear(address, uvw, size);
    auto t = luisa::fract(st_max);
    auto c0 = make_uint3(st_min);
    auto c1 = make_uint3(st_max);
    auto v000 = view.read3d<float>(make_uint3(c0.x, c0.y, c0.z));
    auto v001 = view.read3d<float>(make_uint3(c1.x, c0.y, c0.z));
    auto v010 = view.read3d<float>(make_uint3(c0.x, c1.y, c0.z));
    auto v011 = view.read3d<float>(make_uint3(c1.x, c1.y, c0.z));
    auto v100 = view.read3d<float>(make_uint3(c0.x, c0.y, c1.z));
    auto v101 = view.read3d<float>(make_uint3(c1.x, c0.y, c1.z));
    auto v110 = view.read3d<float>(make_uint3(c0.x, c1.y, c1.z));
    auto v111 = view.read3d<float>(make_uint3(c1.x, c1.y, c1.z));
    return luisa::lerp(
        luisa::lerp(luisa::lerp(v000, v001, t.x),
                    luisa::lerp(v010, v011, t.x), t.y),
        luisa::lerp(luisa::lerp(v100, v101, t.x),
                    luisa::lerp(v110, v111, t.x), t.y),
        t.z);
}

[[nodiscard]] inline auto texture_sample_point(LLVMTextureView view, Sampler::Address address, float2 uv) noexcept {
    auto size = make_float2(view.size2d());
    auto c = make_uint2(texture_coord_point(address, uv, size));
    return view.read2d<float>(c);
}

[[nodiscard]] inline auto texture_sample_point(LLVMTextureView view, Sampler::Address address, float3 uvw) noexcept {
    auto size = make_float3(view.size3d());
    auto c = make_uint3(texture_coord_point(address, uvw, size));
    return view.read3d<float>(c);
}

// from PBRT-v4
[[nodiscard]] inline auto texture_sample_ewa(LLVMTextureView view, Sampler::Address address,
                                             float2 uv, float2 dst0, float2 dst1) noexcept {
    auto size = make_float2(view.size2d());
    auto st = texture_coord_point(address, uv, size) - .5f;
    dst0 = dst0 * size;
    dst1 = dst1 * size;

    constexpr auto sqr = [](float x) noexcept { return x * x; };
    constexpr auto safe_sqrt = [](float x) noexcept { return select(std::sqrt(x), 0.f, x <= 0.f); };

    // Find ellipse coefficients that bound EWA filter region
    auto A = sqr(dst0.y) + sqr(dst1.y) + 1.f;
    auto B = -2.f * (dst0.x * dst0.y + dst1.x * dst1.y);
    auto C = sqr(dst0.x) + sqr(dst1.x) + 1.f;
    auto inv_f = 1.f / (A * C - sqr(B) * 0.25f);
    A *= inv_f;
    B *= inv_f;
    C *= inv_f;

    // Compute the ellipse's $(s,t)$ bounding box in texture space
    auto det = -sqr(B) + 4.f * A * C;
    auto inv_det = 1.f / det;
    auto sqrt_u = safe_sqrt(det * C);
    auto sqrt_v = safe_sqrt(A * det);
    auto s_min = static_cast<int>(std::ceil(st.x - 2.f * inv_det * sqrt_u));
    auto s_max = static_cast<int>(std::floor(st.x + 2.f * inv_det * sqrt_u));
    auto t_min = static_cast<int>(std::ceil(st.y - 2.f * inv_det * sqrt_v));
    auto t_max = static_cast<int>(std::floor(st.y + 2.f * inv_det * sqrt_v));

    // Scan over ellipse bound and evaluate quadratic equation to filter image
    auto sum = make_float4();
    auto sum_w = 0.f;
    auto inv_size = 1.f / size;
    for (auto t = t_min; t <= t_max; t++) {
        for (auto s = s_min; s <= s_max; s++) {
            auto tt = static_cast<float>(t) - st.y;
            auto ss = static_cast<float>(s) - st.x;
            // Compute squared radius and filter texel if it is inside the ellipse
            if (auto rr = A * sqr(ss) + B * ss * tt + C * sqr(tt); rr < 1.f) {
                constexpr auto lut_size = static_cast<float>(detail::ewa_filter_weight_lut.size());
                auto index = std::clamp(rr * lut_size, 0.f, lut_size - 1.f);
                auto weight = detail::ewa_filter_weight_lut[static_cast<int>(index)];
                auto p = texture_coord_point(address, uv + make_float2(ss, tt) * inv_size, size);
                sum += weight * view.read2d<float>(make_uint2(p));
                sum_w += weight;
            }
        }
    }
    return select(sum / sum_w, make_float4(0.f), sum_w <= 0.f);
}

[[nodiscard]] inline auto texture_sample_ewa(LLVMTextureView view, Sampler::Address address,
                                             float3 uvw, float3 ddx, float3 ddy) noexcept {
    LUISA_ERROR_WITH_LOCATION("EWA texture sampling is not implemented.");
    return make_float4();
}

float4 LLVMTexture::sample2d(Sampler sampler, float2 uv) const noexcept {
    return sampler.filter() == Sampler::Filter::POINT ?
               texture_sample_point(view(0), sampler.address(), uv) :
               texture_sample_linear(view(0), sampler.address(), uv);
}

float4 LLVMTexture::sample3d(Sampler sampler, float3 uvw) const noexcept {
    return sampler.filter() == Sampler::Filter::POINT ?
               texture_sample_point(view(0), sampler.address(), uvw) :
               texture_sample_linear(view(0), sampler.address(), uvw);
}

float4 LLVMTexture::sample2d(Sampler sampler, float2 uv, float lod) const noexcept {
    auto filter = sampler.filter();
    if (lod <= 0.f || _mip_levels == 0u ||
        filter == Sampler::Filter::POINT) {
        return sample2d(sampler, uv);
    }
    auto level0 = std::min(static_cast<uint32_t>(lod),
                           _mip_levels - 1u);
    auto v0 = texture_sample_linear(
        view(level0), sampler.address(), uv);
    if (level0 == _mip_levels - 1u ||
        filter == Sampler::Filter::LINEAR_POINT) {
        return v0;
    }
    auto v1 = texture_sample_linear(
        view(level0 + 1u), sampler.address(), uv);
    return luisa::lerp(v0, v1, luisa::fract(lod));
}

float4 LLVMTexture::sample3d(Sampler sampler, float3 uvw, float lod) const noexcept {
    auto filter = sampler.filter();
    if (lod <= 0.f || _mip_levels == 0u ||
        filter == Sampler::Filter::POINT) {
        return sample3d(sampler, uvw);
    }
    auto level0 = std::min(static_cast<uint32_t>(lod),
                           _mip_levels - 1u);
    auto v0 = texture_sample_linear(
        view(level0), sampler.address(), uvw);
    if (level0 == _mip_levels - 1u ||
        filter == Sampler::Filter::LINEAR_POINT) {
        return v0;
    }
    auto v1 = texture_sample_linear(
        view(level0 + 1u), sampler.address(), uvw);
    return luisa::lerp(v0, v1, luisa::fract(lod));
}

float4 LLVMTexture::sample2d(Sampler sampler, float2 uv, float2 dpdx, float2 dpdy) const noexcept {
    constexpr auto ll = [](float2 v) noexcept { return dot(v, v); };
    if (all(dpdx == 0.f) || all(dpdy == 0.f)) { return sample2d(sampler, uv); }
    if (sampler.filter() != Sampler::Filter::ANISOTROPIC) {
        auto s = make_float2(_size[0], _size[1]);
        auto level = .5f * std::log2(std::max(ll(dpdx * s), ll(dpdy * s)));
        return sample2d(sampler, uv, level);
    }
    auto longer = length(dpdx);
    auto shorter = length(dpdy);
    if (longer < shorter) {
        std::swap(longer, shorter);
        std::swap(dpdx, dpdy);
    }
    constexpr auto max_anisotropy = 16.f;
    if (auto s = shorter * max_anisotropy; s < longer) {
        auto scale = longer / s;
        dpdy *= scale;
        shorter *= scale;
    }
    auto last_level = static_cast<float>(_mip_levels - 1u);
    auto level = std::clamp(last_level + std::log2(shorter), 0.f, last_level);
    auto level_uint = static_cast<uint>(level);
    auto v0 = texture_sample_ewa(view(level_uint), sampler.address(), uv, dpdx, dpdy);
    if (level == 0.f || level == last_level) { return v0; }
    auto v1 = texture_sample_ewa(view(level_uint + 1u), sampler.address(), uv, dpdx, dpdy);
    return luisa::lerp(v0, v1, luisa::fract(level));
}

float4 LLVMTexture::sample3d(Sampler sampler, float3 uvw, float3 dpdx, float3 dpdy) const noexcept {
    constexpr auto ll = [](float3 v) noexcept { return dot(v, v); };
    if (all(dpdx == 0.f) || all(dpdy == 0.f)) { return sample3d(sampler, uvw); }
    if (sampler.filter() != Sampler::Filter::ANISOTROPIC) {
        auto s = make_float3(_size[0], _size[1], _size[2]);
        auto level = .5f * std::log2(std::max(ll(dpdx * s), ll(dpdy * s)));
        return sample3d(sampler, uvw, level);
    }
    auto longer = length(dpdx);
    auto shorter = length(dpdy);
    if (longer < shorter) {
        std::swap(longer, shorter);
        std::swap(dpdx, dpdy);
    }
    constexpr auto max_anisotropy = 16.f;
    if (auto s = shorter * max_anisotropy; s < longer) {
        auto scale = longer / s;
        dpdy *= scale;
        shorter *= scale;
    }
    auto last_level = static_cast<float>(_mip_levels - 1u);
    auto level = std::clamp(last_level + std::log2(shorter), 0.f, last_level);
    auto level_uint = static_cast<uint>(level);
    auto v0 = texture_sample_ewa(view(level_uint), sampler.address(), uvw, dpdx, dpdy);
    if (level == 0.f || level == last_level) { return v0; }
    auto v1 = texture_sample_ewa(view(level_uint + 1u), sampler.address(), uvw, dpdx, dpdy);
    return luisa::lerp(v0, v1, luisa::fract(level));
}

void texture_write_2d_int(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1, uint64_t v0, uint64_t v1) noexcept {
    detail::decode_texture_view(t0, t1).write2d<int>(
        detail::decode_uint2(c0), detail::decode_int4(v0, v1));
}

void texture_write_3d_int(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1, uint64_t v0, uint64_t v1) noexcept {
    detail::decode_texture_view(t0, t1).write3d<int>(
        detail::decode_uint3(c0, c1), detail::decode_int4(v0, v1));
}

void texture_write_2d_uint(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1, uint64_t v0, uint64_t v1) noexcept {
    detail::decode_texture_view(t0, t1).write2d<uint>(
        detail::decode_uint2(c0),
        detail::decode_uint4(v0, v1));
}

void texture_write_3d_uint(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1, uint64_t v0, uint64_t v1) noexcept {
    detail::decode_texture_view(t0, t1).write3d<uint>(
        detail::decode_uint3(c0, c1), detail::decode_uint4(v0, v1));
}

void texture_write_2d_float(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1, uint64_t v0, uint64_t v1) noexcept {
    detail::decode_texture_view(t0, t1).write2d<float>(
        detail::decode_uint2(c0), detail::decode_float4(v0, v1));
}

void texture_write_3d_float(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1, uint64_t v0, uint64_t v1) noexcept {
    detail::decode_texture_view(t0, t1).write3d<float>(
        detail::decode_uint3(c0, c1), detail::decode_float4(v0, v1));
}

detail::ulong2 texture_read_2d_int(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1) noexcept {
    return detail::encode_int4(
        detail::decode_texture_view(t0, t1).read2d<int>(
            detail::decode_uint2(c0)));
}

detail::ulong2 texture_read_3d_int(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1) noexcept {
    return detail::encode_int4(
        detail::decode_texture_view(t0, t1).read3d<int>(
            detail::decode_uint3(c0, c1)));
}

detail::ulong2 texture_read_2d_uint(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1) noexcept {
    return detail::encode_uint4(
        detail::decode_texture_view(t0, t1).read2d<uint>(
            detail::decode_uint2(c0)));
}

detail::ulong2 texture_read_3d_uint(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1) noexcept {
    return detail::encode_uint4(
        detail::decode_texture_view(t0, t1).read3d<uint>(
            detail::decode_uint3(c0, c1)));
}

detail::ulong2 texture_read_2d_float(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1) noexcept {
    return detail::encode_float4(
        detail::decode_texture_view(t0, t1).read2d<float>(
            detail::decode_uint2(c0)));
}

detail::ulong2 texture_read_3d_float(uint64_t t0, uint64_t t1, uint64_t c0, uint64_t c1) noexcept {
    return detail::encode_float4(
        detail::decode_texture_view(t0, t1).read3d<float>(
            detail::decode_uint3(c0, c1)));
}

detail::ulong2 bindless_texture_2d_read(const LLVMTexture *tex, uint level, uint x, uint y) noexcept {
    return detail::encode_float4(tex->read2d(level, make_uint2(x, y)));
}

detail::ulong2 bindless_texture_3d_read(const LLVMTexture *tex, uint level, uint x, uint y, uint z) noexcept {
    return detail::encode_float4(tex->read3d(level, make_uint3(x, y, z)));
}

detail::ulong2 bindless_texture_2d_sample(const LLVMTexture *tex, uint sampler, float u, float v) noexcept {
    return detail::encode_float4(tex->sample2d(Sampler::decode(sampler), make_float2(u, v)));
}

detail::ulong2 bindless_texture_3d_sample(const LLVMTexture *tex, uint sampler, float u, float v, float w) noexcept {
    return detail::encode_float4(tex->sample3d(Sampler::decode(sampler), make_float3(u, v, w)));
}

detail::ulong2 bindless_texture_2d_sample_level(const LLVMTexture *tex, uint sampler, float u, float v, float lod) noexcept {
    return detail::encode_float4(tex->sample2d(Sampler::decode(sampler), make_float2(u, v), lod));
}

detail::ulong2 bindless_texture_3d_sample_level(const LLVMTexture *tex, uint sampler, float u, float v, float w, float lod) noexcept {
    return detail::encode_float4(tex->sample3d(Sampler::decode(sampler), make_float3(u, v, w), lod));
}

detail::ulong2 bindless_texture_2d_sample_grad(const LLVMTexture *tex, uint sampler, float u, float v, uint64_t dpdx, uint64_t dpdy) noexcept {
    return detail::encode_float4(tex->sample2d(
        Sampler::decode(sampler), make_float2(u, v),
        detail::decode_float2(dpdx), detail::decode_float2(dpdy)));
}

detail::ulong2 bindless_texture_3d_sample_grad(const LLVMTexture *tex, uint64_t sampler_w, uint64_t uv, uint64_t dudxy, uint64_t dvdxy, uint64_t dwdxy) noexcept {
    struct alignas(8) sampler_and_float {
        uint sampler;
        float w;
    };
    auto du_dxy = detail::decode_float2(dudxy);
    auto dv_dxy = detail::decode_float2(dvdxy);
    auto dw_dxy = detail::decode_float2(dwdxy);
    auto dpdx = make_float3(du_dxy.x, dv_dxy.x, dw_dxy.x);
    auto dpdy = make_float3(du_dxy.y, dv_dxy.y, dw_dxy.y);
    auto [sampler, w] = luisa::bit_cast<sampler_and_float>(sampler_w);
    auto uvw = make_float3(detail::decode_float2(uv), w);
    return detail::encode_float4(tex->sample3d(Sampler::decode(sampler), uvw, dpdx, dpdy));
}

}// namespace luisa::compute::llvm
