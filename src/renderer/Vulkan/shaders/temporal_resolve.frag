#version 450

layout(set = 0, binding = 0) uniform sampler2D currentScene;
layout(set = 1, binding = 0) uniform sampler2D sceneDepth;
layout(set = 2, binding = 0) uniform sampler2D historyScene;

layout(std140, set = 6, binding = 0) uniform TemporalResolveBlock {
    // xy = inverse scene extent; zw = native output extent.
    vec4 sceneOutputExtent;
    // xy = inverse projection scale; zw = current projection offsets.
    vec4 currentReconstruct;
    // xy = previous projection scale; zw = previous jittered offsets.
    vec4 previousProject;
    // xy = current depth projection; z = feedback; w = reactive scale.
    vec4 depthFeedback;
    vec4 currentViewOrigin;
    vec4 currentViewAxis0;
    vec4 currentViewAxis1;
    vec4 currentViewAxis2;
    vec4 previousViewOrigin;
    vec4 previousViewAxis0;
    vec4 previousViewAxis1;
    vec4 previousViewAxis2;
    // xy = current jitter in native normalized coordinates;
    // z = use history; w = debug mode.
    vec4 temporalParams;
    // x = use object velocity (currently zero on Vulkan);
    // y = camera reprojection valid; z = depth valid;
    // w = recenter the current jitter for capture/spatial fallback.
    vec4 motionParams;
    // Conservative current-frame regions whose packet motion domains lack an
    // exact Vulkan velocity stream. Invalid entries have non-positive extent.
    vec4 reactiveRect0;
    vec4 reactiveRect1;
} temporal;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

const float kDepthEpsilon = 0.00001;
vec2 TextureToCameraUV(vec2 uv) {
    return vec2(uv.x, 1.0 - uv.y);
}

vec2 CameraToTextureUV(vec2 uv) {
    return vec2(uv.x, 1.0 - uv.y);
}

float ViewZFromDepth(float depth) {
    float denominator = depth * 2.0 - 1.0 + temporal.depthFeedback.x;
    if (abs(denominator) < kDepthEpsilon) {
        denominator = denominator < 0.0 ? -kDepthEpsilon : kDepthEpsilon;
    }
    return -temporal.depthFeedback.y / denominator;
}

vec3 CurrentViewToWorldDirection(vec3 viewPosition) {
    return temporal.currentViewAxis0.xyz * (-viewPosition.z)
        + temporal.currentViewAxis1.xyz * (-viewPosition.x)
        + temporal.currentViewAxis2.xyz * viewPosition.y;
}

vec3 WorldToPreviousViewDirection(vec3 worldDirection) {
    return vec3(
        -dot(worldDirection, temporal.previousViewAxis1.xyz),
        dot(worldDirection, temporal.previousViewAxis2.xyz),
        -dot(worldDirection, temporal.previousViewAxis0.xyz));
}

vec2 ProjectPreviousView(vec3 previousViewPosition) {
    float w = -previousViewPosition.z;
    if (abs(w) < kDepthEpsilon) {
        return vec2(-1000.0);
    }
    vec2 clipXY = temporal.previousProject.xy * previousViewPosition.xy
        + temporal.previousProject.zw * previousViewPosition.z;
    return clipXY / w * 0.5 + 0.5;
}

vec2 CameraPreviousUV(vec2 cameraUV, float depth) {
    vec2 ndc = cameraUV * 2.0 - 1.0;
    if (depth >= 0.99999) {
        vec3 ray = vec3(
            -(ndc.x + temporal.currentReconstruct.z)
                * temporal.currentReconstruct.x,
            -(ndc.y + temporal.currentReconstruct.w)
                * temporal.currentReconstruct.y,
            -1.0);
        return ProjectPreviousView(WorldToPreviousViewDirection(
            CurrentViewToWorldDirection(ray)));
    }
    float viewZ = ViewZFromDepth(depth);
    vec3 viewPosition = vec3(
        -viewZ * (ndc.x + temporal.currentReconstruct.z)
            * temporal.currentReconstruct.x,
        -viewZ * (ndc.y + temporal.currentReconstruct.w)
            * temporal.currentReconstruct.y,
        viewZ);
    vec3 worldPosition = temporal.currentViewOrigin.xyz
        + CurrentViewToWorldDirection(viewPosition);
    vec3 delta = worldPosition - temporal.previousViewOrigin.xyz;
    return ProjectPreviousView(vec3(
        -dot(delta, temporal.previousViewAxis1.xyz),
        dot(delta, temporal.previousViewAxis2.xyz),
        -dot(delta, temporal.previousViewAxis0.xyz)));
}

float MaxComponent(vec3 value) {
    return max(value.x, max(value.y, value.z));
}

