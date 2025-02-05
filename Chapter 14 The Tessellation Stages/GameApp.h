#pragma once

#include <unordered_map>
#include "GameCore.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "Camera.h"
#include "d3dUtil.h"

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
    void buildQuadPatchGeo();
    void buildBezierGeo();
    void buildRenderItem();
    void drawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& ritems);

private:
    // 几何结构map
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_mapGeometries;

    // 渲染队列
    enum class RenderLayer : int
    {
        Opaque = 0,
        Bezier = 1,
        Count
    };
    std::vector<RenderItem*> m_vecRenderItems[(int)RenderLayer::Count];
    std::vector<std::unique_ptr<RenderItem>> m_vecAll;

private:
    // 根签名
    RootSignature m_RootSignature;

    // 渲染流水线
    enum ePSOType
    {
        E_EPT_DEFAULT = 1,
        E_EPT_BEZIER = 2,
    };
    std::unordered_map<int, GraphicsPSO> m_mapPSO;

    bool m_bShowBezier = true;

    // 摄像机
    // 以(0, 0, -m_radius) 为初始位置
    Math::Camera m_Camera;
    Math::Matrix4 m_ViewProjMatrix;
    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

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