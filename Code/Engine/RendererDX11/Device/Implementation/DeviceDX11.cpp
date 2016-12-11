
#include <RendererDX11/PCH.h>
#include <RendererDX11/Device/DeviceDX11.h>
#include <RendererDX11/Device/SwapChainDX11.h>
#include <RendererDX11/Shader/ShaderDX11.h>
#include <RendererDX11/Resources/BufferDX11.h>
#include <RendererDX11/Resources/FenceDX11.h>
#include <RendererDX11/Resources/TextureDX11.h>
#include <RendererDX11/Resources/RenderTargetViewDX11.h>
#include <RendererDX11/Resources/UnorderedAccessViewDX11.h>
#include <RendererDX11/Shader/VertexDeclarationDX11.h>
#include <RendererDX11/State/StateDX11.h>
#include <RendererDX11/Resources/ResourceViewDX11.h>
#include <RendererDX11/Context/ContextDX11.h>


#include <d3d11.h>

ezGALDeviceDX11::ezGALDeviceDX11(const ezGALDeviceCreationDescription& Description)
  : ezGALDevice(Description)
  , m_pDevice(nullptr)
  , m_pDebug(nullptr)
  , m_pDXGIFactory(nullptr)
  , m_pDXGIAdapter(nullptr)
  , m_pDXGIDevice(nullptr)
  , m_FeatureLevel(D3D_FEATURE_LEVEL_9_1)
  , m_uiCurrentEndFrameFence(0)
  , m_uiNextEndFrameFence(0)
  , m_uiFrameCounter(0)
{
}

ezGALDeviceDX11::~ezGALDeviceDX11()
{
}


// Init & shutdown functions

ezResult ezGALDeviceDX11::InitPlatform()
{
  EZ_LOG_BLOCK("ezGALDeviceDX11::InitPlatform");

  DWORD dwFlags = 0;

retry:

  if (m_Description.m_bDebugDevice)
    dwFlags |= D3D11_CREATE_DEVICE_DEBUG;
  else
    dwFlags &= ~D3D11_CREATE_DEVICE_DEBUG;

  D3D_FEATURE_LEVEL FeatureLevels[] =
  {
    D3D_FEATURE_LEVEL_11_1,
    D3D_FEATURE_LEVEL_11_0,
    D3D_FEATURE_LEVEL_10_1,
    D3D_FEATURE_LEVEL_10_0,
    D3D_FEATURE_LEVEL_9_3
  };
  ID3D11DeviceContext* pImmediateContext = nullptr;

  // Manually step through feature levels - if a Win 7 system doesn't have the 11.1 runtime installed
  // The create device call will fail even though the 11.0 (or lower) level could've been
  // initialized successfully
  int FeatureLevelIdx = 0;
  for (FeatureLevelIdx = 0; FeatureLevelIdx < EZ_ARRAY_SIZE(FeatureLevels); FeatureLevelIdx++)
  {
    if (SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, dwFlags, &FeatureLevels[FeatureLevelIdx], 1, D3D11_SDK_VERSION, &m_pDevice, &m_FeatureLevel, &pImmediateContext)))
    {
      break;
    }
  }

  // Nothing could be initialized:
  if (pImmediateContext == nullptr)
  {
    if (m_Description.m_bDebugDevice)
    {
      ezLog::ErrorPrintf("Couldn't initialize D3D11 debug device!");

      m_Description.m_bDebugDevice = false;
      goto retry;
    }

    ezLog::ErrorPrintf("Couldn't initialize D3D11 device!");
    return EZ_FAILURE;
  }
  else
  {
    const char* FeatureLevelNames[] =
    {
      "11.1",
      "11.0",
      "10.1",
      "10",
      "9.3"
    };

    EZ_CHECK_AT_COMPILETIME(EZ_ARRAY_SIZE(FeatureLevels) == EZ_ARRAY_SIZE(FeatureLevelNames));

    ezLog::SuccessPrintf("Initialized D3D11 device with feature level %s.", FeatureLevelNames[FeatureLevelIdx]);
  }

  if (m_Description.m_bDebugDevice)
  {
    if (SUCCEEDED(m_pDevice->QueryInterface(__uuidof(ID3D11Debug), (void **)&m_pDebug)))
    {
      ID3D11InfoQueue* pInfoQueue = nullptr;
      if (SUCCEEDED(m_pDebug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&pInfoQueue)))
      {
        pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, true);
        pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, true);
        pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING, true);

        pInfoQueue->Release();
      }
    }
  }


  // Create primary context object
  m_pPrimaryContext = EZ_NEW(&m_Allocator, ezGALContextDX11, this, pImmediateContext);
  EZ_ASSERT_RELEASE(m_pPrimaryContext != nullptr, "Couldn't create primary context!");

  if (FAILED(m_pDevice->QueryInterface(__uuidof(IDXGIDevice1), (void **)&m_pDXGIDevice)))
  {
    ezLog::ErrorPrintf("Couldn't get the DXGIDevice1 interface of the D3D11 device - this may happen when running on Windows Vista without SP2 installed!");
    return EZ_FAILURE;
  }

  if (FAILED(m_pDXGIDevice->SetMaximumFrameLatency(1)))
  {
    ezLog::WarningPrintf("Failed to set max frames latency");
  }

  if (FAILED(m_pDXGIDevice->GetParent(__uuidof(IDXGIAdapter), (void **)&m_pDXGIAdapter)))
  {
    return EZ_FAILURE;
  }

  if (FAILED(m_pDXGIAdapter->GetParent(__uuidof(IDXGIFactory1), (void **)&m_pDXGIFactory)))
  {
    return EZ_FAILURE;
  }

  // Fill lookup table
  FillFormatLookupTable();

  /// \todo Get features of the device (depending on feature level, CheckFormat* functions etc.)

  ezProjectionDepthRange::Default = ezProjectionDepthRange::ZeroToOne;


  //End frame fences
  for (ezUInt32 i = 0; i < EZ_ARRAY_SIZE(m_EndFrameFences); ++i)
  {
    m_EndFrameFences[i].m_pFence = CreateFencePlatform();
    m_EndFrameFences[i].m_uiFrame = -1;
  }

  return EZ_SUCCESS;
}

