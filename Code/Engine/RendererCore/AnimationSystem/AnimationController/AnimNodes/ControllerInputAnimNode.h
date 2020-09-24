#pragma once

#include <RendererCore/AnimationSystem/AnimationController/AnimationControllerNode.h>

class EZ_RENDERERCORE_DLL ezControllerInputAnimNode : public ezAnimGraphNode
{
  EZ_ADD_DYNAMIC_REFLECTION(ezControllerInputAnimNode, ezAnimGraphNode);

public:
  virtual float UpdateWeight(ezTime tDiff) override;
  virtual void Step(ezTime ov, const ezSkeletonResource* pSkeleton) override;

  virtual ezResult SerializeNode(ezStreamWriter& stream) const override;
  virtual ezResult DeserializeNode(ezStreamReader& stream) override;

private:
  ezAnimCtrlTriggerOutputPin m_ButtonA; // [ property ]
  ezAnimCtrlTriggerOutputPin m_ButtonB; // [ property ]
  ezAnimCtrlTriggerOutputPin m_ButtonX; // [ property ]
  ezAnimCtrlTriggerOutputPin m_ButtonY; // [ property ]

  ezAnimCtrlTriggerOutputPin m_StickLeft;  // [ property ]
  ezAnimCtrlTriggerOutputPin m_StickRight; // [ property ]
  ezAnimCtrlTriggerOutputPin m_StickUp;    // [ property ]
  ezAnimCtrlTriggerOutputPin m_StickDown;  // [ property ]
};
