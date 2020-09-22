#include <RendererCorePCH.h>

#include <Core/World/GameObject.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationController.h>
#include <RendererCore/AnimationSystem/SkeletonResource.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/skeleton.h>

ezAnimationController::ezAnimationController() = default;
ezAnimationController::~ezAnimationController() = default;

void ezAnimationController::Update(ezTime tDiff)
{
  if (!m_hSkeleton.IsValid())
    return;

  ezResourceLock<ezSkeletonResource> pSkeleton(m_hSkeleton, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (pSkeleton.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  auto pOzzSkeleton = &pSkeleton->GetDescriptor().m_Skeleton.GetOzzSkeleton();

  ezHybridArray<ozz::animation::BlendingJob::Layer, 8> layers;

  m_vRootMotion.SetZero();

  float fTotalWeight = 0.0f;
  for (const auto& pNode : m_Nodes)
  {
    pNode->m_pOwner = this;
    const float fWeight = pNode->UpdateWeight(tDiff);

    if (fWeight <= 0.0f)
      continue;

    fTotalWeight += fWeight;

    pNode->Step(tDiff, pSkeleton.GetPointer());

    auto& l = layers.ExpandAndGetRef();
    l.weight = fWeight;
    l.transform = make_span(pNode->m_ozzLocalTransforms);

    if (!pNode->m_ozzBlendWeightsSOA.empty())
    {
      l.joint_weights = make_span(pNode->m_ozzBlendWeightsSOA);
    }

    m_vRootMotion += pNode->GetRootMotion() * fWeight;
  }

  {
    m_ozzLocalTransforms.resize(pOzzSkeleton->num_soa_joints());

    ozz::animation::BlendingJob job;
    job.threshold = 0.1f;
    job.layers = ozz::span<const ozz::animation::BlendingJob::Layer>(begin(layers), end(layers));
    job.bind_pose = pOzzSkeleton->joint_bind_poses();
    job.output = make_span(m_ozzLocalTransforms);
    EZ_ASSERT_DEBUG(job.Validate(), "");
    job.Run();
  }

  m_bFinalized = false;
}

void ezAnimationController::Finalize(const ezSkeletonResource* pSkeleton)
{
  if (m_bFinalized)
    return;

  m_bFinalized = true;

  const ozz::animation::Skeleton* pOzzSkeleton = &pSkeleton->GetDescriptor().m_Skeleton.GetOzzSkeleton();

  {
    m_ModelSpaceTransforms.SetCountUninitialized(pOzzSkeleton->num_joints());

    ozz::animation::LocalToModelJob job;
    job.input = make_span(m_ozzLocalTransforms);
    job.output = ozz::span<ozz::math::Float4x4>(reinterpret_cast<ozz::math::Float4x4*>(begin(m_ModelSpaceTransforms)), reinterpret_cast<ozz::math::Float4x4*>(end(m_ModelSpaceTransforms)));
    job.skeleton = pOzzSkeleton;
    EZ_ASSERT_DEBUG(job.Validate(), "");
    job.Run();
  }
}

void ezAnimationController::SendResultTo(ezGameObject* pObject)
{
  if (!m_hSkeleton.IsValid())
    return;

  ezResourceLock<ezSkeletonResource> pSkeleton(m_hSkeleton, ezResourceAcquireMode::BlockTillLoaded_NeverFail);
  if (pSkeleton.GetAcquireResult() != ezResourceAcquireResult::Final)
    return;

  Finalize(pSkeleton.GetPointer());

  ezMsgAnimationPoseUpdated msg;
  msg.m_pSkeleton = &pSkeleton->GetDescriptor().m_Skeleton;
  msg.m_ModelTransforms = m_ModelSpaceTransforms;

  pObject->SendMessageRecursive(msg);
}

ezResult ezAnimationController::Serialize(ezStreamWriter& stream) const
{
  stream.WriteVersion(2);

  const ezUInt32 uiNumNodes = m_Nodes.GetCount();
  stream << uiNumNodes;

  for (const auto& node : m_Nodes)
  {
    stream << node->GetDynamicRTTI()->GetTypeName();

    node->SerializeNode(stream);
  }

  stream << m_hSkeleton;

  EZ_SUCCEED_OR_RETURN(m_Blackboard.Serialize(stream));

  EZ_SUCCEED_OR_RETURN(stream.WriteArray(m_TriggerInputPinStates));

  stream << m_TriggerOutputToInputPinMapping.GetCount();
  for (const auto& ar : m_TriggerOutputToInputPinMapping)
  {
    EZ_SUCCEED_OR_RETURN(stream.WriteArray(ar));
  }

  return EZ_SUCCESS;
}

ezResult ezAnimationController::Deserialize(ezStreamReader& stream)
{
  const auto uiVersion = stream.ReadVersion(2);

  ezUInt32 uiNumNodes = 0;
  stream >> uiNumNodes;
  m_Nodes.SetCount(uiNumNodes);

  ezStringBuilder sTypeName;

  for (auto& node : m_Nodes)
  {
    stream >> sTypeName;
    node = std::move(ezRTTI::FindTypeByName(sTypeName)->GetAllocator()->Allocate<ezAnimGraphNode>());

    node->DeserializeNode(stream);
  }

  stream >> m_hSkeleton;

  EZ_SUCCEED_OR_RETURN(m_Blackboard.Deserialize(stream));

  if (uiVersion >= 2)
  {
    EZ_SUCCEED_OR_RETURN(stream.ReadArray(m_TriggerInputPinStates));

    ezUInt32 sar = 0;
    stream >> sar;
    m_TriggerOutputToInputPinMapping.SetCount(sar);
    for (auto& ar : m_TriggerOutputToInputPinMapping)
    {
      EZ_SUCCEED_OR_RETURN(stream.ReadArray(ar));
    }
  }

  return EZ_SUCCESS;
}