ezResult ezGALDeviceDX11::ShutdownPlatform()
{
  for (ezUInt32 type = 0; type < TempResourceType::ENUM_COUNT; ++type)
  {
    for (auto it = m_FreeTempResources[type].GetIterator(); it.IsValid(); ++it)
    {
      ezDynamicArray<ID3D11Resource*>& resources = it.Value();
      for (auto pResource : resources)
      {
        EZ_GAL_DX11_RELEASE(pResource);
      }
    }
    m_FreeTempResources[type].Clear();

    for (auto& tempResource : m_UsedTempResources[type])
    {
      EZ_GAL_DX11_RELEASE(tempResource.m_pResource);
    }
    m_UsedTempResources[type].Clear();
  }

  for (ezUInt32 i = 0; i < EZ_ARRAY_SIZE(m_EndFrameFences); ++i)
  {
    DestroyFencePlatform(m_EndFrameFences[i].m_pFence);
    m_EndFrameFences[i].m_pFence = nullptr;
  }

  EZ_DELETE(&m_Allocator, m_pPrimaryContext);

  EZ_GAL_DX11_RELEASE(m_pDevice);
  EZ_GAL_DX11_RELEASE(m_pDebug);
  EZ_GAL_DX11_RELEASE(m_pDXGIFactory);
  EZ_GAL_DX11_RELEASE(m_pDXGIAdapter);
  EZ_GAL_DX11_RELEASE(m_pDXGIDevice);

  return EZ_SUCCESS;
}


// State creation functions

ezGALBlendState* ezGALDeviceDX11::CreateBlendStatePlatform(const ezGALBlendStateCreationDescription& Description)
{
  ezGALBlendStateDX11* pState = EZ_NEW(&m_Allocator, ezGALBlendStateDX11, Description);

  if (pState->InitPlatform(this).Succeeded())
  {
    return pState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pState);
    return nullptr;
  }
  return nullptr;
}

void ezGALDeviceDX11::DestroyBlendStatePlatform(ezGALBlendState* pBlendState)
{
  ezGALBlendStateDX11* pState = static_cast<ezGALBlendStateDX11*>(pBlendState);
  pState->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pState);
}

ezGALDepthStencilState* ezGALDeviceDX11::CreateDepthStencilStatePlatform(const ezGALDepthStencilStateCreationDescription& Description)
{
  ezGALDepthStencilStateDX11* pDX11DepthStencilState = EZ_NEW(&m_Allocator, ezGALDepthStencilStateDX11, Description);

  if (pDX11DepthStencilState->InitPlatform(this).Succeeded())
  {
    return pDX11DepthStencilState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pDX11DepthStencilState);
    return nullptr;
  }
}

void ezGALDeviceDX11::DestroyDepthStencilStatePlatform(ezGALDepthStencilState* pDepthStencilState)
{
  ezGALDepthStencilStateDX11* pDX11DepthStencilState = static_cast<ezGALDepthStencilStateDX11*>(pDepthStencilState);
  pDX11DepthStencilState->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11DepthStencilState);
}

ezGALRasterizerState* ezGALDeviceDX11::CreateRasterizerStatePlatform(const ezGALRasterizerStateCreationDescription& Description)
{
  ezGALRasterizerStateDX11* pDX11RasterizerState = EZ_NEW(&m_Allocator, ezGALRasterizerStateDX11, Description);

  if(pDX11RasterizerState->InitPlatform(this).Succeeded())
  {
    return pDX11RasterizerState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pDX11RasterizerState);
    return nullptr;
  }
}

void ezGALDeviceDX11::DestroyRasterizerStatePlatform(ezGALRasterizerState* pRasterizerState)
{
  ezGALRasterizerStateDX11* pDX11RasterizerState = static_cast<ezGALRasterizerStateDX11*>(pRasterizerState);
  pDX11RasterizerState->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11RasterizerState);
}

ezGALSamplerState* ezGALDeviceDX11::CreateSamplerStatePlatform(const ezGALSamplerStateCreationDescription& Description)
{
  ezGALSamplerStateDX11* pDX11SamplerState = EZ_NEW(&m_Allocator, ezGALSamplerStateDX11, Description);

  if (pDX11SamplerState->InitPlatform(this).Succeeded())
  {
    return pDX11SamplerState;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pDX11SamplerState);
    return nullptr;
  }
}

void ezGALDeviceDX11::DestroySamplerStatePlatform(ezGALSamplerState* pSamplerState)
{
  ezGALSamplerStateDX11* pDX11SamplerState = static_cast<ezGALSamplerStateDX11*>(pSamplerState);
  pDX11SamplerState->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11SamplerState);
}


