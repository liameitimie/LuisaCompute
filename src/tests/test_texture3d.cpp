//
// Created by Mike on 5/24/2023.
//

#include <luisa-compute.h>

using namespace luisa;
using namespace luisa::compute;

struct PerlinSettings {
    int octave;
    float power;
    float frequency;
};

struct TRay {
    float3 origin;
    float3 direction;
};

struct Bbox {
    float3 min;
    float3 max;
};

LUISA_STRUCT(PerlinSettings, octave, power, frequency){};
LUISA_STRUCT(TRay, origin, direction){};
LUISA_STRUCT(Bbox, min, max){};

// credit: https://github.com/nvpro-samples/vk_mini_samples/tree/main/samples/texture_3d/shaders (Apache License 2.0)
int main(int argc, char *argv[]) {

    Context context{argv[0]};

    if (argc <= 1) {
        LUISA_INFO("Usage: {} <backend>. <backend>: cuda, dx, ispc, metal", argv[0]);
        exit(1);
    }
    Device device = context.create_device(argv[1]);

    static constexpr auto mod289 = [](auto x) noexcept { return x - floor(x * (1.f / 289.f)) * 289.f; };
    static constexpr auto permute = [](auto x) noexcept { return mod289(((x * 34.f) + 1.f) * x); };
    static constexpr auto taylor_inv_sqrt = [](auto r) noexcept { return 1.79284291400159f - 0.85373472095314f * r; };
    static constexpr auto fade = [](auto t) noexcept { return t * t * t * (t * (t * 6.f - 15.f) + 10.f); };

    Callable perlin = [](Float3 P) noexcept -> Float {
        auto Pi0 = floor(P); // Integer part for indexing
        auto Pi1 = Pi0 + 1.f;// Integer part + 1
        Pi0 = mod289(Pi0);
        Pi1 = mod289(Pi1);
        auto Pf0 = fract(P); // Fractional part for interpolation
        auto Pf1 = Pf0 - 1.f;// Fractional part - 1.0
        auto ix = make_float4(Pi0.x, Pi1.x, Pi0.x, Pi1.x);
        auto iy = make_float4(Pi0.yy(), Pi1.yy());
        auto iz0 = Pi0.zzzz();
        auto iz1 = Pi1.zzzz();

        auto ixy = permute(permute(ix) + iy);
        auto ixy0 = permute(ixy + iz0);
        auto ixy1 = permute(ixy + iz1);

        auto gx0 = ixy0 / 7.f;
        auto gy0 = fract(floor(gx0) / 7.f) - .5f;
        gx0 = fract(gx0);
        auto gz0 = .5f - abs(gx0) - abs(gy0);
        auto sz0 = step(gz0, 0.f);
        gx0 -= sz0 * (step(0.f, gx0) - .5f);
        gy0 -= sz0 * (step(0.f, gy0) - .5f);

        auto gx1 = ixy1 / 7.f;
        auto gy1 = fract(floor(gx1) / 7.f) - .5f;
        gx1 = fract(gx1);
        auto gz1 = .5f - abs(gx1) - abs(gy1);
        auto sz1 = step(gz1, 0.f);
        gx1 -= sz1 * (step(0.f, gx1) - .5f);
        gy1 -= sz1 * (step(0.f, gy1) - .5f);

        auto g000 = make_float3(gx0.x, gy0.x, gz0.x);
        auto g100 = make_float3(gx0.y, gy0.y, gz0.y);
        auto g010 = make_float3(gx0.z, gy0.z, gz0.z);
        auto g110 = make_float3(gx0.w, gy0.w, gz0.w);
        auto g001 = make_float3(gx1.x, gy1.x, gz1.x);
        auto g101 = make_float3(gx1.y, gy1.y, gz1.y);
        auto g011 = make_float3(gx1.z, gy1.z, gz1.z);
        auto g111 = make_float3(gx1.w, gy1.w, gz1.w);

        auto norm0 = taylor_inv_sqrt(make_float4(
            dot(g000, g000), dot(g010, g010),
            dot(g100, g100), dot(g110, g110)));

        g000 *= norm0.x;
        g010 *= norm0.y;
        g100 *= norm0.z;
        g110 *= norm0.w;
        auto norm1 = taylor_inv_sqrt(make_float4(
            dot(g001, g001), dot(g011, g011),
            dot(g101, g101), dot(g111, g111)));
        g001 *= norm1.x;
        g011 *= norm1.y;
        g101 *= norm1.z;
        g111 *= norm1.w;

        auto n000 = dot(g000, Pf0);
        auto n100 = dot(g100, make_float3(Pf1.x, Pf0.yz()));
        auto n010 = dot(g010, make_float3(Pf0.x, Pf1.y, Pf0.z));
        auto n110 = dot(g110, make_float3(Pf1.xy(), Pf0.z));
        auto n001 = dot(g001, make_float3(Pf0.xy(), Pf1.z));
        auto n101 = dot(g101, make_float3(Pf1.x, Pf0.y, Pf1.z));
        auto n011 = dot(g011, make_float3(Pf0.x, Pf1.yz()));
        auto n111 = dot(g111, Pf1);

        auto fade_xyz = fade(Pf0);
        auto n_z = lerp(make_float4(n000, n100, n010, n110),
                        make_float4(n001, n101, n011, n111),
                        fade_xyz.z);
        auto n_yz = lerp(n_z.xy(), n_z.zw(), fade_xyz.y);
        auto n_xyz = lerp(n_yz.x, n_yz.y, fade_xyz.x);
        return 2.2f * n_xyz;
    };

    auto make_perlin_noise = device.compile<3>([&](VolumeFloat out, Var<PerlinSettings> settings) noexcept {
        auto v = def(0.f);
        auto scale = settings.power;
        auto freq = settings.frequency / cast<float>(dispatch_size_x());
        $for(oct, settings.octave) {
            v += perlin(make_float3(dispatch_id()) * freq) / scale;
            freq *= 2.f;
            scale *= settings.power;
        };
        out.write(dispatch_id(), make_float4(v));
    });

    Callable calculate_shading = [](Float3 surface_color,
                                    Float3 view_direction,
                                    Float3 surface_normal,
                                    Float3 light_direction) noexcept {
        auto shaded_color = surface_color;
        auto world_up_direction = make_float3(0.f, 1.f, 0.f);
        auto reflected_light_direction = normalize(reflect(-light_direction, surface_normal));

        // Diffuse + Specular
        auto light_intensity = max(dot(surface_normal, light_direction) +
                                       pow(max(0.f, dot(reflected_light_direction,
                                                        view_direction)),
                                           16.f),
                                   0.f);
        shaded_color *= light_intensity;

        // Ambient term (sky effect)
        auto sky_ambient_color = lerp(make_float3(.1f, .1f, .4f),
                                      make_float3(.8f, .6f, .2f),
                                      dot(surface_normal, world_up_direction) * .5f + .5f) *
                                 .2f;
        return shaded_color + sky_ambient_color;
    };

    Callable intersect_cube = [](Var<TRay> ray, Var<Bbox> bbox,
                                 Float3 &p1, Float3 &p2) noexcept {
        auto inv_dir = make_float3(1.f) / ray.direction;
        auto t_min = (bbox.min - ray.origin) * inv_dir;
        auto t_max = (bbox.max - ray.origin) * inv_dir;
        auto t1 = min(t_min, t_max);
        auto t2 = max(t_min, t_max);
        auto t_near = max(max(t1.x, t1.y), t1.z);
        auto t_far = min(min(t2.x, t2.y), t2.z);
        auto hit = t_near <= t_far & t_far > 0.f;
        $if(hit) {
            p1 = ray.origin + ray.direction * max(t_near, 0.f);
            p2 = ray.origin + ray.direction * t_far;
        };
        return hit;
    };

    Callable compute_volume_gradient = [](BindlessVar bindless, Float3 p, Float voxel_size) noexcept {
        auto v = [&bindless](auto p) noexcept { return bindless.tex3d(0u).sample(p).x; };
        auto inc = voxel_size * .5f;
        auto dx = v(p - make_float3(inc, 0.f, 0.f)) - v(p + make_float3(inc, 0.f, 0.f));
        auto dy = v(p - make_float3(0.f, inc, 0.f)) - v(p + make_float3(0.f, inc, 0.f));
        auto dz = v(p - make_float3(0.f, 0.f, inc)) - v(p + make_float3(0.f, 0.f, inc));
        return normalize(make_float3(dx, dy, dz));
    };

    Callable ray_marching = [](BindlessVar bindless,
                               Float3 p1, Float3 p2,
                               Int num_steps,
                               Float threshold,
                               Float3 &hit_point) noexcept {
        auto v = [&bindless](auto p) noexcept { return bindless.tex3d(0u).sample(p).x; };
        auto step_size = (p2 - p1) / cast<float>(num_steps);
        hit_point = p1;
        auto prev_point = hit_point;
        auto value = v(hit_point);
        auto prev_value = value;
        auto hit = def(false);
        $for(i, num_steps) {
            $if(value > threshold) {
                auto t = clamp((threshold - prev_value) / (value - prev_value), 0.f, 1.f);
                hit_point = lerp(prev_point, hit_point, t);
                hit = true;
                $break;
            };
            prev_value = value;
            prev_point = hit_point;
            hit_point += step_size;
            value = v(hit_point);
        };
        return hit;
    };

    Callable trace = [&](BindlessVar bindless,
                         Float3 camera_pos,
                         Float fov,
                         Float2 jitter) noexcept {
        auto size = make_float2(dispatch_size().xy());
        auto uv = (make_float2(dispatch_id().xy()) + jitter) / size * 2.f - 1.f;
        auto aspect = size.x / size.y;
        auto scale = tan(radians(fov) * .5f);
        auto p = make_float2(uv.x * aspect * scale, uv.y * scale);
        auto front = normalize(-camera_pos);
        auto right = normalize(cross(front, make_float3(0.f, 1.f, 0.f)));
        auto up = normalize(cross(right, front));
        auto ray_dir = normalize(p.x * right + p.y * up + front);
        auto ray = def<TRay>(camera_pos, ray_dir);

        auto p1 = def(make_float3(0.f));
        auto p2 = def(make_float3(0.f));
        auto bbox = def<Bbox>(make_float3(-.48f), make_float3(.48f));

        auto color = def(make_float3(.2f, .4f, .6f));
        $if(intersect_cube(ray, bbox, p1, p2)) {
            p1 = p1 - bbox.min / (bbox.max - bbox.min);
            p2 = p2 - bbox.min / (bbox.max - bbox.min);
            auto hit_point = def(make_float3(0.f));
            constexpr auto steps = 100;
            constexpr auto threshold = 5e-3f;
            $if(ray_marching(bindless, p1, p2, steps, threshold, hit_point)) {
                auto volume_size = cast<float>(bindless.tex3d(0u).size().x);
                auto normal = -compute_volume_gradient(bindless, hit_point, 1.f / volume_size);
                auto to_light = normalize(make_float3(1.f));
                $if(dot(normal, ray.direction) > 0.f & dot(normal, to_light) > 0.f) {
                    color = calculate_shading(make_float3(.5f), -ray.direction, normal, to_light);
                }
                $else {
                    color = make_float3(0.f);
                };
            };
        };
        return color;
    };

    auto render = device.compile<2>([&](ImageFloat accum,
                                        BindlessVar bindless,
                                        Float3 camera_pos,
                                        Float fov) noexcept {
        auto color = def(make_float3(0.f));
        auto samples = 2u;
        $for(dy, samples) {
            $for(dx, samples) {
                auto jitter = make_float2(make_uint2(dx, dy)) + .5f;
                color += trace(bindless, camera_pos, fov, jitter / cast<float>(samples));
            };
        };
        auto scale = 1.f / cast<float>(samples * samples);
        accum.write(dispatch_id().xy(), make_float4(color * scale, 1.f));
    });

    static constexpr auto volume_size = 256u;
    static constexpr auto settings = PerlinSettings{
        .octave = 4, .power = 1.f, .frequency = 1.f};

    auto stream = device.create_stream(StreamTag::GRAPHICS);
    auto bindless = device.create_bindless_array(1u);
    auto volume = device.create_volume<float>(PixelStorage::FLOAT1, make_uint3(volume_size));
    bindless.emplace_on_update(0u, volume, Sampler::linear_point_edge());

    stream << bindless.update()
           << make_perlin_noise(volume, settings)
                  .dispatch(make_uint3(volume_size));

    auto resolution = make_uint2(1024u);
    Window window{"Display", resolution};
    auto swapchain = device.create_swapchain(
        window.native_handle(), stream,
        resolution, false, true, 3u);
    auto image = device.create_image<float>(
        swapchain.backend_storage(), resolution);

    auto fov = 30.f;
    auto camera_pos = make_float3(2.f, -2.f, -2.f);
    Clock clk;
    while (!window.should_close()) {
        window.poll_events();
        auto dt = static_cast<float>(clk.toc() * 1e-3f);
        clk.tic();
        if (window.is_key_down(KEY_W)) {
            auto R = rotation(make_float3(1.f, 0.f, 0.f), dt);
            camera_pos = make_float3x3(R) * camera_pos;
        }
        if (window.is_key_down(KEY_S)) {
            auto R = rotation(make_float3(1.f, 0.f, 0.f), -dt);
            camera_pos = make_float3x3(R) * camera_pos;
        }
        if (window.is_key_down(KEY_A)) {
            auto R = rotation(make_float3(0.f, 1.f, 0.f), -dt);
            camera_pos = make_float3x3(R) * camera_pos;
        }
        if (window.is_key_down(KEY_D)) {
            auto R = rotation(make_float3(0.f, 1.f, 0.f), dt);
            camera_pos = make_float3x3(R) * camera_pos;
        }
        if (window.is_key_down(KEY_MINUS)) {
            fov = clamp(fov * 1.02f, 5.f, 170.f);
        }
        if (window.is_key_down(KEY_EQUAL)) {
            fov = clamp(fov / 1.02f, 5.f, 170.f);
        }
        LUISA_INFO("Camera: ({}, {}, {})", camera_pos.x, camera_pos.y, camera_pos.z);
        stream << render(image, bindless, camera_pos, fov)
                      .dispatch(resolution)
               << swapchain.present(image);
    }
    stream << synchronize();
}
