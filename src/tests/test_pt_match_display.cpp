//
// Created by Mike Smith on 2021/6/23.
//

#include <iostream>

#include <runtime/context.h>
#include <runtime/device.h>
#include <runtime/stream.h>
#include <runtime/event.h>
#include <dsl/sugar.h>
#include <rtx/accel.h>
#include <tests/fake_device.h>
#include <tests/cornell_box.h>

#include <opencv2/opencv.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tests/tiny_obj_loader.h>

using namespace luisa;
using namespace luisa::compute;

struct Onb {
    float3 tangent;
    float3 binormal;
    float3 normal;
};

LUISA_STRUCT(Onb, tangent, binormal, normal){
    [[nodiscard]] auto to_world(Expr<float3> v) const noexcept {
        return v.x * tangent + v.y * binormal + v.z * normal;
}
}
;

struct MaterialEvaluation {
    float3 f;
    float3 emission;
    float pdf;
};

struct MaterialSample {
    float3 wi;
    MaterialEvaluation eval;
};

struct MaterialSampleAndEvaluation {
    MaterialSample sample;
    MaterialEvaluation eval;
};

struct LightSample {
    float3 emission;
    float3 pdf;
};

LUISA_STRUCT(MaterialEvaluation, f, emission, pdf){};
LUISA_STRUCT(MaterialSample, wi, eval){};
LUISA_STRUCT(MaterialSampleAndEvaluation, sample, eval){};