// Resource creation functions

ezGALShader* ezGALDeviceDX11::CreateShaderPlatform(const ezGALShaderCreationDescription& Description)
{
  ezGALShaderDX11* pShader = EZ_NEW(&m_Allocator, ezGALShaderDX11, Description);

  if(!pShader->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pShader);
    return nullptr;
  }

  return pShader;
}

void ezGALDeviceDX11::DestroyShaderPlatform(ezGALShader* pShader)
{
  ezGALShaderDX11* pDX11Shader = static_cast<ezGALShaderDX11*>(pShader);
  pDX11Shader->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11Shader);
}

ezGALBuffer* ezGALDeviceDX11::CreateBufferPlatform(const ezGALBufferCreationDescription& Description, ezArrayPtr<const ezUInt8> pInitialData)
{
  ezGALBufferDX11* pBuffer = EZ_NEW(&m_Allocator, ezGALBufferDX11, Description);

  if(!pBuffer->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pBuffer);
    return nullptr;
  }

  return pBuffer;
}

void ezGALDeviceDX11::DestroyBufferPlatform(ezGALBuffer* pBuffer)
{
  ezGALBufferDX11* pDX11Buffer = static_cast<ezGALBufferDX11*>(pBuffer);
  pDX11Buffer->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11Buffer);
}

ezGALTexture* ezGALDeviceDX11::CreateTexturePlatform(const ezGALTextureCreationDescription& Description, ezArrayPtr<ezGALSystemMemoryDescription> pInitialData)
{
  ezGALTextureDX11* pTexture = EZ_NEW(&m_Allocator, ezGALTextureDX11, Description);

  if(!pTexture->InitPlatform(this, pInitialData).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pTexture);
    return nullptr;
  }

  return pTexture;
}

void ezGALDeviceDX11::DestroyTexturePlatform(ezGALTexture* pTexture)
{
  ezGALTextureDX11* pDX11Texture = static_cast<ezGALTextureDX11*>(pTexture);
  pDX11Texture->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11Texture);
}

ezGALResourceView* ezGALDeviceDX11::CreateResourceViewPlatform(ezGALResourceBase* pResource, const ezGALResourceViewCreationDescription& Description)
{
  ezGALResourceViewDX11* pResourceView = EZ_NEW(&m_Allocator, ezGALResourceViewDX11, pResource, Description);

  if(!pResourceView->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pResourceView);
    return nullptr;
  }

  return pResourceView;
}

void ezGALDeviceDX11::DestroyResourceViewPlatform(ezGALResourceView* pResourceView)
{
  ezGALResourceViewDX11* pDX11ResourceView = static_cast<ezGALResourceViewDX11*>(pResourceView);
  pDX11ResourceView->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11ResourceView);
}

ezGALRenderTargetView* ezGALDeviceDX11::CreateRenderTargetViewPlatform(ezGALTexture* pTexture, const ezGALRenderTargetViewCreationDescription& Description)
{
  ezGALRenderTargetViewDX11* pRTView = EZ_NEW(&m_Allocator, ezGALRenderTargetViewDX11, pTexture, Description);

  if(!pRTView->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pRTView);
    return nullptr;
  }

  return pRTView;
}

void ezGALDeviceDX11::DestroyRenderTargetViewPlatform(ezGALRenderTargetView* pRenderTargetView)
{
  ezGALRenderTargetViewDX11* pDX11RenderTargetView = static_cast<ezGALRenderTargetViewDX11*>(pRenderTargetView);
  pDX11RenderTargetView->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pDX11RenderTargetView);
}

ezGALUnorderedAccessView* ezGALDeviceDX11::CreateUnorderedAccessViewPlatform(ezGALResourceBase* pTextureOfBuffer, const ezGALUnorderedAccessViewCreationDescription& Description)
{
  ezGALUnorderedAccessViewDX11* pUnorderedAccessView = EZ_NEW(&m_Allocator, ezGALUnorderedAccessViewDX11, pTextureOfBuffer, Description);

  if (!pUnorderedAccessView->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pUnorderedAccessView);
    return nullptr;
  }

  return pUnorderedAccessView;
}

void ezGALDeviceDX11::DestroyUnorderedAccessViewPlatform(ezGALUnorderedAccessView* pUnorderedAccessView)
{
  ezGALUnorderedAccessViewDX11* pUnorderedAccessViewDX11 = static_cast<ezGALUnorderedAccessViewDX11*>(pUnorderedAccessView);
  pUnorderedAccessViewDX11->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pUnorderedAccessViewDX11);
}



// Other rendering creation functions
ezGALSwapChain* ezGALDeviceDX11::CreateSwapChainPlatform(const ezGALSwapChainCreationDescription& Description)
{
  ezGALSwapChainDX11* pSwapChain = EZ_NEW(&m_Allocator, ezGALSwapChainDX11, Description);

  if (!pSwapChain->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pSwapChain);
    return nullptr;
  }

  return pSwapChain;
}

void ezGALDeviceDX11::DestroySwapChainPlatform(ezGALSwapChain* pSwapChain)
{
  ezGALSwapChainDX11* pSwapChainDX11 = static_cast<ezGALSwapChainDX11*>(pSwapChain);
  pSwapChainDX11->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pSwapChainDX11);
}

