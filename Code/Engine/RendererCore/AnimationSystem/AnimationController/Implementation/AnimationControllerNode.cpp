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
    EZ_MEMBER_PROPERTY("PinIdx", m_iPinIndex),
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

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezSampleAnimGraphNode, 1, ezRTTIDefaultAllocator<ezSampleAnimGraphNode>)
  {
    EZ_BEGIN_PROPERTIES
    {
      EZ_ACCESSOR_PROPERTY("AnimationClip", GetAnimationClip, SetAnimationClip)->AddAttributes(new ezAssetBrowserAttribute("Animation Clip")),
      EZ_ACCESSOR_PROPERTY("BlackboardEntry", GetBlackboardEntry, SetBlackboardEntry),
      EZ_MEMBER_PROPERTY("Speed", m_fSpeed)->AddAttributes(new ezDefaultValueAttribute(1.0f)),
      EZ_MEMBER_PROPERTY("RampUpTime", m_RampUp),
      EZ_MEMBER_PROPERTY("RampDownTime", m_RampDown),
      EZ_ACCESSOR_PROPERTY("PartialBlendingRootBone", GetPartialBlendingRootBone, SetPartialBlendingRootBone),

      EZ_MEMBER_PROPERTY("Active", m_Active),
    }
    EZ_END_PROPERTIES;
  }
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezResult ezSampleAnimGraphNode::SerializeNode(ezStreamWriter& stream) const
{
  stream.WriteVersion(1);

  EZ_SUCCEED_OR_RETURN(SUPER::SerializeNode(stream));

  stream << m_sBlackboardEntry;
  stream << m_RampUp;
  stream << m_RampDown;
  stream << m_PlaybackTime;
  stream << m_hAnimationClip;
  stream << m_fSpeed;
  stream << m_sPartialBlendingRootBone;

  EZ_SUCCEED_OR_RETURN(m_Active.Serialize(stream));

  return EZ_SUCCESS;
}

ezResult ezSampleAnimGraphNode::DeserializeNode(ezStreamReader& stream)
{
  stream.ReadVersion(1);

  EZ_SUCCEED_OR_RETURN(SUPER::DeserializeNode(stream));

  stream >> m_sBlackboardEntry;
  stream >> m_RampUp;
  stream >> m_RampDown;
  stream >> m_PlaybackTime;
  stream >> m_hAnimationClip;
  stream >> m_fSpeed;
  stream >> m_sPartialBlendingRootBone;

  EZ_SUCCEED_OR_RETURN(m_Active.Deserialize(stream));

  return EZ_SUCCESS;
}


float ezSampleAnimGraphNode::UpdateWeight(ezTime tDiff)
{
  if (!m_Active.IsTriggered(*m_pOwner))
    return 0.0f;

  //const ezVariant vValue = m_pOwner->m_Blackboard.GetEntryValue(m_sBlackboardEntry);

  //if (!vValue.IsFloatingPoint())
  //  return 0.0f;

  m_vRootMotion.SetZero();

  const float fValue = 1.0f; //vValue.ConvertTo<float>();

  if (m_fCurWeight < fValue)
  {
    m_bIsRampingUpOrDown = true;
    m_fCurWeight += tDiff.AsFloatInSeconds() / m_RampUp.AsFloatInSeconds();
    m_fCurWeight = ezMath::Min(m_fCurWeight, fValue);
  }
  else if (m_fCurWeight > fValue)
  {
    m_bIsRampingUpOrDown = true;
    m_fCurWeight -= tDiff.AsFloatInSeconds() / m_RampDown.AsFloatInSeconds();
    m_fCurWeight = ezMath::Max(0.0f, m_fCurWeight);
  }
  else
  {
    m_bIsRampingUpOrDown = false;
  }

  if (m_fCurWeight <= 0.0f)
  {
    m_PlaybackTime.SetZero();
  }

  return m_fCurWeight;
}

