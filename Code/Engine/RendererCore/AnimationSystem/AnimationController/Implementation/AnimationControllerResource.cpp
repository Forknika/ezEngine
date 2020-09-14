#include <RendererCorePCH.h>

#include <Core/Assets/AssetFileHeader.h>
#include <Foundation/IO/MemoryStream.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationController.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationControllerResource.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationControllerResource, 1, ezRTTIDefaultAllocator<ezAnimationControllerResource>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_RESOURCE_IMPLEMENT_COMMON_CODE(ezAnimationControllerResource);
// clang-format on

ezAnimationControllerResource::ezAnimationControllerResource()
  : ezResource(ezResource::DoUpdate::OnAnyThread, 0)
{
}

ezAnimationControllerResource::~ezAnimationControllerResource() = default;

void ezAnimationControllerResource::DeserializeAnimationControllerState(ezAnimationController& out)
{
  ezMemoryStreamContainerWrapperStorage<ezDataBuffer> wrapper(&m_Storage);
  ezMemoryStreamReader reader(&wrapper);
  out.Deserialize(reader);
}

ezResourceLoadDesc ezAnimationControllerResource::UnloadData(Unload WhatToUnload)
{
  m_Storage.Clear();
  m_Storage.Compact();

  ezResourceLoadDesc d;
  d.m_State = ezResourceState::Unloaded;
  d.m_uiQualityLevelsDiscardable = 0;
  d.m_uiQualityLevelsLoadable = 0;
  return d;
}

ezResourceLoadDesc ezAnimationControllerResource::UpdateContent(ezStreamReader* Stream)
{
  ezResourceLoadDesc res;
  res.m_uiQualityLevelsDiscardable = 0;
  res.m_uiQualityLevelsLoadable = 0;

  if (Stream == nullptr)
  {
    res.m_State = ezResourceState::LoadedResourceMissing;
    return res;
  }

  // skip the absolute file path data that the standard file reader writes into the stream
  {
    ezStringBuilder sAbsFilePath;
    (*Stream) >> sAbsFilePath;
  }

  ezAssetFileHeader AssetHash;
  AssetHash.Read(*Stream);

  ezUInt32 uiDateSize = 0;
  *Stream >> uiDateSize;
  m_Storage.SetCountUninitialized(uiDateSize);
  Stream->ReadBytes(m_Storage.GetData(), uiDateSize);

  res.m_State = ezResourceState::Loaded;
  return res;
}

void ezAnimationControllerResource::UpdateMemoryUsage(MemoryUsage& out_NewMemoryUsage)
{
  out_NewMemoryUsage.m_uiMemoryGPU = 0;
  out_NewMemoryUsage.m_uiMemoryCPU = m_Storage.GetHeapMemoryUsage();
}
