#pragma once

#include <Core/ResourceManager/ResourceHandle.h>
#include <Foundation/Time/Time.h>
#include <RendererCore/AnimationSystem/AnimationClipResource.h>
#include <RendererCore/RendererCoreDLL.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>

class ezSkeletonResource;
class ezAnimationController;
class ezStreamWriter;
class ezStreamReader;
using ezAnimationClipResourceHandle = ezTypedResourceHandle<class ezAnimationClipResource>;

class EZ_RENDERERCORE_DLL ezAnimCtrlPin : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimCtrlPin, ezReflectedClass);

public:
  ezInt16 m_iPinIndex = -1;
};

class EZ_RENDERERCORE_DLL ezAnimCtrlInputPin : public ezAnimCtrlPin
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimCtrlInputPin, ezAnimCtrlPin);

public:
};

class EZ_RENDERERCORE_DLL ezAnimCtrlOutputPin : public ezAnimCtrlPin
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimCtrlOutputPin, ezAnimCtrlPin);

public:
};

class EZ_RENDERERCORE_DLL ezAnimCtrlTriggerInputPin : public ezAnimCtrlInputPin
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimCtrlTriggerInputPin, ezAnimCtrlInputPin);

public:
};

class EZ_RENDERERCORE_DLL ezAnimCtrlTriggerOutputPin : public ezAnimCtrlOutputPin
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimCtrlTriggerOutputPin, ezAnimCtrlOutputPin);

public:
};

class EZ_RENDERERCORE_DLL ezAnimationControllerNode : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimationControllerNode, ezReflectedClass);

public:
  ezAnimationControllerNode();
  virtual ~ezAnimationControllerNode();

  virtual float UpdateWeight(ezTime tDiff) = 0;
  virtual void Step(ezTime tDiff, const ezSkeletonResource* pSkeleton) = 0;
  const ezVec3& GetRootMotion() const { return m_vRootMotion; }

  virtual ezResult SerializeNode(ezStreamWriter& stream) const = 0;
  virtual ezResult DeserializeNode(ezStreamReader& stream) = 0;

protected:
  friend ezAnimationController;

  ezVec3 m_vRootMotion = ezVec3::ZeroVector();
  ezAnimationController* m_pOwner = nullptr;
  ozz::vector<ozz::math::SoaTransform> m_ozzLocalTransforms;
  ozz::vector<ozz::math::SimdFloat4> m_ozzBlendWeightsSOA;
};

class EZ_RENDERERCORE_DLL ezSampleAnimGraphNode : public ezAnimationControllerNode
{
  EZ_ADD_DYNAMIC_REFLECTION(ezSampleAnimGraphNode, ezAnimationControllerNode);

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
  ezAnimCtrlTriggerInputPin m_TriggerInputPin;   // [ property ]
  ezAnimCtrlTriggerOutputPin m_TriggerOutputPin; // [ property ]

  ezHashedString m_sBlackboardEntry;
  ezHashedString m_sPartialBlendingRootBone;
  ezTime m_PlaybackTime;
  ozz::animation::SamplingCache m_ozzSamplingCache;
  float m_fCurWeight = 0.0f;
  bool m_bIsRampingUpOrDown = false;
};
