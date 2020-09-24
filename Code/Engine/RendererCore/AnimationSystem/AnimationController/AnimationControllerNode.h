#pragma once

#include <Core/ResourceManager/ResourceHandle.h>
#include <Foundation/Time/Time.h>
#include <RendererCore/RendererCoreDLL.h>
#include <ozz/base/containers/vector.h>
#include <ozz/base/maths/soa_transform.h>

class ezSkeletonResource;
class ezAnimationController;
class ezStreamWriter;
class ezStreamReader;

class EZ_RENDERERCORE_DLL ezAnimCtrlPin : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimCtrlPin, ezReflectedClass);

public:
  ezInt16 m_iPinIndex = -1;

  ezResult Serialize(ezStreamWriter& stream) const;
  ezResult Deserialize(ezStreamReader& stream);
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
  bool IsTriggered(ezAnimationController& controller) const;
};

class EZ_RENDERERCORE_DLL ezAnimCtrlTriggerOutputPin : public ezAnimCtrlOutputPin
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimCtrlTriggerOutputPin, ezAnimCtrlOutputPin);

public:
  void SetTriggered(ezAnimationController& controller, bool triggered);

private:
  bool m_bTriggered = false;
};

class EZ_RENDERERCORE_DLL ezAnimGraphNode : public ezReflectedClass
{
  EZ_ADD_DYNAMIC_REFLECTION(ezAnimGraphNode, ezReflectedClass);

public:
  ezAnimGraphNode();
  virtual ~ezAnimGraphNode();

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
