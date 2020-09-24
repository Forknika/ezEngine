#pragma once

#include <Core/ResourceManager/ResourceHandle.h>
#include <RendererCore/AnimationSystem/AnimationClipResource.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationControllerNode.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>

class ezSkeletonResource;
class ezStreamWriter;
class ezStreamReader;
using ezAnimationClipResourceHandle = ezTypedResourceHandle<class ezAnimationClipResource>;

class EZ_RENDERERCORE_DLL ezPlayClipAnimNode : public ezAnimGraphNode
{
  EZ_ADD_DYNAMIC_REFLECTION(ezPlayClipAnimNode, ezAnimGraphNode);

public:
  virtual float UpdateWeight(ezTime tDiff) override;
  virtual void Step(ezTime ov, const ezSkeletonResource* pSkeleton) override;

  void SetAnimationClip(const char* szFile); // [ property ]
  const char* GetAnimationClip() const;      // [ property ]

  void SetBlackboardEntry(const char* szFile); // [ property ]
  const char* GetBlackboardEntry() const;      // [ property ]

  void SetPartialBlendingRootBone(const char* szBone); // [ property ]
  const char* GetPartialBlendingRootBone() const;      // [ property ]

  virtual ezResult SerializeNode(ezStreamWriter& stream) const override;
  virtual ezResult DeserializeNode(ezStreamReader& stream) override;

  ezTime m_RampUp;       // [ property ]
  ezTime m_RampDown;     // [ property ]
  float m_fSpeed = 1.0f; // [ property ]

  ezAnimationClipResourceHandle m_hAnimationClip;

private:
  ezAnimCtrlTriggerInputPin m_Active; // [ property ]

  ezHashedString m_sBlackboardEntry;
  ezHashedString m_sPartialBlendingRootBone;
  ezTime m_PlaybackTime;
  ozz::animation::SamplingCache m_ozzSamplingCache;
  float m_fCurWeight = 0.0f;
  bool m_bIsRampingUpOrDown = false;
};
