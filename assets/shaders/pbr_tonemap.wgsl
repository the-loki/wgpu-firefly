@group(0) @binding(0)
var hdrInputTex : texture_2d<f32>;
@group(0) @binding(1)
var hdrSampler : sampler;

struct VertexOut {
    @builtin(position) position : vec4<f32>,
    @location(0) uv : vec2<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) index : u32) -> VertexOut {
    var out : VertexOut;
    var positions = array<vec2<f32>, 3>(
        vec2<f32>(-1.0, -3.0),
        vec2<f32>(-1.0,  1.0),
        vec2<f32>( 3.0,  1.0)
    );
    let pos = positions[index];
    out.position = vec4<f32>(pos, 0.0, 1.0);
    out.uv = pos * 0.5 + vec2<f32>(0.5, 0.5);
    return out;
}

fn aces_tonemap(x : vec3<f32>) -> vec3<f32> {
    let a = 2.51;
    let b = 0.03;
    let c = 2.43;
    let d = 0.59;
    let e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), vec3<f32>(0.0), vec3<f32>(1.0));
}

@fragment
fn fs_main(input : VertexOut) -> @location(0) vec4<f32> {
    let hdr = textureSampleLevel(hdrInputTex, hdrSampler, input.uv, 0.0).rgb;
    let mapped = aces_tonemap(hdr);
    let gammaCorrected = pow(mapped, vec3<f32>(1.0 / 2.2));
    return vec4<f32>(gammaCorrected, 1.0);
}
