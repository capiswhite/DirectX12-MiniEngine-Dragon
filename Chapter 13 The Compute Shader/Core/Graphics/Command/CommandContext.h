//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#pragma once

#include "pch.h"
#include "CommandListManager.h"
#include "Color.h"
#include "PipelineState.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "TextureManager.h"
#include "PixelBuffer.h"
#include "DynamicDescriptorHeap.h"
#include "LinearAllocator.h"
#include "CommandSignature.h"
#include "GraphicsCore.h"
#include <vector>

class ColorBuffer;
class DepthBuffer;
class Texture;
class GraphicsContext;
class ComputeContext;

struct DWParam
{
    DWParam( FLOAT f ) : Float(f) {}
    DWParam( UINT u ) : Uint(u) {}
    DWParam( INT i ) : Int(i) {}

    void operator= ( FLOAT f ) { Float = f; }
    void operator= ( UINT u ) { Uint = u; }
    void operator= ( INT i ) { Int = i; }

    union
    {
        FLOAT Float;
        UINT Uint;
        INT Int;
    };
};

#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )

class ContextManager
{
public:
    ContextManager(void) {}

    CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);
    void FreeContext(CommandContext*);
    void DestroyAllContexts();

private:
    std::vector<std::unique_ptr<CommandContext> > sm_ContextPool[4];
    std::queue<CommandContext*> sm_AvailableContexts[4];
    std::mutex sm_ContextAllocationMutex;
};

struct NonCopyable
{
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable & operator=(const NonCopyable&) = delete;
};

class CommandContext : NonCopyable
{
    friend ContextManager;
private:

    CommandContext(D3D12_COMMAND_LIST_TYPE Type);

    void Reset( void );

public:

    ~CommandContext(void);

    // 摧毁所有命令环境
    static void DestroyAllContexts(void);

    // 开始一个命令环境
    static CommandContext& Begin(const std::wstring ID = L"");

    // Flush existing commands to the GPU but keep the context alive
    uint64_t Flush( bool WaitForCompletion = false );

    // Flush existing commands and release the current context
    uint64_t Finish( bool WaitForCompletion = false );

    // Prepare to render by reserving a command list and command allocator
    void Initialize(void);

    // 图形命令上下文环境
    GraphicsContext& GetGraphicsContext() {
        ASSERT(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE, "Cannot convert async compute context to graphics");
        return reinterpret_cast<GraphicsContext&>(*this);
    }

    // 计算命令上下文环境
    ComputeContext& GetComputeContext() {
        return reinterpret_cast<ComputeContext&>(*this);
    }

    // 获取命令列表
    ID3D12GraphicsCommandList* GetCommandList() {
        return m_CommandList;
    }

    // 从src资源中拷贝数据到Dest资源中
    void CopyBuffer(GpuResource& Dest, GpuResource& Src);
    void CopyBufferRegion(GpuResource& Dest, size_t DestOffset, GpuResource& Src, size_t SrcOffset, size_t NumBytes);
    void CopySubresource(GpuResource& Dest, UINT DestSubIndex, GpuResource& Src, UINT SrcSubIndex);
    void CopyCounter(GpuResource& Dest, size_t DestOffset, StructuredBuffer& Src);
    void ResetCounter(StructuredBuffer& Buf, uint32_t Value = 0);

    // 准备上传缓冲区
    DynAlloc ReserveUploadMemory(size_t SizeInBytes)
    {
        return m_CpuLinearAllocator.Allocate(SizeInBytes);
    }

    static void InitializeTexture(GpuResource& Dest, UINT NumSubresources, D3D12_SUBRESOURCE_DATA SubData[]);
    static void InitializeBuffer(GpuResource& Dest, const void* Data, size_t NumBytes, size_t Offset = 0);
    static void InitializeTextureArraySlice(GpuResource& Dest, UINT SliceIndex, GpuResource& Src);
    static void ReadbackTexture2D(GpuResource& ReadbackBuffer, PixelBuffer& SrcBuffer);

    // 将数据写入给定的Dest资源中
    void WriteBuffer(GpuResource& Dest, size_t DestOffset, const void* Data, size_t NumBytes);
    // 将数据填充到给定的Dest资源中
    void FillBuffer(GpuResource& Dest, size_t DestOffset, DWParam Value, size_t NumBytes);

    // 修改一个资源的状态
    void TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
    void BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
    void InsertUAVBarrier(GpuResource& Resource, bool FlushImmediate = false);
    void InsertAliasBarrier(GpuResource& Before, GpuResource& After, bool FlushImmediate = false);
    // 修改资源状态实际上是放入了一个队列，这个是把资源状态的修改直接发送给gpu
    inline void FlushResourceBarriers(void);

