const MAX_DIRECTIONAL_LIGHTS : u32 = 2u;
const MAX_POINT_LIGHTS : u32 = 8u;

struct GlobalUniform {
    viewProjection : mat4x4<f32>,
    lightViewProjectionNear : mat4x4<f32>,
    lightViewProjectionFar : mat4x4<f32>,
    cameraPosition : vec4<f32>,
    directionalDirectionIntensity : array<vec4<f32>, MAX_DIRECTIONAL_LIGHTS>,
    directionalColor : array<vec4<f32>, MAX_DIRECTIONAL_LIGHTS>,
    pointPositionRange : array<vec4<f32>, MAX_POINT_LIGHTS>,
    pointColorIntensity : array<vec4<f32>, MAX_POINT_LIGHTS>,
    lightCounts : vec4<f32>, // x=directionalCount y=pointCount z=ambientIntensity
    shadowParams : vec4<f32>, // x=constantBias y=slopeBias z=pcfEnabled
    shadowCascadeParams : vec4<f32>, // x=splitDistance y=enableCascades z=blendDistance
};

struct ObjectUniform {
    model : mat4x4<f32>,
    normalMatrix : mat4x4<f32>,
    baseColor : vec4<f32>,
    emissive : vec4<f32>,
    material : vec4<f32>, // x=metallic y=roughness z=ao
    grid : vec4<f32>, // x=columns y=rows z=spacing w=enabled(0/1)
};

@group(0) @binding(0)
var<uniform> uGlobal : GlobalUniform;

@group(1) @binding(0)
var<uniform> uObject : ObjectUniform;

@group(2) @binding(0)
var shadowDepthTexNear : texture_depth_2d;
@group(2) @binding(1)
var shadowDepthTexFar : texture_depth_2d;
@group(2) @binding(2)
var shadowSampler : sampler_comparison;

struct VertexIn {
    @location(0) position : vec3<f32>,
    @location(1) normal : vec3<f32>,
    @location(2) uv : vec2<f32>,
};

struct VertexOut {
    @builtin(position) position : vec4<f32>,
    @location(0) worldPosition : vec3<f32>,
    @location(1) normal : vec3<f32>,
    @location(2) uv : vec2<f32>,
    @location(3) metallicRoughness : vec2<f32>,
    @location(4) lightClipPositionNear : vec4<f32>,
    @location(5) lightClipPositionFar : vec4<f32>,
};

@vertex
fn vs_main(input : VertexIn, @builtin(instance_index) instanceIndex : u32) -> VertexOut {
    var out : VertexOut;
    let gridEnabled = uObject.grid.w > 0.5;
    let columns = max(u32(uObject.grid.x), 1u);
    let rows = max(u32(uObject.grid.y), 1u);
    let spacing = max(uObject.grid.z, 0.01);

    var position = input.position;
    var metallic = clamp(uObject.material.x, 0.0, 1.0);
    var roughness = clamp(uObject.material.y, 0.045, 1.0);
    if (gridEnabled) {
        let col = instanceIndex % columns;
        let row = min(instanceIndex / columns, rows - 1u);

        let xOffset = (f32(col) - 0.5 * f32(columns - 1u)) * spacing;
        let yOffset = (0.5 * f32(rows - 1u) - f32(row)) * spacing;
        position = position + vec3<f32>(xOffset, yOffset, 0.0);

        if (columns > 1u) {
            metallic = clamp(f32(col) / f32(columns - 1u), 0.0, 1.0);
        }
        if (rows > 1u) {
            roughness = clamp(f32(row) / f32(rows - 1u), 0.045, 1.0);
        }
    }

    let worldPos = uObject.model * vec4<f32>(position, 1.0);
    out.position = uGlobal.viewProjection * worldPos;
    out.worldPosition = worldPos.xyz;
    out.lightClipPositionNear = uGlobal.lightViewProjectionNear * worldPos;
    out.lightClipPositionFar = uGlobal.lightViewProjectionFar * worldPos;
    let worldNormal = (uObject.normalMatrix * vec4<f32>(input.normal, 0.0)).xyz;
    out.normal = normalize(worldNormal);
    out.uv = input.uv;
    out.metallicRoughness = vec2<f32>(metallic, roughness);
    return out;
}