ezGALFence* ezGALDeviceDX11::CreateFencePlatform()
{
  ezGALFenceDX11* pFence = EZ_NEW(&m_Allocator, ezGALFenceDX11);

  if (!pFence->InitPlatform(this).Succeeded())
  {
    EZ_DELETE(&m_Allocator, pFence);
    return nullptr;
  }

  return pFence;
}

void ezGALDeviceDX11::DestroyFencePlatform(ezGALFence* pFence)
{
  ezGALFenceDX11* pFenceDX11 = static_cast<ezGALFenceDX11*>(pFence);
  pFenceDX11->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pFenceDX11);
}

ezGALQuery* ezGALDeviceDX11::CreateQueryPlatform(const ezGALQueryCreationDescription& Description)
{
  EZ_ASSERT_NOT_IMPLEMENTED;
  return nullptr;
}

void ezGALDeviceDX11::DestroyQueryPlatform(ezGALQuery* pQuery)
{
  EZ_ASSERT_NOT_IMPLEMENTED;
}

ezGALVertexDeclaration* ezGALDeviceDX11::CreateVertexDeclarationPlatform(const ezGALVertexDeclarationCreationDescription& Description)
{
  ezGALVertexDeclarationDX11* pVertexDeclaration = EZ_NEW(&m_Allocator, ezGALVertexDeclarationDX11, Description);

  if(pVertexDeclaration->InitPlatform(this).Succeeded())
  {
    return pVertexDeclaration;
  }
  else
  {
    EZ_DELETE(&m_Allocator, pVertexDeclaration);
    return nullptr;
  }
}

void ezGALDeviceDX11::DestroyVertexDeclarationPlatform(ezGALVertexDeclaration* pVertexDeclaration)
{
  ezGALVertexDeclarationDX11* pVertexDeclarationDX11 = static_cast<ezGALVertexDeclarationDX11*>(pVertexDeclaration);
  pVertexDeclarationDX11->DeInitPlatform(this);
  EZ_DELETE(&m_Allocator, pVertexDeclarationDX11);
}


void ezGALDeviceDX11::GetQueryDataPlatform(ezGALQuery* pQuery, ezUInt64* puiRendererdPixels)
{
  EZ_ASSERT_NOT_IMPLEMENTED;
  *puiRendererdPixels = 42;
}





// Swap chain functions

void ezGALDeviceDX11::PresentPlatform(ezGALSwapChain* pSwapChain)
{
  IDXGISwapChain* pDXGISwapChain = static_cast<ezGALSwapChainDX11*>(pSwapChain)->GetDXSwapChain();

  pDXGISwapChain->Present(pSwapChain->GetDescription().m_bVerticalSynchronization ? 1 : 0, 0);
}

// Misc functions

void ezGALDeviceDX11::BeginFramePlatform()
{
}

void ezGALDeviceDX11::EndFramePlatform()
{
  ezGALContextDX11* pContext = GetPrimaryContext<ezGALContextDX11>();

  // check if fence is reached
  {
    auto& endFrameFence = m_EndFrameFences[m_uiCurrentEndFrameFence];
    if (endFrameFence.m_uiFrame != ((ezUInt64)-1))
    {
      bool bFenceReached = pContext->IsFenceReachedPlatform(endFrameFence.m_pFence);
      if (!bFenceReached && m_uiNextEndFrameFence == m_uiCurrentEndFrameFence)
      {
        pContext->WaitForFencePlatform(endFrameFence.m_pFence);
        bFenceReached = true;
      }

      if (bFenceReached)
      {
        FreeTempResources(endFrameFence.m_uiFrame);

        m_uiCurrentEndFrameFence = (m_uiCurrentEndFrameFence + 1) % EZ_ARRAY_SIZE(m_EndFrameFences);
      }
    }
  }

  // insert fence
  {
    auto& endFrameFence = m_EndFrameFences[m_uiNextEndFrameFence];

    pContext->InsertFencePlatform(endFrameFence.m_pFence);
    endFrameFence.m_uiFrame = m_uiFrameCounter;

    m_uiNextEndFrameFence = (m_uiNextEndFrameFence + 1) % EZ_ARRAY_SIZE(m_EndFrameFences);
  }


  ++m_uiFrameCounter;
}

void ezGALDeviceDX11::SetPrimarySwapChainPlatform(ezGALSwapChain* pSwapChain)
{
  // Make window association
  m_pDXGIFactory->MakeWindowAssociation(pSwapChain->GetDescription().m_pWindow->GetNativeWindowHandle(), 0);
}