    void InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx);
    void ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries);
    void PIXBeginEvent(const wchar_t* label);
    void PIXEndEvent(void);
    void PIXSetMarker(const wchar_t* label);

    // 设置描述符堆
    void SetDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr );
    void SetDescriptorHeaps( UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[] );

    void SetPredication(ID3D12Resource* Buffer, UINT64 BufferOffset, D3D12_PREDICATION_OP Op);

protected:

    void BindDescriptorHeaps( void );

    CommandListManager* m_OwningManager;
    ID3D12GraphicsCommandList* m_CommandList;
    ID3D12CommandAllocator* m_CurrentAllocator;

    ID3D12RootSignature* m_CurGraphicsRootSignature;
    ID3D12PipelineState* m_CurGraphicsPipelineState;
    ID3D12RootSignature* m_CurComputeRootSignature;
    ID3D12PipelineState* m_CurComputePipelineState;

    DynamicDescriptorHeap m_DynamicViewDescriptorHeap;        // HEAP_TYPE_CBV_SRV_UAV
    DynamicDescriptorHeap m_DynamicSamplerDescriptorHeap;    // HEAP_TYPE_SAMPLER

    D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
    UINT m_NumBarriersToFlush;

    ID3D12DescriptorHeap* m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

    LinearAllocator m_CpuLinearAllocator;
    LinearAllocator m_GpuLinearAllocator;

    std::wstring m_ID;
    void SetID(const std::wstring& ID) { m_ID = ID; }

    D3D12_COMMAND_LIST_TYPE m_Type;
};

class GraphicsContext : public CommandContext
{
public:

    static GraphicsContext& Begin(const std::wstring& ID = L"")
    {
        return CommandContext::Begin(ID).GetGraphicsContext();
    }

    // 清理视图
    void ClearUAV(GpuBuffer& Target);
    void ClearUAV(ColorBuffer& Target);
    void ClearColor( ColorBuffer& Target );
    void ClearDepth( DepthBuffer& Target );
    void ClearStencil( DepthBuffer& Target );
    void ClearDepthAndStencil( DepthBuffer& Target );

    void BeginQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex);
    void EndQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex);
    void ResolveQueryData(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource* DestinationBuffer, UINT64 DestinationBufferOffset);
    
    // 设置根签名
    void SetRootSignature(const RootSignature& RootSig);

    // 设置渲染目标视图
    void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[]);
    void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV);
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV ) { SetRenderTargets(1, &RTV); }
    void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV ) { SetRenderTargets(1, &RTV, DSV); }
    void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV ) { SetRenderTargets(0, nullptr, DSV); }

    // 设置视口、裁剪矩形
    void SetViewport( const D3D12_VIEWPORT& vp );
    void SetViewport( FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth = 0.0f, FLOAT maxDepth = 1.0f );
    void SetScissor( const D3D12_RECT& rect );
    void SetScissor( UINT left, UINT top, UINT right, UINT bottom );
    void SetViewportAndScissor( const D3D12_VIEWPORT& vp, const D3D12_RECT& rect );
    void SetViewportAndScissor( UINT x, UINT y, UINT w, UINT h );
    void SetStencilRef(UINT StencilRef);
    void SetBlendFactor(Color BlendFactor);
    void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology);

    // 设置流水线状态
    void SetPipelineState(const GraphicsPSO& PSO);
    void SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants);
    void SetConstant(UINT RootIndex, DWParam Val, UINT Offset = 0);
    void SetConstants(UINT RootIndex, DWParam X);
    void SetConstants(UINT RootIndex, DWParam X, DWParam Y);
    void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z);
    void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W);
    void SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV);
    void SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData);
    void SetBufferSRV(UINT RootIndex, const GpuBuffer& SRV, UINT64 Offset = 0);
    void SetBufferUAV(UINT RootIndex, const GpuBuffer& UAV, UINT64 Offset = 0);
    void SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle);

    // 设置描述符
    void SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
    void SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
    // 设置采样
    void SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
    void SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);

    // 设置索引视图、顶点视图
    void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView);
    void SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView);
    void SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[]);
    // 动态设置顶点视图
    void SetDynamicVB(UINT Slot, size_t NumVertices, size_t VertexStride, const void* VBData);
    // 动态设置索引视图
    void SetDynamicIB(size_t IndexCount, const uint16_t* IBData);
    void SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData);

    // 根据顶点绘制
    void Draw(UINT VertexCount, UINT VertexStartOffset = 0);
    // 根据索引绘制
    void DrawIndexed(UINT IndexCount, UINT StartIndexLocation = 0, INT BaseVertexLocation = 0);
    // 根据顶点绘制
    void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
        UINT StartVertexLocation = 0, UINT StartInstanceLocation = 0);
    // 根据索引绘制
    void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
        INT BaseVertexLocation, UINT StartInstanceLocation);
    void DrawIndirect(GpuBuffer& ArgumentBuffer, uint64_t ArgumentBufferOffset = 0);
    void ExecuteIndirect(CommandSignature& CommandSig, GpuBuffer& ArgumentBuffer, uint64_t ArgumentStartOffset = 0,
        uint32_t MaxCommands = 1, GpuBuffer * CommandCounterBuffer = nullptr, uint64_t CounterOffset = 0);