fn distribution_ggx(n : vec3<f32>, h : vec3<f32>, roughness : f32) -> f32 {
    let a = roughness * roughness;
    let a2 = a * a;
    let nDotH = max(dot(n, h), 0.0);
    let nDotH2 = nDotH * nDotH;
    let num = a2;
    let denom = (nDotH2 * (a2 - 1.0) + 1.0);
    return num / max(3.14159265 * denom * denom, 0.00001);
}

fn geometry_schlick_ggx(nDotV : f32, roughness : f32) -> f32 {
    let r = roughness + 1.0;
    let k = (r * r) / 8.0;
    let num = nDotV;
    let denom = nDotV * (1.0 - k) + k;
    return num / max(denom, 0.00001);
}

fn geometry_smith(n : vec3<f32>, v : vec3<f32>, l : vec3<f32>, roughness : f32) -> f32 {
    let nDotV = max(dot(n, v), 0.0);
    let nDotL = max(dot(n, l), 0.0);
    let ggx2 = geometry_schlick_ggx(nDotV, roughness);
    let ggx1 = geometry_schlick_ggx(nDotL, roughness);
    return ggx1 * ggx2;
}

fn fresnel_schlick(cosTheta : f32, f0 : vec3<f32>) -> vec3<f32> {
    return f0 + (vec3<f32>(1.0, 1.0, 1.0) - f0) * pow(1.0 - cosTheta, 5.0);
}

fn evaluate_direct_light(
    n : vec3<f32>,
    v : vec3<f32>,
    l : vec3<f32>,
    radiance : vec3<f32>,
    albedo : vec3<f32>,
    metallic : f32,
    roughness : f32,
    f0 : vec3<f32>) -> vec3<f32> {
    let h = normalize(v + l);
    let ndf = distribution_ggx(n, h, roughness);
    let g = geometry_smith(n, v, l, roughness);
    let f = fresnel_schlick(max(dot(h, v), 0.0), f0);

    let numerator = ndf * g * f;
    let denominator = max(4.0 * max(dot(n, v), 0.0) * max(dot(n, l), 0.0), 0.0001);
    let specular = numerator / denominator;

    let ks = f;
    let kd = (vec3<f32>(1.0, 1.0, 1.0) - ks) * (1.0 - metallic);
    let nDotL = max(dot(n, l), 0.0);
    return (kd * albedo / 3.14159265 + specular) * radiance * nDotL;
}

fn sample_shadow_factor(
    lightClipPosition : vec4<f32>,
    normal : vec3<f32>,
    lightDir : vec3<f32>,
    useFarCascade : bool) -> f32 {
    let clip = lightClipPosition.xyz / max(lightClipPosition.w, 0.0001);
    let uv = clip.xy * 0.5 + vec2<f32>(0.5, 0.5);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
        return 1.0;
    }

    let depth = clip.z * 0.5 + 0.5;
    if (depth <= 0.0 || depth >= 1.0) {
        return 1.0;
    }

    let nDotL = max(dot(normal, lightDir), 0.0);
    let baseBias = max(uGlobal.shadowParams.x, 0.0);
    let slopeBias = max(uGlobal.shadowParams.y, 0.0);
    let bias = max(baseBias, slopeBias * (1.0 - nDotL));

    if (uGlobal.shadowParams.z < 0.5) {
        if (useFarCascade) {
            return textureSampleCompare(shadowDepthTexFar, shadowSampler, uv, depth - bias);
        }
        return textureSampleCompare(shadowDepthTexNear, shadowSampler, uv, depth - bias);
    }

    var dims = vec2<f32>(textureDimensions(shadowDepthTexNear));
    if (useFarCascade) {
        dims = vec2<f32>(textureDimensions(shadowDepthTexFar));
    }
    let safeDims = max(dims, vec2<f32>(1.0, 1.0));
    let texelSize = vec2<f32>(1.0, 1.0) / safeDims;
    var sum = 0.0;
    for (var y : i32 = -1; y <= 1; y = y + 1) {
        for (var x : i32 = -1; x <= 1; x = x + 1) {
            let sampleUv = uv + vec2<f32>(f32(x), f32(y)) * texelSize;
            if (useFarCascade) {
                sum += textureSampleCompare(shadowDepthTexFar, shadowSampler, sampleUv, depth - bias);
            } else {
                sum += textureSampleCompare(shadowDepthTexNear, shadowSampler, sampleUv, depth - bias);
            }
        }
    }
    return sum / 9.0;
}