void ezGALDeviceDX11::FillCapabilitiesPlatform()
{
  m_Capabilities.m_bMultithreadedResourceCreation = true;

  switch (m_FeatureLevel)
  {
  case D3D_FEATURE_LEVEL_11_1:
    m_Capabilities.m_bB5G6R5Textures = true;
  case D3D_FEATURE_LEVEL_11_0:
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::VertexShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::HullShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::DomainShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::GeometryShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::PixelShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::ComputeShader] = true;
    m_Capabilities.m_bInstancing = true;
    m_Capabilities.m_b32BitIndices = true;
    m_Capabilities.m_bIndirectDraw = true;
    m_Capabilities.m_bStreamOut = true;
    m_Capabilities.m_uiMaxConstantBuffers = D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT;
    m_Capabilities.m_bTextureArrays = true;
    m_Capabilities.m_bCubemapArrays = true;
    m_Capabilities.m_uiMaxTextureDimension = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    m_Capabilities.m_uiMaxCubemapDimension = D3D11_REQ_TEXTURECUBE_DIMENSION;
    m_Capabilities.m_uiMax3DTextureDimension = D3D11_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    m_Capabilities.m_uiMaxAnisotropy = D3D11_REQ_MAXANISOTROPY;
    m_Capabilities.m_uiMaxRendertargets = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
    m_Capabilities.m_uiUAVCount = (m_FeatureLevel == D3D_FEATURE_LEVEL_11_1 ? 64 : 8);
    m_Capabilities.m_bAlphaToCoverage = true;
    break;

  case D3D_FEATURE_LEVEL_10_1:
  case D3D_FEATURE_LEVEL_10_0:
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::VertexShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::HullShader] = false;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::DomainShader] = false;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::GeometryShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::PixelShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::ComputeShader] = false;
    m_Capabilities.m_bInstancing = true;
    m_Capabilities.m_b32BitIndices = true;
    m_Capabilities.m_bIndirectDraw = false;
    m_Capabilities.m_bStreamOut = true;
    m_Capabilities.m_uiMaxConstantBuffers = D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT;
    m_Capabilities.m_bTextureArrays = true;
    m_Capabilities.m_bCubemapArrays = (m_FeatureLevel == D3D_FEATURE_LEVEL_10_1 ? true : false);
    m_Capabilities.m_uiMaxTextureDimension = D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
    m_Capabilities.m_uiMaxCubemapDimension = D3D10_REQ_TEXTURECUBE_DIMENSION;
    m_Capabilities.m_uiMax3DTextureDimension = D3D10_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;
    m_Capabilities.m_uiMaxAnisotropy = D3D10_REQ_MAXANISOTROPY;
    m_Capabilities.m_uiMaxRendertargets = D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT;
    m_Capabilities.m_uiUAVCount = 0;
    m_Capabilities.m_bAlphaToCoverage = true;
    break;

  case D3D_FEATURE_LEVEL_9_3:
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::VertexShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::HullShader] = false;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::DomainShader] = false;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::GeometryShader] = false;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::PixelShader] = true;
    m_Capabilities.m_bShaderStageSupported[ezGALShaderStage::ComputeShader] = false;
    m_Capabilities.m_bInstancing = true;
    m_Capabilities.m_b32BitIndices = true;
    m_Capabilities.m_bIndirectDraw = false;
    m_Capabilities.m_bStreamOut = false;
    m_Capabilities.m_uiMaxConstantBuffers = D3D11_COMMONSHADER_CONSTANT_BUFFER_HW_SLOT_COUNT;
    m_Capabilities.m_bTextureArrays = false;
    m_Capabilities.m_bCubemapArrays = false;
    m_Capabilities.m_uiMaxTextureDimension = D3D_FL9_3_REQ_TEXTURE1D_U_DIMENSION;
    m_Capabilities.m_uiMaxCubemapDimension = D3D_FL9_3_REQ_TEXTURECUBE_DIMENSION;
    m_Capabilities.m_uiMax3DTextureDimension = 0;
    m_Capabilities.m_uiMaxAnisotropy = 16;
    m_Capabilities.m_uiMaxRendertargets = D3D_FL9_3_SIMULTANEOUS_RENDER_TARGET_COUNT;
    m_Capabilities.m_uiUAVCount = 0;
    m_Capabilities.m_bAlphaToCoverage = false;
    break;

  }

}


ID3D11Resource* ezGALDeviceDX11::FindTempBuffer(ezUInt32 uiSize)
{
  const ezUInt32 uiExpGrowthLimit = 16 * 1024 * 1024;

  uiSize = ezMath::Max(uiSize, 256U);
  if (uiSize < uiExpGrowthLimit)
  {
    uiSize = ezMath::PowerOfTwo_Ceil(uiSize);
  }
  else
  {
    uiSize = ezMemoryUtils::AlignSize(uiSize, uiExpGrowthLimit);
  }

  ID3D11Resource* pResource = nullptr;
  auto it = m_FreeTempResources[TempResourceType::Buffer].Find(uiSize);
  if (it.IsValid())
  {
    ezDynamicArray<ID3D11Resource*>& resources = it.Value();
    if (!resources.IsEmpty())
    {
      pResource = resources[0];
      resources.RemoveAtSwap(0);
    }
  }

  if (pResource == nullptr)
  {
    D3D11_BUFFER_DESC desc;
    desc.ByteWidth = uiSize;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;
    desc.StructureByteStride = 0;

    ID3D11Buffer* pBuffer = nullptr;
    if (!SUCCEEDED(m_pDevice->CreateBuffer(&desc, nullptr, &pBuffer)))
    {
      return nullptr;
    }

    pResource = pBuffer;
  }

  auto& tempResource = m_UsedTempResources[TempResourceType::Buffer].ExpandAndGetRef();
  tempResource.m_pResource = pResource;
  tempResource.m_uiFrame = m_uiFrameCounter;
  tempResource.m_uiHash = uiSize;

  return pResource;
}


