#pragma once

#include <RendererCore/RendererCoreDLL.h>

#include <Core/ResourceManager/ResourceHandle.h>
#include <Core/Utils/Blackboard.h>
#include <Foundation/Containers/DynamicArray.h>
#include <Foundation/Memory/AllocatorWrapper.h>
#include <Foundation/Types/UniquePtr.h>
#include <RendererCore/AnimationSystem/AnimGraph/AnimGraphNode.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>

class ezGameObject;
class ezStreamWriter;
class ezStreamReader;

using ezSkeletonResourceHandle = ezTypedResourceHandle<class ezSkeletonResource>;

class EZ_RENDERERCORE_DLL ezAnimGraph
{
  EZ_DISALLOW_COPY_AND_ASSIGN(ezAnimGraph);

public:
  ezAnimGraph();
  ~ezAnimGraph();

  void Update(ezTime tDiff);
  void SendResultTo(ezGameObject* pObject);
  const ezVec3& GetRootMotion() const { return m_vRootMotion; }

  ezDynamicArray<ezUniquePtr<ezAnimGraphNode>> m_Nodes;

  ezSkeletonResourceHandle m_hSkeleton;

  ezBlackboard m_Blackboard;

  ezResult Serialize(ezStreamWriter& stream) const;
  ezResult Deserialize(ezStreamReader& stream);

  ezDynamicArray<ezInt8> m_TriggerInputPinStates;
  ezDynamicArray<ezDynamicArray<ezUInt16>> m_TriggerOutputToInputPinMapping;

  /// \brief To be called by ezAnimGraphNode classes every frame that they want to affect animation
  void AddFrameBlendLayer(const ozz::animation::BlendingJob::Layer& layer);

  /// \brief To be called by ezAnimGraphNode classes every frame that they want to affect the root motion
  void AddFrameRootMotion(const ezVec3& motion);

private:
  ezDynamicArray<ozz::animation::BlendingJob::Layer> m_ozzBlendLayers;
  ozz::vector<ozz::math::SoaTransform> m_ozzLocalTransforms;
  ezDynamicArray<ezMat4, ezAlignedAllocatorWrapper> m_ModelSpaceTransforms;

  bool m_bFinalized = false;
  ezVec3 m_vRootMotion;
  void Finalize(const ezSkeletonResource* pSkeleton);
};