@fragment
fn fs_main(input : VertexOut) -> @location(0) vec4<f32> {
    let albedo = uObject.baseColor.rgb;
    let metallic = clamp(input.metallicRoughness.x, 0.0, 1.0);
    let roughness = clamp(input.metallicRoughness.y, 0.045, 1.0);
    let ao = clamp(uObject.material.z, 0.0, 1.0);

    let n = normalize(input.normal);
    let v = normalize(uGlobal.cameraPosition.xyz - input.worldPosition);

    var f0 = vec3<f32>(0.04, 0.04, 0.04);
    f0 = mix(f0, albedo, vec3<f32>(metallic, metallic, metallic));

    let directionalCount = u32(clamp(uGlobal.lightCounts.x, 0.0, f32(MAX_DIRECTIONAL_LIGHTS)));
    let pointCount = u32(clamp(uGlobal.lightCounts.y, 0.0, f32(MAX_POINT_LIGHTS)));

    var direct = vec3<f32>(0.0, 0.0, 0.0);

    for (var i : u32 = 0u; i < MAX_DIRECTIONAL_LIGHTS; i = i + 1u) {
        if (i >= directionalCount) {
            break;
        }
        let lightDir = normalize(-uGlobal.directionalDirectionIntensity[i].xyz);
        let intensity = max(uGlobal.directionalDirectionIntensity[i].w, 0.0);
        let lightColor = max(uGlobal.directionalColor[i].xyz, vec3<f32>(0.0, 0.0, 0.0));
        var shadowFactor = 1.0;
        if (i == 0u) {
            let cascadeEnabled = uGlobal.shadowCascadeParams.y > 0.5;
            let splitDistance = max(uGlobal.shadowCascadeParams.x, 0.01);
            let blendDistance = max(uGlobal.shadowCascadeParams.z, 0.001);
            let viewDistance = length(uGlobal.cameraPosition.xyz - input.worldPosition);
            if (cascadeEnabled) {
                let nearShadow = sample_shadow_factor(input.lightClipPositionNear, n, lightDir, false);
                let farShadow = sample_shadow_factor(input.lightClipPositionFar, n, lightDir, true);
                let blend = smoothstep(splitDistance - blendDistance, splitDistance + blendDistance, viewDistance);
                shadowFactor = mix(nearShadow, farShadow, blend);
            } else {
                shadowFactor = sample_shadow_factor(input.lightClipPositionNear, n, lightDir, false);
            }
        }
        direct += evaluate_direct_light(
            n, v, lightDir, lightColor * intensity * shadowFactor, albedo, metallic, roughness, f0);
    }

    for (var i : u32 = 0u; i < MAX_POINT_LIGHTS; i = i + 1u) {
        if (i >= pointCount) {
            break;
        }
        let lightPos = uGlobal.pointPositionRange[i].xyz;
        let range = max(uGlobal.pointPositionRange[i].w, 0.001);
        let toLight = lightPos - input.worldPosition;
        let distance = length(toLight);
        if (distance >= range) {
            continue;
        }
        let l = toLight / max(distance, 0.0001);
        let attenuation = pow(max(1.0 - distance / range, 0.0), 2.0);
        let lightColor = max(uGlobal.pointColorIntensity[i].xyz, vec3<f32>(0.0, 0.0, 0.0));
        let intensity = max(uGlobal.pointColorIntensity[i].w, 0.0);
        direct += evaluate_direct_light(
            n, v, l, lightColor * intensity * attenuation, albedo, metallic, roughness, f0);
    }

    let ambient = albedo * max(uGlobal.lightCounts.z, 0.0) * ao;
    let color = max(ambient + direct + uObject.emissive.rgb, vec3<f32>(0.0, 0.0, 0.0));
    return vec4<f32>(color, uObject.baseColor.a);
}