bool InsideReactiveRect(vec2 cameraUV, vec4 rect) {
    return rect.z > rect.x && rect.w > rect.y
        && all(greaterThanEqual(cameraUV, rect.xy))
        && all(lessThan(cameraUV, rect.zw));
}

void main() {
    vec2 outputCameraUV = TextureToCameraUV(fragUV);
    vec2 sceneCameraUV = clamp(outputCameraUV
        - temporal.temporalParams.xy * temporal.motionParams.w,
        vec2(0.0), vec2(1.0));
    vec2 sceneTextureUV = CameraToTextureUV(sceneCameraUV);
    vec4 current = texture(currentScene, sceneTextureUV);
    vec3 neighborhoodMin = current.rgb;
    vec3 neighborhoodMax = current.rgb;
    float centerDepth = texture(sceneDepth, sceneTextureUV).r;
    float depthMin = centerDepth;
    float depthMax = centerDepth;
    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            vec2 sampleCameraUV = clamp(sceneCameraUV
                + vec2(float(x), float(y)) * temporal.sceneOutputExtent.xy,
                vec2(0.0), vec2(1.0));
            vec2 sampleTextureUV = CameraToTextureUV(sampleCameraUV);
            vec3 sampleColor = texture(currentScene, sampleTextureUV).rgb;
            neighborhoodMin = min(neighborhoodMin, sampleColor);
            neighborhoodMax = max(neighborhoodMax, sampleColor);
            float sampleDepth = texture(sceneDepth, sampleTextureUV).r;
            depthMin = min(depthMin, sampleDepth);
            depthMax = max(depthMax, sampleDepth);
        }
    }

    // Vulkan currently has no dedicated rigid/skinned velocity target. Keep
    // the object-valid branch in the shared contract disabled and rely on
    // camera reprojection plus conservative rejection for moving geometry.
    bool objectValid = false;
    bool cameraValid = temporal.motionParams.y > 0.5
        && temporal.motionParams.z > 0.5;
    vec2 previousCameraUV = cameraValid
        ? CameraPreviousUV(sceneCameraUV, centerDepth)
        : outputCameraUV;
    bool inside = previousCameraUV.x >= 0.0
        && previousCameraUV.y >= 0.0
        && previousCameraUV.x <= 1.0
        && previousCameraUV.y <= 1.0;
    bool historyUsable = temporal.temporalParams.z > 0.5
        && inside && (objectValid || cameraValid);
    vec3 historyRaw = historyUsable
        ? texture(historyScene, CameraToTextureUV(previousCameraUV)).rgb
        : current.rgb;
    vec3 historyClamped = clamp(historyRaw, neighborhoodMin, neighborhoodMax);
    float colorDelta = MaxComponent(abs(current.rgb - historyRaw));
    float clampDelta = MaxComponent(abs(historyRaw - historyClamped));
    vec2 velocityPixels = (outputCameraUV - previousCameraUV)
        * temporal.sceneOutputExtent.zw;
    float motionReactive = smoothstep(12.0, 96.0,
        length(velocityPixels)) * 0.35;
    float depthReactive = temporal.motionParams.z > 0.5
        ? smoothstep(0.001, 0.02, depthMax - depthMin) * 0.20
        : 0.0;
    float unsupportedNear = (temporal.motionParams.z > 0.5 && !objectValid)
        ? (1.0 - smoothstep(0.82, 0.98, centerDepth)) * 0.25
        : 0.0;
    float packetReactive = (InsideReactiveRect(outputCameraUV,
            temporal.reactiveRect0) || InsideReactiveRect(outputCameraUV,
            temporal.reactiveRect1)) ? 1.0 : 0.0;
    float reactive = clamp(max(
        max(colorDelta * 2.5, clampDelta * 5.0)
            * temporal.depthFeedback.w,
        max(motionReactive, max(depthReactive, unsupportedNear))),
        0.0, 1.0);
    reactive = max(reactive, packetReactive);
    if (!historyUsable) {
        reactive = 1.0;
    }
    float historyWeight = historyUsable
        ? temporal.depthFeedback.z * (1.0 - reactive)
        : 0.0;
    vec3 resolved = mix(current.rgb, historyClamped, historyWeight);
    if (temporal.temporalParams.w > 0.5
            && temporal.temporalParams.w < 1.5) {
        float magnitude = clamp(length(velocityPixels) / 32.0, 0.0, 1.0);
        vec2 direction = clamp(velocityPixels / 32.0,
            vec2(-1.0), vec2(1.0));
        resolved = vec3(direction * 0.5 + 0.5, 1.0) * magnitude;
    } else if (temporal.temporalParams.w > 1.5
            && temporal.temporalParams.w < 2.5) {
        resolved = vec3(reactive, reactive * 0.25, 0.0);
    } else if (temporal.temporalParams.w > 2.5) {
        resolved = vec3(historyWeight);
    }
    outColor = vec4(resolved, current.a);
}