private:
};

class ComputeContext : public CommandContext
{
public:

    static ComputeContext& Begin(const std::wstring& ID = L"", bool Async = false);

    void ClearUAV(GpuBuffer& Target);
    void ClearUAV(ColorBuffer& Target);

    void SetRootSignature(const RootSignature& RootSig);

    void SetPipelineState(const ComputePSO& PSO);
    void SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants);
    void SetConstant(UINT RootIndex, DWParam Val, UINT Offset = 0);
    void SetConstants(UINT RootIndex, DWParam X);
    void SetConstants(UINT RootIndex, DWParam X, DWParam Y);
    void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z);
    void SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W);
    void SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV);
    void SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData);
    void SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData);
    void SetBufferSRV(UINT RootIndex, const GpuBuffer& SRV, UINT64 Offset = 0);
    void SetBufferUAV(UINT RootIndex, const GpuBuffer& UAV, UINT64 Offset = 0);
    void SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle);

    void SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
    void SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
    void SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle);
    void SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);

    void Dispatch(size_t GroupCountX = 1, size_t GroupCountY = 1, size_t GroupCountZ = 1);
    void Dispatch1D(size_t ThreadCountX, size_t GroupSizeX = 64);
    void Dispatch2D(size_t ThreadCountX, size_t ThreadCountY, size_t GroupSizeX = 8, size_t GroupSizeY = 8);
    void Dispatch3D(size_t ThreadCountX, size_t ThreadCountY, size_t ThreadCountZ, size_t GroupSizeX, size_t GroupSizeY, size_t GroupSizeZ);
    void DispatchIndirect(GpuBuffer& ArgumentBuffer, uint64_t ArgumentBufferOffset = 0);
    void ExecuteIndirect(CommandSignature& CommandSig, GpuBuffer& ArgumentBuffer, uint64_t ArgumentStartOffset = 0,
        uint32_t MaxCommands = 1, GpuBuffer * CommandCounterBuffer = nullptr, uint64_t CounterOffset = 0);

private:
};

// ====================== CommandContext ======================
inline void CommandContext::CopyBuffer(GpuResource& Dest, GpuResource& Src)
{
    TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    FlushResourceBarriers();
    m_CommandList->CopyResource(Dest.GetResource(), Src.GetResource());
}

inline void CommandContext::CopyBufferRegion(GpuResource& Dest, size_t DestOffset, GpuResource& Src, size_t SrcOffset, size_t NumBytes)
{
    TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
    //TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    FlushResourceBarriers();
    m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetResource(), SrcOffset, NumBytes);
}

inline void CommandContext::CopyCounter(GpuResource& Dest, size_t DestOffset, StructuredBuffer& Src)
{
    TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionResource(Src.GetCounterBuffer(), D3D12_RESOURCE_STATE_COPY_SOURCE);
    FlushResourceBarriers();
    m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetCounterBuffer().GetResource(), 0, 4);
}