ID3D11Resource* ezGALDeviceDX11::FindTempTexture(ezUInt32 uiWidth, ezUInt32 uiHeight, ezUInt32 uiDepth, ezGALResourceFormat::Enum format)
{
  ezUInt32 data[] = { uiWidth, uiHeight, uiDepth, (ezUInt32)format };
  ezUInt32 uiHash = ezHashing::MurmurHash(data, sizeof(data));

  ID3D11Resource* pResource = nullptr;
  auto it = m_FreeTempResources[TempResourceType::Texture].Find(uiHash);
  if (it.IsValid())
  {
    ezDynamicArray<ID3D11Resource*>& resources = it.Value();
    if (!resources.IsEmpty())
    {
      pResource = resources[0];
      resources.RemoveAtSwap(0);
    }
  }

  if (pResource == nullptr)
  {
    if (uiDepth == 1)
    {
      D3D11_TEXTURE2D_DESC desc;
      desc.Width = uiWidth;
      desc.Height = uiHeight;
      desc.MipLevels = 1;
      desc.ArraySize = 1;
      desc.Format = GetFormatLookupTable().GetFormatInfo(format).m_eStorage;
      desc.SampleDesc.Count = 1;
      desc.SampleDesc.Quality = 0;
      desc.Usage = D3D11_USAGE_STAGING;
      desc.BindFlags = 0;
      desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
      desc.MiscFlags = 0;

      ID3D11Texture2D* pTexture = nullptr;
      if (!SUCCEEDED(m_pDevice->CreateTexture2D(&desc, nullptr, &pTexture)))
      {
        return nullptr;
      }

      pResource = pTexture;
    }
    else
    {
      EZ_ASSERT_NOT_IMPLEMENTED;
      return nullptr;
    }
  }

  auto& tempResource = m_UsedTempResources[TempResourceType::Texture].ExpandAndGetRef();
  tempResource.m_pResource = pResource;
  tempResource.m_uiFrame = m_uiFrameCounter;
  tempResource.m_uiHash = uiHash;

  return pResource;
}

void ezGALDeviceDX11::FreeTempResources(ezUInt64 uiFrame)
{
  for (ezUInt32 type = 0; type < TempResourceType::ENUM_COUNT; ++type)
  {
    while (!m_UsedTempResources[type].IsEmpty())
    {
      auto& usedTempResource = m_UsedTempResources[type].PeekFront();
      if (usedTempResource.m_uiFrame == uiFrame)
      {
        auto it = m_FreeTempResources[type].Find(usedTempResource.m_uiHash);
        if (!it.IsValid())
        {
          it = m_FreeTempResources[type].Insert(usedTempResource.m_uiHash, ezDynamicArray<ID3D11Resource*>(&m_Allocator));
        }

        it.Value().PushBack(usedTempResource.m_pResource);
        m_UsedTempResources[type].PopFront();
      }
      else
      {
        break;
      }
    }
  }
}

