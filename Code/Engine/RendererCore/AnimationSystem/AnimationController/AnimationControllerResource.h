#pragma once

#include <RendererCore/RendererCoreDLL.h>

#include <Core/ResourceManager/Resource.h>
#include <Foundation/Containers/DynamicArray.h>

class ezAnimationController;

//////////////////////////////////////////////////////////////////////////

using ezAnimationControllerResourceHandle = ezTypedResourceHandle<class ezAnimationControllerResource>;

class EZ_RENDERERCORE_DLL ezAnimationControllerResource : public ezResource
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimationControllerResource, ezResource);
  EZ_RESOURCE_DECLARE_COMMON_CODE(ezAnimationControllerResource);

public:
  ezAnimationControllerResource();
  ~ezAnimationControllerResource();

  void DeserializeAnimationControllerState(ezAnimationController& out);

private:
  virtual ezResourceLoadDesc UnloadData(Unload WhatToUnload) override;
  virtual ezResourceLoadDesc UpdateContent(ezStreamReader* Stream) override;
  virtual void UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage) override;

  ezDataBuffer m_Storage;
};