inline void CommandContext::ResetCounter(StructuredBuffer& Buf, uint32_t Value)
{
    FillBuffer(Buf.GetCounterBuffer(), 0, Value, sizeof(uint32_t));
    TransitionResource(Buf.GetCounterBuffer(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
}

inline void CommandContext::FlushResourceBarriers( void )
{
    if (m_NumBarriersToFlush > 0)
    {
        m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
        m_NumBarriersToFlush = 0;
    }
}

inline void CommandContext::InsertTimeStamp(ID3D12QueryHeap* pQueryHeap, uint32_t QueryIdx)
{
    m_CommandList->EndQuery(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, QueryIdx);
}

inline void CommandContext::ResolveTimeStamps(ID3D12Resource* pReadbackHeap, ID3D12QueryHeap* pQueryHeap, uint32_t NumQueries)
{
    m_CommandList->ResolveQueryData(pQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, 0, NumQueries, pReadbackHeap, 0);
}

inline void CommandContext::SetDescriptorHeap( D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr )
{
    if (m_CurrentDescriptorHeaps[Type] != HeapPtr)
    {
        m_CurrentDescriptorHeaps[Type] = HeapPtr;
        BindDescriptorHeaps();
    }
}

inline void CommandContext::SetDescriptorHeaps( UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[] )
{
    bool AnyChanged = false;

    for (UINT i = 0; i < HeapCount; ++i)
    {
        if (m_CurrentDescriptorHeaps[Type[i]] != HeapPtrs[i])
        {
            m_CurrentDescriptorHeaps[Type[i]] = HeapPtrs[i];
            AnyChanged = true;
        }
    }

    if (AnyChanged)
        BindDescriptorHeaps();
}

inline void CommandContext::SetPredication(ID3D12Resource* Buffer, UINT64 BufferOffset, D3D12_PREDICATION_OP Op)
{
    m_CommandList->SetPredication(Buffer, BufferOffset, Op);
}

// ====================== GraphicsContext ======================
inline void GraphicsContext::SetRootSignature(const RootSignature& RootSig)
{
    if (RootSig.GetSignature() == m_CurGraphicsRootSignature)
        return;

    m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature = RootSig.GetSignature());

    m_DynamicViewDescriptorHeap.ParseGraphicsRootSignature(RootSig);
    m_DynamicSamplerDescriptorHeap.ParseGraphicsRootSignature(RootSig);
}

inline void GraphicsContext::SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h)
{
    SetViewport((float)x, (float)y, (float)w, (float)h);
    SetScissor(x, y, x + w, y + h);
}

inline void GraphicsContext::SetScissor(UINT left, UINT top, UINT right, UINT bottom)
{
    SetScissor(CD3DX12_RECT(left, top, right, bottom));
}

inline void GraphicsContext::SetStencilRef(UINT ref)
{
    m_CommandList->OMSetStencilRef(ref);
}

inline void GraphicsContext::SetBlendFactor(Color BlendFactor)
{
    m_CommandList->OMSetBlendFactor(BlendFactor.GetPtr());
}

inline void GraphicsContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
{
    m_CommandList->IASetPrimitiveTopology(Topology);
}

inline void GraphicsContext::SetPipelineState(const GraphicsPSO& PSO)
{
    ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
    if (PipelineState == m_CurGraphicsPipelineState)
        return;

    m_CommandList->SetPipelineState(PipelineState);
    m_CurGraphicsPipelineState = PipelineState;
}

inline void GraphicsContext::SetConstantArray(UINT RootIndex, UINT NumConstants, const void* pConstants)
{
    m_CommandList->SetGraphicsRoot32BitConstants(RootIndex, NumConstants, pConstants, 0);
}

inline void GraphicsContext::SetConstant(UINT RootEntry, DWParam Val, UINT Offset)
{
    m_CommandList->SetGraphicsRoot32BitConstant(RootEntry, Val.Uint, Offset);
}

inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X)
{
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
}

inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X, DWParam Y)
{
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Y.Uint, 1);
}

inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z)
{
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Y.Uint, 1);
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Z.Uint, 2);
}

inline void GraphicsContext::SetConstants(UINT RootIndex, DWParam X, DWParam Y, DWParam Z, DWParam W)
{
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, X.Uint, 0);
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Y.Uint, 1);
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, Z.Uint, 2);
    m_CommandList->SetGraphicsRoot32BitConstant(RootIndex, W.Uint, 3);
}

inline void GraphicsContext::SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV)
{
    m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, CBV);
}

inline void GraphicsContext::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
    ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
    DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
    //SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
    memcpy(cb.DataPtr, BufferData, BufferSize);
    m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, cb.GpuAddress);
}

inline void GraphicsContext::SetBufferSRV(UINT RootIndex, const GpuBuffer& SRV, UINT64 Offset)
{
    ASSERT((SRV.m_UsageState & (D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) != 0);
    m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, SRV.GetGpuVirtualAddress() + Offset);
}

