#pragma once

#include "VectorMath.h"

using namespace Math;
// 与HLSL一致
struct Light
{
    XMFLOAT3 Strength = { 0.0f, 0.0f, 0.05f };
    float FalloffStart = 0.0f;                          // point/spot light only
    XMFLOAT3 Direction = { 0.0f, 0.0f, 0.0f };          // directional/spot light only
    float FalloffEnd = 0.0f;                           // point/spot light only
    XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };            // point/spot light only
    float SpotPower = 0;                            // spot light only
};

#define MaxLights 16

__declspec(align(16)) struct ObjectConstants
{
    Matrix4 World = Matrix4(kIdentity); // 把物体从模型坐标转换到世界坐标
    Matrix4 texTransform = Matrix4(kIdentity); // 该顶点所用纹理的转换矩阵
    Matrix4 matTransform = Matrix4(kIdentity);
};

__declspec(align(16)) struct PassConstants
{
    Matrix4 viewProj = Matrix4(kIdentity);      // 从世界坐标转为投影坐标的矩阵
    Vector3 eyePosW = { 0.0f, 0.0f, 0.0f };     // 观察点也就是摄像机位置
    Vector4 ambientLight = { 0.0f, 0.0f, 0.0f, 1.0f };

    Vector4 FogColor = { 0.7f, 0.7f, 0.7f, 0.3f };
    float gFogStart = 40.0f;
    float gFogRange = 150.0f;
    XMFLOAT2 pad;

    // Indices [0, NUM_DIR_LIGHTS) are directional lights;
    // indices [NUM_DIR_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHTS) are point lights;
    // indices [NUM_DIR_LIGHTS+NUM_POINT_LIGHTS, NUM_DIR_LIGHTS+NUM_POINT_LIGHT+NUM_SPOT_LIGHTS)
    // are spot lights for a maximum of MaxLights per object.
    Light Lights[MaxLights];
};

__declspec(align(16)) struct MaterialConstants
{
    Vector4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
    Vector3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
    float Roughness = 0.25f;
};