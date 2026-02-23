struct GlobalUniform {
    viewProjection : mat4x4<f32>,
    lightViewProjectionNear : mat4x4<f32>,
    lightViewProjectionFar : mat4x4<f32>,
    cameraPosition : vec4<f32>,
    directionalDirectionIntensity : array<vec4<f32>, 2>,
    directionalColor : array<vec4<f32>, 2>,
    pointPositionRange : array<vec4<f32>, 8>,
    pointColorIntensity : array<vec4<f32>, 8>,
    lightCounts : vec4<f32>,
    shadowParams : vec4<f32>,
    shadowCascadeParams : vec4<f32>,
};

struct ObjectUniform {
    model : mat4x4<f32>,
    normalMatrix : mat4x4<f32>,
    baseColor : vec4<f32>,
    emissive : vec4<f32>,
    material : vec4<f32>,
    grid : vec4<f32>,
};

@group(0) @binding(0)
var<uniform> uGlobal : GlobalUniform;

@group(1) @binding(0)
var<uniform> uObject : ObjectUniform;

struct VertexIn {
    @location(0) position : vec3<f32>,
    @location(1) normal : vec3<f32>,
    @location(2) uv : vec2<f32>,
};

@vertex
fn vs_main(input : VertexIn, @builtin(instance_index) instanceIndex : u32) -> @builtin(position) vec4<f32> {
    let gridEnabled = uObject.grid.w > 0.5;
    let columns = max(u32(uObject.grid.x), 1u);
    let rows = max(u32(uObject.grid.y), 1u);
    let spacing = max(uObject.grid.z, 0.01);

    var position = input.position;
    if (gridEnabled) {
        let col = instanceIndex % columns;
        let row = min(instanceIndex / columns, rows - 1u);
        let xOffset = (f32(col) - 0.5 * f32(columns - 1u)) * spacing;
        let yOffset = (0.5 * f32(rows - 1u) - f32(row)) * spacing;
        position = position + vec3<f32>(xOffset, yOffset, 0.0);
    }

    let worldPos = uObject.model * vec4<f32>(position, 1.0);
    return uGlobal.lightViewProjectionNear * worldPos;
}

@fragment
fn fs_main() -> @location(0) vec4<f32> {
    return vec4<f32>(1.0, 1.0, 1.0, 1.0);
}