inline void GraphicsContext::SetBufferUAV(UINT RootIndex, const GpuBuffer & UAV, UINT64 Offset)
{
    ASSERT((UAV.m_UsageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0);
    m_CommandList->SetGraphicsRootUnorderedAccessView(RootIndex, UAV.GetGpuVirtualAddress() + Offset);
}

inline void GraphicsContext::SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle)
{
    m_CommandList->SetGraphicsRootDescriptorTable(RootIndex, FirstHandle);
}

inline void GraphicsContext::SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
    SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
}

inline void GraphicsContext::SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
    m_DynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
}
inline void GraphicsContext::SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
    SetDynamicSamplers(RootIndex, Offset, 1, &Handle);
}

inline void GraphicsContext::SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
    m_DynamicSamplerDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
}

inline void GraphicsContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& IBView)
{
    m_CommandList->IASetIndexBuffer(&IBView);
}

inline void GraphicsContext::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& VBView)
{
    SetVertexBuffers(Slot, 1, &VBView);
}

inline void GraphicsContext::SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW VBViews[])
{
    m_CommandList->IASetVertexBuffers(StartSlot, Count, VBViews);
}

inline void GraphicsContext::SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
    ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
    DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
    SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
    m_CommandList->SetGraphicsRootShaderResourceView(RootIndex, cb.GpuAddress);
}

inline void GraphicsContext::Draw(UINT VertexCount, UINT VertexStartOffset)
{
    DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
}

inline void GraphicsContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
    DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}

inline void GraphicsContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
    UINT StartVertexLocation, UINT StartInstanceLocation)
{
    FlushResourceBarriers();
    m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
    m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
    m_CommandList->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

inline void GraphicsContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation,
    INT BaseVertexLocation, UINT StartInstanceLocation)
{
    FlushResourceBarriers();
    m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
    m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
    m_CommandList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

inline void GraphicsContext::DrawIndirect(GpuBuffer& ArgumentBuffer, uint64_t ArgumentBufferOffset)
{
    ExecuteIndirect(Graphics::DrawIndirectCommandSignature, ArgumentBuffer, ArgumentBufferOffset);
}

// ====================== ComputeContext ======================
inline void ComputeContext::SetRootSignature(const RootSignature& RootSig)
{
    if (RootSig.GetSignature() == m_CurComputeRootSignature)
        return;

    m_CommandList->SetComputeRootSignature(m_CurComputeRootSignature = RootSig.GetSignature());

    m_DynamicViewDescriptorHeap.ParseComputeRootSignature(RootSig);
    m_DynamicSamplerDescriptorHeap.ParseComputeRootSignature(RootSig);
}

inline void ComputeContext::SetPipelineState(const ComputePSO& PSO)
{
    ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
    if (PipelineState == m_CurComputePipelineState)
        return;

    m_CommandList->SetPipelineState(PipelineState);
    m_CurComputePipelineState = PipelineState;
}

inline void ComputeContext::SetConstantArray(UINT RootEntry, UINT NumConstants, const void* pConstants)
{
    m_CommandList->SetComputeRoot32BitConstants(RootEntry, NumConstants, pConstants, 0);
}

inline void ComputeContext::SetConstant(UINT RootEntry, DWParam Val, UINT Offset)
{
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, Val.Uint, Offset);
}

inline void ComputeContext::SetConstants(UINT RootEntry, DWParam X)
{
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, X.Uint, 0);
}

inline void ComputeContext::SetConstants(UINT RootEntry, DWParam X, DWParam Y)
{
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, X.Uint, 0);
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, Y.Uint, 1);
}

inline void ComputeContext::SetConstants(UINT RootEntry, DWParam X, DWParam Y, DWParam Z)
{
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, X.Uint, 0);
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, Y.Uint, 1);
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, Z.Uint, 2);
}

inline void ComputeContext::SetConstants(UINT RootEntry, DWParam X, DWParam Y, DWParam Z, DWParam W)
{
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, X.Uint, 0);
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, Y.Uint, 1);
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, Z.Uint, 2);
    m_CommandList->SetComputeRoot32BitConstant(RootEntry, W.Uint, 3);
}

inline void ComputeContext::SetConstantBuffer(UINT RootIndex, D3D12_GPU_VIRTUAL_ADDRESS CBV)
{
    m_CommandList->SetComputeRootConstantBufferView(RootIndex, CBV);
}

inline void ComputeContext::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
    ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
    DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
    //SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
    memcpy(cb.DataPtr, BufferData, BufferSize);
    m_CommandList->SetComputeRootConstantBufferView(RootIndex, cb.GpuAddress);
}

