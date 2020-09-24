#include <RendererCorePCH.h>

#include <AnimationSystem/AnimationClipResource.h>
#include <Core/Input/InputManager.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationController.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationControllerNode.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/skeleton_utils.h>

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimGraphNode, 1, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimCtrlPin, 1, ezRTTIDefaultAllocator<ezAnimCtrlPin>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("PinIdx", m_iPinIndex)->AddAttributes(new ezHiddenAttribute()),
  }
  EZ_END_PROPERTIES;
}
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimCtrlInputPin, 1, ezRTTIDefaultAllocator<ezAnimCtrlInputPin>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimCtrlOutputPin, 1, ezRTTIDefaultAllocator<ezAnimCtrlOutputPin>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimCtrlTriggerInputPin, 1, ezRTTIDefaultAllocator<ezAnimCtrlTriggerInputPin>)
EZ_END_DYNAMIC_REFLECTED_TYPE;

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimCtrlTriggerOutputPin, 1, ezRTTIDefaultAllocator<ezAnimCtrlTriggerOutputPin>)
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezAnimGraphNode::ezAnimGraphNode() = default;
ezAnimGraphNode::~ezAnimGraphNode() = default;

ezResult ezAnimGraphNode::SerializeNode(ezStreamWriter& stream) const
{
  stream.WriteVersion(1);
  return EZ_SUCCESS;
}

ezResult ezAnimGraphNode::DeserializeNode(ezStreamReader& stream)
{
  stream.ReadVersion(1);
  return EZ_SUCCESS;
}





ezResult ezAnimCtrlPin::Serialize(ezStreamWriter& stream) const
{
  stream << m_iPinIndex;
  return EZ_SUCCESS;
}

ezResult ezAnimCtrlPin::Deserialize(ezStreamReader& stream)
{
  stream >> m_iPinIndex;
  return EZ_SUCCESS;
}

void ezAnimCtrlTriggerOutputPin::SetTriggered(ezAnimationController& controller, bool triggered)
{
  if (m_iPinIndex < 0)
    return;

  if (m_bTriggered == triggered)
    return;

  m_bTriggered = triggered;

  const auto& map = controller.m_TriggerOutputToInputPinMapping[m_iPinIndex];

  const ezInt8 offset = triggered ? +1 : -1;

  // trigger or reset all input pins that are connected to this output pin
  for (ezUInt16 idx : map)
  {
    controller.m_TriggerInputPinStates[idx] += offset;
  }
}

bool ezAnimCtrlTriggerInputPin::IsTriggered(ezAnimationController& controller) const
{
  if (m_iPinIndex < 0)
    return false;

  return controller.m_TriggerInputPinStates[m_iPinIndex] > 0;
}
