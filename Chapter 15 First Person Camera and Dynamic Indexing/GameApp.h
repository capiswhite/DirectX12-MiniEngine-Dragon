#pragma once

#include <unordered_map>
#include "GameCore.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "Camera.h"
#include "d3dUtil.h"
#include "CameraController.h"

class RootSignature;
class GraphicsPSO;
class GameApp : public GameCore::IGameApp
{
public:

	GameApp(void) {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:
    void cameraUpdate();   // camera更新

private:
    void buildPSO();
    void buildGeo();
    void buildMaterials();
    void buildRenderItem();
    void drawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& ritems);

private:
    inline void makeMaterials(const std::string& name, const Math::Vector4& diffuseAlbedo, const Math::Vector3& fresnelR0, 
        const float roughness, const std::string& materialName, int idx);

private:
    // 几何结构map
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_mapGeometries;
    // 纹理map
    std::unordered_map<std::string, std::unique_ptr<Material>> m_mapMaterial;

    // 渲染队列
    enum class RenderLayer : int
    {
        Opaque = 0,
        Count
    };
    std::vector<RenderItem*> m_vecRenderItems[(int)RenderLayer::Count];
    std::vector<std::unique_ptr<RenderItem>> m_vecAll;

    // 存储所有的纹理参数
    StructuredBuffer m_mats;
    // 存储所有的纹理资源
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvs[4];

private:
    // 根签名
    RootSignature m_RootSignature;

    // 渲染流水线
    enum ePSOType
    {
        E_EPT_DEFAULT = 1,
    };
    std::unordered_map<int, GraphicsPSO> m_mapPSO;


    // 摄像机
    // 以(0, 0, -m_radius) 为初始位置
    Math::Camera m_Camera;
    Math::Matrix4 m_ViewProjMatrix;
    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    // 摄像机控制器
    std::auto_ptr<GameCore::CameraController> m_CameraController;

    // 半径
    float m_radius = 60.0f;

    // x方向弧度，摄像机的x坐标增加，则m_xRotate增加
    float m_xRotate = -Math::XM_PIDIV4 / 2.0f;
    float m_xLast = 0.0f;
    float m_xDiff = 0.0f;

    // y方向弧度，摄像机y坐标增加，则m_yRotate增加
    // m_yRotate范围 [-XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f]
    float m_yRotate = Math::XM_PIDIV4 / 2.0f;
    float m_yLast = 0.0f;
    float m_yDiff = 0.0f;
};