void ezGALDeviceDX11::FillFormatLookupTable()
{
  ///       The list below is in the same order as the ezGALResourceFormat enum. No format should be missing except the ones that are just different names for the same enum value.

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAFloat,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32B32A32_TYPELESS).
      RT(DXGI_FORMAT_R32G32B32A32_FLOAT).
      VA(DXGI_FORMAT_R32G32B32A32_FLOAT).
      RV(DXGI_FORMAT_R32G32B32A32_FLOAT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAUInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32B32A32_TYPELESS).
      RT(DXGI_FORMAT_R32G32B32A32_UINT).
      VA(DXGI_FORMAT_R32G32B32A32_UINT).
      RV(DXGI_FORMAT_R32G32B32A32_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32B32A32_TYPELESS).
      RT(DXGI_FORMAT_R32G32B32A32_SINT).
      VA(DXGI_FORMAT_R32G32B32A32_SINT).
      RV(DXGI_FORMAT_R32G32B32A32_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBFloat,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32B32_TYPELESS).
      RT(DXGI_FORMAT_R32G32B32_FLOAT).
      VA(DXGI_FORMAT_R32G32B32_FLOAT).
      RV(DXGI_FORMAT_R32G32B32_FLOAT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBUInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32B32_TYPELESS).
      RT(DXGI_FORMAT_R32G32B32_UINT).
      VA(DXGI_FORMAT_R32G32B32_UINT).
      RV(DXGI_FORMAT_R32G32B32_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32B32_TYPELESS).
      RT(DXGI_FORMAT_R32G32B32_SINT).
      VA(DXGI_FORMAT_R32G32B32_SINT).
      RV(DXGI_FORMAT_R32G32B32_SINT)
      );

  // Supported with DX 11.1
  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::B5G6R5UNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_B5G6R5_UNORM).
      RT(DXGI_FORMAT_B5G6R5_UNORM).
      VA(DXGI_FORMAT_B5G6R5_UNORM).
      RV(DXGI_FORMAT_B5G6R5_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BGRAUByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_B8G8R8A8_TYPELESS).
      RT(DXGI_FORMAT_B8G8R8A8_UNORM).
      VA(DXGI_FORMAT_B8G8R8A8_UNORM).
      RV(DXGI_FORMAT_B8G8R8A8_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BGRAUByteNormalizedsRGB,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_B8G8R8A8_TYPELESS).
      RT(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB).
      RV(DXGI_FORMAT_B8G8R8A8_UNORM_SRGB)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAHalf,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16B16A16_TYPELESS).
      RT(DXGI_FORMAT_R16G16B16A16_FLOAT).
      VA(DXGI_FORMAT_R16G16B16A16_FLOAT).
      RV(DXGI_FORMAT_R16G16B16A16_FLOAT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAUShort,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16B16A16_TYPELESS).
      RT(DXGI_FORMAT_R16G16B16A16_UINT).
      VA(DXGI_FORMAT_R16G16B16A16_UINT).
      RV(DXGI_FORMAT_R16G16B16A16_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAUShortNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16B16A16_TYPELESS).
      RT(DXGI_FORMAT_R16G16B16A16_UNORM).
      VA(DXGI_FORMAT_R16G16B16A16_UNORM).
      RV(DXGI_FORMAT_R16G16B16A16_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAShort,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16B16A16_TYPELESS).
      RT(DXGI_FORMAT_R16G16B16A16_SINT).
      VA(DXGI_FORMAT_R16G16B16A16_SINT).
      RV(DXGI_FORMAT_R16G16B16A16_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAShortNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16B16A16_TYPELESS).
      RT(DXGI_FORMAT_R16G16B16A16_SNORM).
      VA(DXGI_FORMAT_R16G16B16A16_SNORM).
      RV(DXGI_FORMAT_R16G16B16A16_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGFloat,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32_TYPELESS).
      RT(DXGI_FORMAT_R32G32_FLOAT).
      VA(DXGI_FORMAT_R32G32_FLOAT).
      RV(DXGI_FORMAT_R32G32_FLOAT));

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGUInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32_TYPELESS).
      RT(DXGI_FORMAT_R32G32_UINT).
      VA(DXGI_FORMAT_R32G32_UINT).
      RV(DXGI_FORMAT_R32G32_UINT));

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32G32_TYPELESS).
      RT(DXGI_FORMAT_R32G32_SINT).
      VA(DXGI_FORMAT_R32G32_SINT).
      RV(DXGI_FORMAT_R32G32_SINT));

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGB10A2UInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R10G10B10A2_TYPELESS).
      RT(DXGI_FORMAT_R10G10B10A2_UINT).
      VA(DXGI_FORMAT_R10G10B10A2_UINT).
      RV(DXGI_FORMAT_R10G10B10A2_UINT));

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGB10A2UIntNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R10G10B10A2_TYPELESS).
      RT(DXGI_FORMAT_R10G10B10A2_UNORM).
      VA(DXGI_FORMAT_R10G10B10A2_UNORM).
      RV(DXGI_FORMAT_R10G10B10A2_UNORM));

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RG11B10Float,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R11G11B10_FLOAT).
      RT(DXGI_FORMAT_R11G11B10_FLOAT).
      VA(DXGI_FORMAT_R11G11B10_FLOAT).
      RV(DXGI_FORMAT_R11G11B10_FLOAT));

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAUByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8B8A8_TYPELESS).
      RT(DXGI_FORMAT_R8G8B8A8_UNORM).
      VA(DXGI_FORMAT_R8G8B8A8_UNORM).
      RV(DXGI_FORMAT_R8G8B8A8_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAUByteNormalizedsRGB,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8B8A8_TYPELESS).
      RT(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB).
      RV(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAUByte,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8B8A8_TYPELESS).
      RT(DXGI_FORMAT_R8G8B8A8_UINT).
      VA(DXGI_FORMAT_R8G8B8A8_UINT).
      RV(DXGI_FORMAT_R8G8B8A8_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8B8A8_TYPELESS).
      RT(DXGI_FORMAT_R8G8B8A8_SNORM).
      VA(DXGI_FORMAT_R8G8B8A8_SNORM).
      RV(DXGI_FORMAT_R8G8B8A8_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGBAByte,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8B8A8_TYPELESS).
      RT(DXGI_FORMAT_R8G8B8A8_SINT).
      VA(DXGI_FORMAT_R8G8B8A8_SINT).
      RV(DXGI_FORMAT_R8G8B8A8_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGHalf,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16_TYPELESS).
      RT(DXGI_FORMAT_R16G16_FLOAT).
      VA(DXGI_FORMAT_R16G16_FLOAT).
      RV(DXGI_FORMAT_R16G16_FLOAT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGUShort,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16_TYPELESS).
      RT(DXGI_FORMAT_R16G16_UINT).
      VA(DXGI_FORMAT_R16G16_UINT).
      RV(DXGI_FORMAT_R16G16_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGUShortNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16_TYPELESS).
      RT(DXGI_FORMAT_R16G16_UNORM).
      VA(DXGI_FORMAT_R16G16_UNORM).
      RV(DXGI_FORMAT_R16G16_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGShort,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16_TYPELESS).
      RT(DXGI_FORMAT_R16G16_SINT).
      VA(DXGI_FORMAT_R16G16_SINT).
      RV(DXGI_FORMAT_R16G16_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGShortNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16G16_TYPELESS).
      RT(DXGI_FORMAT_R16G16_SNORM).
      VA(DXGI_FORMAT_R16G16_SNORM).
      RV(DXGI_FORMAT_R16G16_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGUByte,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8_TYPELESS).
      RT(DXGI_FORMAT_R8G8_UINT).
      VA(DXGI_FORMAT_R8G8_UINT).
      RV(DXGI_FORMAT_R8G8_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGUByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8_TYPELESS).
      RT(DXGI_FORMAT_R8G8_UNORM).
      VA(DXGI_FORMAT_R8G8_UNORM).
      RV(DXGI_FORMAT_R8G8_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGByte,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8_TYPELESS).
      RT(DXGI_FORMAT_R8G8_SINT).
      VA(DXGI_FORMAT_R8G8_SINT).
      RV(DXGI_FORMAT_R8G8_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RGByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8G8_TYPELESS).
      RT(DXGI_FORMAT_R8G8_SNORM).
      VA(DXGI_FORMAT_R8G8_SNORM).
      RV(DXGI_FORMAT_R8G8_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::DFloat,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32_TYPELESS).
      RT(DXGI_FORMAT_D32_FLOAT).
      RV(DXGI_FORMAT_D32_FLOAT).
      D(DXGI_FORMAT_D32_FLOAT).
      DS(DXGI_FORMAT_D32_FLOAT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RFloat,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32_TYPELESS).
      RT(DXGI_FORMAT_R32_FLOAT).
      VA(DXGI_FORMAT_R32_FLOAT).
      RV(DXGI_FORMAT_R32_FLOAT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RUInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32_TYPELESS).
      RT(DXGI_FORMAT_R32_UINT).
      VA(DXGI_FORMAT_R32_UINT).
      RV(DXGI_FORMAT_R32_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RInt,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R32_TYPELESS).
      RT(DXGI_FORMAT_R32_SINT).
      VA(DXGI_FORMAT_R32_SINT).
      RV(DXGI_FORMAT_R32_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RHalf,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16_TYPELESS).
      RT(DXGI_FORMAT_R16_FLOAT).
      VA(DXGI_FORMAT_R16_FLOAT).
      RV(DXGI_FORMAT_R16_FLOAT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RUShort,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16_TYPELESS).
      RT(DXGI_FORMAT_R16_UINT).
      VA(DXGI_FORMAT_R16_UINT).
      RV(DXGI_FORMAT_R16_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RUShortNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16_TYPELESS).
      RT(DXGI_FORMAT_R16_UNORM).
      VA(DXGI_FORMAT_R16_UNORM).
      RV(DXGI_FORMAT_R16_UNORM).
      D(DXGI_FORMAT_D16_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RShort,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16_TYPELESS).
      RT(DXGI_FORMAT_R16_SINT).
      VA(DXGI_FORMAT_R16_SINT).
      RV(DXGI_FORMAT_R16_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RShortNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R16_TYPELESS).
      RT(DXGI_FORMAT_R16_SNORM).
      VA(DXGI_FORMAT_R16_SNORM).
      RV(DXGI_FORMAT_R16_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RUByte,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8_TYPELESS).
      RT(DXGI_FORMAT_R8_UINT).
      VA(DXGI_FORMAT_R8_UINT).
      RV(DXGI_FORMAT_R8_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RUByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8_TYPELESS).
      RT(DXGI_FORMAT_R8_UNORM).
      VA(DXGI_FORMAT_R8_UNORM).
      RV(DXGI_FORMAT_R8_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RByte,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8_TYPELESS).
      RT(DXGI_FORMAT_R8_SINT).
      VA(DXGI_FORMAT_R8_SINT).
      RV(DXGI_FORMAT_R8_SINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::RByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8_TYPELESS).
      RT(DXGI_FORMAT_R8_SNORM).
      VA(DXGI_FORMAT_R8_SNORM).
      RV(DXGI_FORMAT_R8_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::AUByteNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R8_TYPELESS).
      RT(DXGI_FORMAT_A8_UNORM).
      VA(DXGI_FORMAT_A8_UNORM).
      RV(DXGI_FORMAT_A8_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::D24S8,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_R24G8_TYPELESS).
      DS(DXGI_FORMAT_D24_UNORM_S8_UINT).
      D(DXGI_FORMAT_R24_UNORM_X8_TYPELESS).
      S(DXGI_FORMAT_X24_TYPELESS_G8_UINT)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC1,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC1_TYPELESS).
      RV(DXGI_FORMAT_BC1_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC1sRGB,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC1_TYPELESS).
      RV(DXGI_FORMAT_BC1_UNORM_SRGB)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC2,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC2_TYPELESS).
      RV(DXGI_FORMAT_BC2_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC2sRGB,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC2_TYPELESS).
      RV(DXGI_FORMAT_BC2_UNORM_SRGB)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC3,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC3_TYPELESS).
      RV(DXGI_FORMAT_BC3_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC3sRGB,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC3_TYPELESS).
      RV(DXGI_FORMAT_BC3_UNORM_SRGB)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC4UNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC4_TYPELESS).
      RV(DXGI_FORMAT_BC4_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC4Normalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC4_TYPELESS).
      RV(DXGI_FORMAT_BC4_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC5UNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC5_TYPELESS).
      RV(DXGI_FORMAT_BC5_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC5Normalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC5_TYPELESS).
      RV(DXGI_FORMAT_BC5_SNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC6UFloat,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC6H_TYPELESS).
      RV(DXGI_FORMAT_BC6H_UF16)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC6Float,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC6H_TYPELESS).
      RV(DXGI_FORMAT_BC6H_SF16)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC7UNormalized,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC7_TYPELESS).
      RV(DXGI_FORMAT_BC7_UNORM)
      );

  m_FormatLookupTable.SetFormatInfo(
    ezGALResourceFormat::BC7UNormalizedsRGB,
    ezGALFormatLookupEntryDX11(DXGI_FORMAT_BC7_TYPELESS).
      RV(DXGI_FORMAT_BC7_UNORM_SRGB)
      );

}



EZ_STATICLINK_FILE(RendererDX11, RendererDX11_Device_Implementation_DeviceDX11);