void ezSampleAnimGraphNode::Step(ezTime tDiff, const ezSkeletonResource* pSkeleton)
{
  if (!m_hAnimationClip.IsValid())
    return;

  ezResourceLock<ezAnimationClipResource> pAnimClip(m_hAnimationClip, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (pAnimClip.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  const auto& skeleton = pSkeleton->GetDescriptor().m_Skeleton;
  const auto pOzzSkeleton = &pSkeleton->GetDescriptor().m_Skeleton.GetOzzSkeleton();

  const auto& animDesc = pAnimClip->GetDescriptor();

  m_PlaybackTime += tDiff * m_fSpeed;
  if (m_fSpeed > 0 && m_PlaybackTime > animDesc.GetDuration())
  {
    m_PlaybackTime -= animDesc.GetDuration();
  }
  else if (m_fSpeed < 0 && m_PlaybackTime < ezTime::Zero())
  {
    m_PlaybackTime += animDesc.GetDuration();
  }

  const ozz::animation::Animation* pOzzAnimation = &animDesc.GetMappedOzzAnimation(*pSkeleton);

  if (m_ozzSamplingCache.max_tracks() < pOzzAnimation->num_tracks())
  {
    m_ozzSamplingCache.Resize(pOzzAnimation->num_tracks());
    m_ozzLocalTransforms.resize(pOzzSkeleton->num_soa_joints());
  }

  {
    ozz::animation::SamplingJob job;
    job.animation = pOzzAnimation;
    job.cache = &m_ozzSamplingCache;
    job.ratio = m_PlaybackTime.AsFloatInSeconds() / animDesc.GetDuration().AsFloatInSeconds();
    job.output = make_span(m_ozzLocalTransforms);
    EZ_ASSERT_DEBUG(job.Validate(), "");
    job.Run();
  }

  if (!m_sPartialBlendingRootBone.IsEmpty())
  {
    if (m_bIsRampingUpOrDown || m_ozzBlendWeightsSOA.empty())
    {
      m_ozzBlendWeightsSOA.resize(pOzzSkeleton->num_soa_joints());
      ezMemoryUtils::ZeroFill<ezUInt8>((ezUInt8*)m_ozzBlendWeightsSOA.data(), m_ozzBlendWeightsSOA.size() * sizeof(ozz::math::SimdFloat4));

      int iRootBone = -1;
      for (int iBone = 0; iBone < pOzzSkeleton->num_joints(); ++iBone)
      {
        if (ezStringUtils::IsEqual(pOzzSkeleton->joint_names()[iBone], m_sPartialBlendingRootBone.GetData()))
        {
          iRootBone = iBone;
          break;
        }
      }

      const float fBoneWeight = 10.0f * m_fCurWeight;

      auto setBoneWeight = [&](int currentBone, int) {
        const int iJointIdx0 = currentBone / 4;
        const int iJointIdx1 = currentBone % 4;

        ozz::math::SimdFloat4& soa_weight = m_ozzBlendWeightsSOA.at(iJointIdx0);
        soa_weight = ozz::math::SetI(soa_weight, ozz::math::simd_float4::Load1(fBoneWeight), iJointIdx1);
      };

      ozz::animation::IterateJointsDF(*pOzzSkeleton, setBoneWeight, iRootBone);
    }
  }
  else
  {
    m_ozzBlendWeightsSOA.clear();
  }

  m_vRootMotion = pAnimClip->GetDescriptor().m_vConstantRootMotion * tDiff.AsFloatInSeconds();
}

void ezSampleAnimGraphNode::SetAnimationClip(const char* szFile)
{
  ezAnimationClipResourceHandle hResource;

  if (!ezStringUtils::IsNullOrEmpty(szFile))
  {
    hResource = ezResourceManager::LoadResource<ezAnimationClipResource>(szFile);
  }

  m_hAnimationClip = hResource;
}

const char* ezSampleAnimGraphNode::GetAnimationClip() const
{
  if (!m_hAnimationClip.IsValid())
    return "";

  return m_hAnimationClip.GetResourceID();
}

void ezSampleAnimGraphNode::SetBlackboardEntry(const char* szFile)
{
  m_sBlackboardEntry.Assign(szFile);
}

const char* ezSampleAnimGraphNode::GetBlackboardEntry() const
{
  return m_sBlackboardEntry.GetData();
}

void ezSampleAnimGraphNode::SetPartialBlendingRootBone(const char* szBone)
{
  m_sPartialBlendingRootBone.Assign(szBone);
}

const char* ezSampleAnimGraphNode::GetPartialBlendingRootBone() const
{
  return m_sPartialBlendingRootBone.GetData();
}



//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezControllerInputAnimGraphNode, 1, ezRTTIDefaultAllocator<ezControllerInputAnimGraphNode>)
  {
    EZ_BEGIN_PROPERTIES
    {
      EZ_MEMBER_PROPERTY("ButtonA", m_ButtonA),
      EZ_MEMBER_PROPERTY("ButtonB", m_ButtonB),
      EZ_MEMBER_PROPERTY("ButtonX", m_ButtonX),
      EZ_MEMBER_PROPERTY("ButtonY", m_ButtonY),
      EZ_MEMBER_PROPERTY("StickLeft", m_StickLeft),
      EZ_MEMBER_PROPERTY("StickRight", m_StickRight),
      EZ_MEMBER_PROPERTY("StickUp", m_StickUp),
      EZ_MEMBER_PROPERTY("StickDown", m_StickDown),
    }
    EZ_END_PROPERTIES;
  }
EZ_END_DYNAMIC_REFLECTED_TYPE;
// clang-format on

ezResult ezControllerInputAnimGraphNode::SerializeNode(ezStreamWriter& stream) const
{
  stream.WriteVersion(1);

  EZ_SUCCEED_OR_RETURN(SUPER::SerializeNode(stream));

  m_ButtonA.Serialize(stream);
  m_ButtonB.Serialize(stream);
  m_ButtonX.Serialize(stream);
  m_ButtonY.Serialize(stream);

  m_StickLeft.Serialize(stream);
  m_StickRight.Serialize(stream);
  m_StickUp.Serialize(stream);
  m_StickDown.Serialize(stream);

  return EZ_SUCCESS;
}

ezResult ezControllerInputAnimGraphNode::DeserializeNode(ezStreamReader& stream)
{
  stream.ReadVersion(1);

  EZ_SUCCEED_OR_RETURN(SUPER::DeserializeNode(stream));

  m_ButtonA.Deserialize(stream);
  m_ButtonB.Deserialize(stream);
  m_ButtonX.Deserialize(stream);
  m_ButtonY.Deserialize(stream);

  m_StickLeft.Deserialize(stream);
  m_StickRight.Deserialize(stream);
  m_StickUp.Deserialize(stream);
  m_StickDown.Deserialize(stream);

  return EZ_SUCCESS;
}

float ezControllerInputAnimGraphNode::UpdateWeight(ezTime tDiff)
{
  float fValue = 0.0f;

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_LeftStick_NegX, &fValue);
  m_StickLeft.SetTriggered(*m_pOwner, fValue > 0);

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_LeftStick_PosX, &fValue);
  m_StickRight.SetTriggered(*m_pOwner, fValue > 0);

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_LeftStick_NegY, &fValue);
  m_StickDown.SetTriggered(*m_pOwner, fValue > 0);

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_LeftStick_PosY, &fValue);
  m_StickUp.SetTriggered(*m_pOwner, fValue > 0);

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_ButtonA, &fValue);
  m_ButtonA.SetTriggered(*m_pOwner, fValue > 0);

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_ButtonB, &fValue);
  m_ButtonB.SetTriggered(*m_pOwner, fValue > 0);

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_ButtonX, &fValue);
  m_ButtonX.SetTriggered(*m_pOwner, fValue > 0);

  ezInputManager::GetInputSlotState(ezInputSlot_Controller0_ButtonY, &fValue);
  m_ButtonY.SetTriggered(*m_pOwner, fValue > 0);


  return 0.0f;
}

void ezControllerInputAnimGraphNode::Step(ezTime tDiff, const ezSkeletonResource* pSkeleton)
{
  EZ_ASSERT_NOT_IMPLEMENTED;
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