int main(int argc, char *argv[]) {

    log_level_verbose();

    Context context{argv[0]};

#if defined(LUISA_BACKEND_METAL_ENABLED)
    auto device = context.create_device("metal", 1u);
#elif defined(LUISA_BACKEND_DX_ENABLED)
    auto device = context.create_device("dx");
#else
    auto device = FakeDevice::create(context);
#endif

    // load the Cornell Box scene
    tinyobj::ObjReaderConfig obj_reader_config;
    obj_reader_config.triangulate = true;
    obj_reader_config.vertex_color = false;
    tinyobj::ObjReader obj_reader;
    if (!obj_reader.ParseFromString(obj_string, "", obj_reader_config)) {
        std::string_view error_message = "unknown error.";
        if (auto &&e = obj_reader.Error(); !e.empty()) { error_message = e; }
        LUISA_ERROR_WITH_LOCATION("Failed to load OBJ file: {}", error_message);
    }
    if (auto &&e = obj_reader.Warning(); !e.empty()) {
        LUISA_WARNING_WITH_LOCATION("{}", e);
    }

    auto &&p = obj_reader.GetAttrib().vertices;
    std::vector<float3> vertices;
    vertices.reserve(p.size() / 3u);
    for (auto i = 0u; i < p.size(); i += 3u) {
        vertices.emplace_back(float3{
            p[i + 0u],
            p[i + 1u],
            p[i + 2u]});
    }
    LUISA_INFO(
        "Loaded mesh with {} shape(s) and {} vertices.",
        obj_reader.GetShapes().size(), vertices.size());

    auto heap = device.create_bindless_array();
    auto stream = device.create_stream();
    auto vertex_buffer = device.create_buffer<float3>(vertices.size());
    stream << vertex_buffer.copy_from(vertices.data());
    std::vector<Mesh> meshes;
    for (auto &&shape : obj_reader.GetShapes()) {
        auto index = static_cast<uint>(meshes.size());
        auto &&t = shape.mesh.indices;
        auto triangle_count = t.size() / 3u;
        LUISA_INFO(
            "Processing shape '{}' at index {} with {} triangle(s).",
            shape.name, index, triangle_count);
        auto &&mesh = meshes.emplace_back(device.create_mesh());
        std::vector<uint> indices;
        indices.reserve(t.size());
        for (auto i : t) { indices.emplace_back(i.vertex_index); }
        auto triangle_buffer = device.create_buffer<Triangle>(triangle_count);
        heap.emplace(index, triangle_buffer);
        stream << triangle_buffer.copy_from(indices.data())
               << mesh.build(AccelBuildHint::FAST_TRACE, vertex_buffer, triangle_buffer);
    }
    stream << heap.update();

    std::vector<uint64_t> instances;
    std::vector<float4x4> transforms;
    for (auto &&m : meshes) {
        instances.emplace_back(m.handle());
        transforms.emplace_back(make_float4x4(1.0f));
    }
    auto accel = device.create_accel();
    stream << accel.build(AccelBuildHint::FAST_TRACE, instances, transforms);

    auto make_diffuse_material = [](float3 albedo) noexcept {
        auto cosine_sample_hemisphere = [](Float2 u) noexcept {
            auto r = sqrt(u.x);
            auto phi = 2.0f * constants::pi * u.y;
            return make_float3(r * cos(phi), r * sin(phi), sqrt(1.0f - u.x));
        };
        auto make_onb = [](const Float3 &normal) noexcept {
            auto binormal = normalize(ite(
                abs(normal.x) > abs(normal.z),
                make_float3(-normal.y, normal.x, 0.0f),
                make_float3(0.0f, -normal.z, normal.y)));
            auto tangent = normalize(cross(binormal, normal));
            return def<Onb>(tangent, binormal, normal);
        };
        Callable sample_and_eval = [&](Float3 n, Float3 wo, Float3 wi_light, Float2 u) noexcept {
            // evaluate
            auto cos_wi_light = dot(n, wi_light);
            Var<MaterialSampleAndEvaluation> mat;
            mat.eval.f = max(albedo * inv_pi * cos_wi_light, 0.0f);
            mat.eval.pdf = max(cos_wi_light * inv_pi, 0.0f);
            // sample
            auto onb = make_onb(n);
            auto wi = onb->to_world(cosine_sample_hemisphere(u));
            auto cos_wi = dot(wi, n);
            mat.sample.eval.f = albedo * inv_pi * cos_wi;
            mat.sample.eval.pdf = cos_wi * inv_pi;
            mat.sample.wi = wi;
            return mat;
        };
        return sample_and_eval;
    };

    auto make_emissive_material = [](float3 emission) noexcept {
        Callable sample_and_eval = [&](Float3 n, Float3 wo, Float3, Float2) noexcept {
            auto s = ite(dot(n, wo) > 1e-4f, 1.0f, 0.0f);
            Var<MaterialSampleAndEvaluation> mat;
            mat.eval.emission = emission * s;
            return mat;
        };
        return sample_and_eval;
    };

    std::vector<Callable<MaterialSampleAndEvaluation(float3, float3, float3, float2)>> material_sample_and_eval;
    material_sample_and_eval.emplace_back(make_diffuse_material(make_float3(0.725f, 0.71f, 0.68f)));// white
    material_sample_and_eval.emplace_back(make_diffuse_material(make_float3(0.14f, 0.45f, 0.091f)));// green
    material_sample_and_eval.emplace_back(make_diffuse_material(make_float3(0.63f, 0.065f, 0.05f)));// red
    material_sample_and_eval.emplace_back(make_emissive_material(make_float3(17.0f, 12.0f, 4.0f))); // light

    std::vector material_ids{0u, 0u, 0u, 1u, 2u, 0u, 0u, 3u};
    auto material_buffer = device.create_buffer<uint>(material_ids.size());
    stream << material_buffer.copy_from(material_ids.data());

    Callable linear_to_srgb = [](Var<float3> x) noexcept {
        return clamp(select(1.055f * pow(x, 1.0f / 2.4f) - 0.055f,
                            12.92f * x,
                            x <= 0.00031308f),
                     0.0f, 1.0f);
    };

    Callable tea = [](UInt v0, UInt v1) noexcept {
        auto s0 = def(0u);
        for (auto n = 0u; n < 4u; n++) {
            s0 += 0x9e3779b9u;
            v0 += ((v1 << 4) + 0xa341316cu) ^ (v1 + s0) ^ ((v1 >> 5u) + 0xc8013ea4u);
            v1 += ((v0 << 4) + 0xad90777du) ^ (v0 + s0) ^ ((v0 >> 5u) + 0x7e95761eu);
        }
        return v0;
    };

    Kernel2D make_sampler_kernel = [&](ImageUInt state_image, UInt2 offset) noexcept {
        auto p = dispatch_id().xy() + offset;
        auto state = tea(p.x, p.y);
        state_image.write(dispatch_id().xy(), make_uint4(state));
    };

    Callable lcg = [](UInt &state) noexcept {
        constexpr auto lcg_a = 1664525u;
        constexpr auto lcg_c = 1013904223u;
        state = lcg_a * state + lcg_c;
        return cast<float>(state & 0x00ffffffu) * (1.0f / static_cast<float>(0x01000000u));
    };

    Callable generate_ray = [](Float2 p) noexcept {
        static constexpr auto fov = radians(27.8f);
        static constexpr auto origin = make_float3(-0.01f, 0.995f, 5.0f);
        auto pixel = origin + make_float3(p * tan(0.5f * fov), -1.0f);
        auto direction = normalize(pixel - origin);
        return make_ray(origin, direction);
    };

    Callable balanced_heuristic = [](Float pdf_a, Float pdf_b) noexcept {
        return pdf_a / max(pdf_a + pdf_b, 1e-4f);
    };

    Kernel2D raytracing_kernel = [&](ImageFloat image, ImageUInt state_image, AccelVar accel, UInt2 tile_offset, UInt2 resolution) noexcept {
        set_block_size(8u, 8u, 1u);
        auto coord = dispatch_id().xy() + tile_offset;
        auto frame_size = min(resolution.x, resolution.y).cast<float>();
        auto state = state_image.read(dispatch_id().xy()).x;
        auto rx = lcg(state);
        auto ry = lcg(state);
        auto pixel = (make_float2(coord) + make_float2(rx, ry)) / frame_size * 2.0f - 1.0f;
        auto ray = generate_ray(pixel * make_float2(1.0f, -1.0f));
        auto radiance = def(make_float3(0.0f));
        auto beta = def(make_float3(1.0f));
        auto pdf_bsdf = def(0.0f);
        constexpr auto light_position = make_float3(-0.24f, 1.98f, 0.16f);
        constexpr auto light_u = make_float3(-0.24f, 1.98f, -0.22f) - light_position;
        constexpr auto light_v = make_float3(0.23f, 1.98f, 0.16f) - light_position;
        constexpr auto light_emission = make_float3(17.0f, 12.0f, 4.0f);
        auto light_area = length(cross(light_u, light_v));
        auto light_normal = normalize(cross(light_u, light_v));

        for (auto depth : range(5u)) {

            // trace
            auto hit = accel.trace_closest(ray);
            $if(hit->miss()) { $break; };
            auto triangle = heap.buffer<Triangle>(hit.inst)[hit.prim];
            auto p0 = vertex_buffer[triangle.i0];
            auto p1 = vertex_buffer[triangle.i1];
            auto p2 = vertex_buffer[triangle.i2];
            auto p = interpolate(hit, p0, p1, p2);
            auto n = normalize(cross(p1 - p0, p2 - p0));
            auto wo = -direction(ray);
            auto cos_wo = dot(wo, n);
            $if(cos_wo < 1e-4f) { $break; };
            auto material = material_buffer[hit.inst];

            // sample light
            auto ux_light = lcg(state);
            auto uy_light = lcg(state);
            auto p_light = light_position + ux_light * light_u + uy_light * light_v;
            auto d_light = distance(p, p_light);
            auto wi_light = normalize(p_light - p);
            auto shadow_ray = make_ray_robust(p, n, wi_light, d_light - 1e-3f);
            auto occluded = accel.trace_any(shadow_ray);
            auto cos_wi_light = dot(wi_light, n);
            auto cos_light = -dot(light_normal, wi_light);
            auto pdf_light = def(0.0f);
            $if(!occluded && cos_wi_light > 1e-4f && cos_light > 1e-4f) {
                pdf_light = (d_light * d_light) / (light_area * cos_light);
            };

            auto ux = lcg(state);
            auto uy = lcg(state);
            auto u = make_float2(ux, uy);
            Var<MaterialSampleAndEvaluation> mat;
            $switch (material) {
                for (auto i = 0u; i < material_sample_and_eval.size(); i++) {
                    $case(i) {
                        mat = material_sample_and_eval[i](n, wo, wi_light, u);
                    };
                }
            };

            // hit light
            $if(any(mat.eval.emission != make_float3(0.0f))) {
                $if(depth == 0u) {
                    radiance += light_emission;
                }
                $else {
                    auto pdf_light = length_squared(p - origin(ray)) / (light_area * cos_wo);
                    auto mis_weight = balanced_heuristic(pdf_bsdf, pdf_light);
                    radiance += mis_weight * beta * light_emission;
                };
            };

            // nee
            $if(mat.eval.pdf > 1e-4f) {
                auto mis_weight = balanced_heuristic(pdf_light, mat.eval.pdf);
                radiance += beta * mat.eval.f * mis_weight * light_emission / max(pdf_light, 1e-4f);
            };

            // next bounce
            pdf_bsdf = mat.sample.eval.pdf;
            $if (pdf_bsdf < 1e-4f) { $break; };
            ray = make_ray_robust(p, n, mat.sample.wi);
            beta *= mat.sample.eval.f / pdf_bsdf;

            // rr
            auto l = dot(make_float3(0.212671f, 0.715160f, 0.072169f), beta);
            $if(l == 0.0f) { $break; };
            auto q = max(l, 0.05f);
            auto r = lcg(state);
            $if(r >= q) { $break; };
            beta *= 1.0f / q;
        }
        state_image.write(dispatch_id().xy(), make_uint4(state));
        $if(any(isnan(radiance))) { radiance = make_float3(0.0f); };
        image.write(dispatch_id().xy(), make_float4(clamp(radiance, 0.0f, 30.0f), 1.0f));
    };

    Kernel2D accumulate_kernel = [&](ImageFloat accum_image, ImageFloat curr_image) noexcept {
        auto p = dispatch_id().xy();
        auto accum = accum_image.read(p);
        auto curr = curr_image.read(p).xyz();
        auto t = 1.0f / (accum.w + 1.0f);
        accum_image.write(p, make_float4(lerp(accum.xyz(), curr, t), accum.w + 1.0f));
    };

    Callable aces_tonemapping = [](Float3 x) noexcept {
        static constexpr auto a = 2.51f;
        static constexpr auto b = 0.03f;
        static constexpr auto c = 2.43f;
        static constexpr auto d = 0.59f;
        static constexpr auto e = 0.14f;
        return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
    };

    Kernel2D clear_kernel = [](ImageFloat image) noexcept {
        image.write(dispatch_id().xy(), make_float4(0.0f));
    };

    Kernel2D hdr2ldr_kernel = [&](ImageFloat hdr_image, ImageFloat ldr_image, Float scale) noexcept {
        auto coord = dispatch_id().xy();
        auto hdr = hdr_image.read(coord);
        auto ldr = linear_to_srgb(aces_tonemapping(hdr.zyx() * scale));
        ldr_image.write(coord, make_float4(ldr, 1.0f));
    };

    auto clear_shader = device.compile(clear_kernel);
    auto hdr2ldr_shader = device.compile(hdr2ldr_kernel);
    auto accumulate_shader = device.compile(accumulate_kernel);
    auto raytracing_shader = device.compile(raytracing_kernel);
    auto make_sampler_shader = device.compile(make_sampler_kernel);

    static constexpr auto resolution = make_uint2(1024u);
    static constexpr auto tile_size = make_uint2(1024u);
    auto state_image = device.create_image<uint>(PixelStorage::INT1, tile_size);
    auto framebuffer = device.create_image<float>(PixelStorage::FLOAT4, tile_size);
    auto accum_image = device.create_image<float>(PixelStorage::FLOAT4, tile_size);
    auto ldr_image = device.create_image<float>(PixelStorage::BYTE4, tile_size);
    cv::Mat cv_accum{tile_size.y, tile_size.x, CV_8UC4, cv::Scalar::all(0.0)};
    cv::Mat cv_curr{tile_size.y, tile_size.x, CV_8UC4, cv::Scalar::all(0.0)};
    cv::Mat cv_display{resolution.y, resolution.x, CV_8UC4, cv::Scalar::all(0.0)};

    Clock clock;
    clock.tic();
    auto tile_offset = make_uint2(0u);
    auto reset = [&] {
        stream << clear_shader(accum_image).dispatch(tile_size)
               << make_sampler_shader(state_image, tile_offset).dispatch(tile_size);
    };

    reset();
    for (auto d = 0u;; d++) {
        auto command_buffer = stream.command_buffer();
        static constexpr auto spp_per_dispatch = 1u;
        for (auto i = 0u; i < spp_per_dispatch; i++) {
            command_buffer << raytracing_shader(framebuffer, state_image, accel, tile_offset, resolution).dispatch(tile_size)
                           << accumulate_shader(accum_image, framebuffer).dispatch(tile_size);
        }
        command_buffer << hdr2ldr_shader(framebuffer, ldr_image, 1.0f).dispatch(tile_size)
                       //                       << ldr_image.copy_to(cv_curr.data)
                       << hdr2ldr_shader(accum_image, ldr_image, 1.0f).dispatch(tile_size)
                       << ldr_image.copy_to(cv_accum.data)
                       << commit();
        stream << synchronize();
        cv_accum.copyTo(cv_display(cv::Rect{static_cast<int>(tile_offset.x), static_cast<int>(tile_offset.y), tile_size.x, tile_size.y}));
        //        cv::imshow("Current", cv_curr);
        cv::imshow("Display", cv_display);
        if (auto key = cv::waitKey(1); key == 'q') {
            break;
        } else if (key == 'w') {
            tile_offset.y = std::max(tile_offset.y, tile_size.y) - tile_size.y;
            reset();
        } else if (key == 'a') {
            tile_offset.x = std::max(tile_offset.x, tile_size.x) - tile_size.x;
            reset();
        } else if (key == 's') {
            tile_offset.y = std::min(tile_offset.y + tile_size.y + tile_size.y, resolution.y) - tile_size.y;
            reset();
        } else if (key == 'd') {
            tile_offset.x = std::min(tile_offset.x + tile_size.x + tile_size.x, resolution.x) - tile_size.x;
            reset();
        }
        LUISA_INFO("Progress: {}spp | {}s", (d + 1u) * spp_per_dispatch, clock.toc() * 1e-3f);
    }
    cv::imwrite("test_path_tracing.png", cv_accum);
}
