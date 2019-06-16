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
    void cameraUpdate();   // camera����

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
    // ���νṹmap
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_mapGeometries;
    // ����map
    std::unordered_map<std::string, std::unique_ptr<Material>> m_mapMaterial;

    // ��Ⱦ����
    enum class RenderLayer : int
    {
        Opaque = 0,
        Count
    };
    std::vector<RenderItem*> m_vecRenderItems[(int)RenderLayer::Count];
    std::vector<std::unique_ptr<RenderItem>> m_vecAll;

    // �洢���е���������
    StructuredBuffer m_mats;
    // �洢���е�������Դ
    D3D12_CPU_DESCRIPTOR_HANDLE m_srvs[4];

private:
    // ��ǩ��
    RootSignature m_RootSignature;

    // ��Ⱦ��ˮ��
    enum ePSOType
    {
        E_EPT_DEFAULT = 1,
    };
    std::unordered_map<int, GraphicsPSO> m_mapPSO;


    // �����
    // ��(0, 0, -m_radius) Ϊ��ʼλ��
    Math::Camera m_Camera;
    Math::Matrix4 m_ViewProjMatrix;
    D3D12_VIEWPORT m_MainViewport;
    D3D12_RECT m_MainScissor;

    // �����������
    std::auto_ptr<GameCore::CameraController> m_CameraController;

    // �뾶
    float m_radius = 60.0f;

    // x���򻡶ȣ��������x�������ӣ���m_xRotate����
    float m_xRotate = -Math::XM_PIDIV4 / 2.0f;
    float m_xLast = 0.0f;
    float m_xDiff = 0.0f;

    // y���򻡶ȣ������y�������ӣ���m_yRotate����
    // m_yRotate��Χ [-XM_PIDIV2 + 0.1f, XM_PIDIV2 - 0.1f]
    float m_yRotate = Math::XM_PIDIV4 / 2.0f;
    float m_yLast = 0.0f;
    float m_yDiff = 0.0f;
};