inline void ComputeContext::SetDynamicSRV(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
    ASSERT(BufferData != nullptr && Math::IsAligned(BufferData, 16));
    DynAlloc cb = m_CpuLinearAllocator.Allocate(BufferSize);
    SIMDMemCopy(cb.DataPtr, BufferData, Math::AlignUp(BufferSize, 16) >> 4);
    m_CommandList->SetComputeRootShaderResourceView(RootIndex, cb.GpuAddress);
}

inline void ComputeContext::SetBufferSRV(UINT RootIndex, const GpuBuffer& SRV, UINT64 Offset)
{
    ASSERT((SRV.m_UsageState & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0);
    m_CommandList->SetComputeRootShaderResourceView(RootIndex, SRV.GetGpuVirtualAddress() + Offset);
}

inline void ComputeContext::SetBufferUAV(UINT RootIndex, const GpuBuffer& UAV, UINT64 Offset)
{
    ASSERT((UAV.m_UsageState & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0);
    m_CommandList->SetComputeRootUnorderedAccessView(RootIndex, UAV.GetGpuVirtualAddress() + Offset);
}

inline void ComputeContext::Dispatch(size_t GroupCountX, size_t GroupCountY, size_t GroupCountZ)
{
    FlushResourceBarriers();
    m_DynamicViewDescriptorHeap.CommitComputeRootDescriptorTables(m_CommandList);
    m_DynamicSamplerDescriptorHeap.CommitComputeRootDescriptorTables(m_CommandList);
    m_CommandList->Dispatch((UINT)GroupCountX, (UINT)GroupCountY, (UINT)GroupCountZ);
}

inline void ComputeContext::Dispatch1D(size_t ThreadCountX, size_t GroupSizeX)
{
    Dispatch(Math::DivideByMultiple(ThreadCountX, GroupSizeX), 1, 1);
}

inline void ComputeContext::Dispatch2D(size_t ThreadCountX, size_t ThreadCountY, size_t GroupSizeX, size_t GroupSizeY)
{
    Dispatch(
        Math::DivideByMultiple(ThreadCountX, GroupSizeX),
        Math::DivideByMultiple(ThreadCountY, GroupSizeY), 1);
}

inline void ComputeContext::Dispatch3D(size_t ThreadCountX, size_t ThreadCountY, size_t ThreadCountZ, size_t GroupSizeX, size_t GroupSizeY, size_t GroupSizeZ)
{
    Dispatch(
        Math::DivideByMultiple(ThreadCountX, GroupSizeX),
        Math::DivideByMultiple(ThreadCountY, GroupSizeY),
        Math::DivideByMultiple(ThreadCountZ, GroupSizeZ));
}

inline void ComputeContext::SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
    SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
}

inline void ComputeContext::SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
    m_DynamicViewDescriptorHeap.SetComputeDescriptorHandles(RootIndex, Offset, Count, Handles);
}

inline void ComputeContext::SetDynamicSampler(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
    SetDynamicSamplers(RootIndex, Offset, 1, &Handle);
}

inline void ComputeContext::SetDynamicSamplers(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
    m_DynamicSamplerDescriptorHeap.SetComputeDescriptorHandles(RootIndex, Offset, Count, Handles);
}

inline void ComputeContext::SetDescriptorTable(UINT RootIndex, D3D12_GPU_DESCRIPTOR_HANDLE FirstHandle)
{
    m_CommandList->SetComputeRootDescriptorTable(RootIndex, FirstHandle);
}

inline void ComputeContext::ExecuteIndirect(CommandSignature& CommandSig,
    GpuBuffer& ArgumentBuffer, uint64_t ArgumentStartOffset,
    uint32_t MaxCommands, GpuBuffer* CommandCounterBuffer, uint64_t CounterOffset)
{
    FlushResourceBarriers();
    m_DynamicViewDescriptorHeap.CommitComputeRootDescriptorTables(m_CommandList);
    m_DynamicSamplerDescriptorHeap.CommitComputeRootDescriptorTables(m_CommandList);
    m_CommandList->ExecuteIndirect(CommandSig.GetSignature(), MaxCommands,
        ArgumentBuffer.GetResource(), ArgumentStartOffset,
        CommandCounterBuffer == nullptr ? nullptr : CommandCounterBuffer->GetResource(), CounterOffset);
}

inline void ComputeContext::DispatchIndirect(GpuBuffer& ArgumentBuffer, uint64_t ArgumentBufferOffset)
{
    ExecuteIndirect(Graphics::DispatchIndirectCommandSignature, ArgumentBuffer, ArgumentBufferOffset);
}