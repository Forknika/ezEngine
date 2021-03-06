#include <ParticlePluginPCH.h>

#include <Core/Messages/CommonMessages.h>
#include <Core/Messages/DeleteObjectMessage.h>
#include <Core/Messages/UpdateLocalBoundsMessage.h>
#include <Core/World/WorldModule.h>
#include <Core/WorldSerializer/WorldReader.h>
#include <Core/WorldSerializer/WorldWriter.h>
#include <ParticlePlugin/Components/ParticleComponent.h>
#include <ParticlePlugin/Components/ParticleFinisherComponent.h>
#include <ParticlePlugin/Resources/ParticleEffectResource.h>
#include <ParticlePlugin/WorldModule/ParticleWorldModule.h>
#include <RendererCore/Pipeline/RenderData.h>
#include <RendererCore/Pipeline/View.h>

//////////////////////////////////////////////////////////////////////////

ezParticleComponentManager::ezParticleComponentManager(ezWorld* pWorld)
  : SUPER(pWorld)
{
}

void ezParticleComponentManager::Initialize()
{
  {
    auto desc = ezWorldModule::UpdateFunctionDesc(
      ezWorldModule::UpdateFunction(&ezParticleComponentManager::Update, this), "ezParticleComponentManager::Update");
    desc.m_bOnlyUpdateWhenSimulating = true;
    RegisterUpdateFunction(desc);
  }

  // it is necessary to do a late transform update pass, so that particle effects appear exactly at their parent's location
  // especially for effects that are 'simulated in local space' and should appear absolutely glued to their parent
  {
    auto desc = ezWorldModule::UpdateFunctionDesc(
      ezWorldModule::UpdateFunction(&ezParticleComponentManager::UpdateTransforms, this), "ezParticleComponentManager::UpdateTransforms");
    desc.m_bOnlyUpdateWhenSimulating = true;
    desc.m_Phase = ezWorldModule::UpdateFunctionDesc::Phase::PostTransform;
    RegisterUpdateFunction(desc);
  }
}

void ezParticleComponentManager::Update(const ezWorldModule::UpdateContext& context)
{
  for (auto it = this->m_ComponentStorage.GetIterator(context.m_uiFirstComponentIndex, context.m_uiComponentCount); it.IsValid(); ++it)
  {
    ComponentType* pComponent = it;
    if (pComponent->IsActiveAndInitialized())
    {
      pComponent->Update();
    }
  }
}

void ezParticleComponentManager::UpdateTransforms(const ezWorldModule::UpdateContext& context)
{
  for (auto it = this->m_ComponentStorage.GetIterator(context.m_uiFirstComponentIndex, context.m_uiComponentCount); it.IsValid(); ++it)
  {
    ComponentType* pComponent = it;
    if (pComponent->IsActiveAndInitialized())
    {
      pComponent->UpdateTransform();
    }
  }
}

//////////////////////////////////////////////////////////////////////////

// clang-format off
EZ_BEGIN_COMPONENT_TYPE(ezParticleComponent, 5, ezComponentMode::Static)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ACCESSOR_PROPERTY("Effect", GetParticleEffectFile, SetParticleEffectFile)->AddAttributes(new ezAssetBrowserAttribute("Particle Effect")),
    EZ_MEMBER_PROPERTY("SpawnAtStart", m_bSpawnAtStart)->AddAttributes(new ezDefaultValueAttribute(true)),
    EZ_ENUM_MEMBER_PROPERTY("OnFinishedAction", ezOnComponentFinishedAction2, m_OnFinishedAction),
    EZ_MEMBER_PROPERTY("MinRestartDelay", m_MinRestartDelay),
    EZ_MEMBER_PROPERTY("RestartDelayRange", m_RestartDelayRange),
    EZ_MEMBER_PROPERTY("RandomSeed", m_uiRandomSeed),
    EZ_ENUM_MEMBER_PROPERTY("SpawnDirection", ezBasisAxis, m_SpawnDirection)->AddAttributes(new ezDefaultValueAttribute((ezInt32)ezBasisAxis::PositiveZ)),
    EZ_MEMBER_PROPERTY("IgnoreOwnerRotation", m_bIgnoreOwnerRotation),
    EZ_MEMBER_PROPERTY("SharedInstanceName", m_sSharedInstanceName),
    EZ_MAP_ACCESSOR_PROPERTY("Parameters", GetParameters, GetParameter, SetParameter, RemoveParameter)->AddAttributes(new ezExposedParametersAttribute("Effect"), new ezExposeColorAlphaAttribute),
  }
  EZ_END_PROPERTIES;
  EZ_BEGIN_ATTRIBUTES
  {
    new ezCategoryAttribute("Effects"),
  }
  EZ_END_ATTRIBUTES;
  EZ_BEGIN_MESSAGEHANDLERS
  {
    EZ_MESSAGE_HANDLER(ezMsgSetPlaying, OnMsgSetPlaying),
    EZ_MESSAGE_HANDLER(ezMsgExtractRenderData, OnMsgExtractRenderData),
    EZ_MESSAGE_HANDLER(ezMsgDeleteGameObject, OnMsgDeleteGameObject),
  }
  EZ_END_MESSAGEHANDLERS;
  EZ_BEGIN_FUNCTIONS
  {
    EZ_SCRIPT_FUNCTION_PROPERTY(StartEffect),
    EZ_SCRIPT_FUNCTION_PROPERTY(StopEffect),
    EZ_SCRIPT_FUNCTION_PROPERTY(InterruptEffect),
    EZ_SCRIPT_FUNCTION_PROPERTY(IsEffectActive),
  }
  EZ_END_FUNCTIONS;
}
EZ_END_COMPONENT_TYPE
// clang-format on

ezParticleComponent::ezParticleComponent() = default;
ezParticleComponent::~ezParticleComponent() = default;

void ezParticleComponent::OnDeactivated()
{
  m_EffectController.Invalidate();

  ezRenderComponent::OnDeactivated();
}

void ezParticleComponent::SerializeComponent(ezWorldWriter& stream) const
{
  auto& s = stream.GetStream();

  s << m_hEffectResource;
  s << m_bSpawnAtStart;

  // Version 1
  {
    bool bAutoRestart = false;
    s << bAutoRestart;
  }

  s << m_MinRestartDelay;
  s << m_RestartDelayRange;
  s << m_RestartTime;
  s << m_uiRandomSeed;
  s << m_sSharedInstanceName;

  // Version 2
  s << m_FloatParams.GetCount();
  for (ezUInt32 i = 0; i < m_FloatParams.GetCount(); ++i)
  {
    s << m_FloatParams[i].m_sName;
    s << m_FloatParams[i].m_Value;
  }
  s << m_ColorParams.GetCount();
  for (ezUInt32 i = 0; i < m_ColorParams.GetCount(); ++i)
  {
    s << m_ColorParams[i].m_sName;
    s << m_ColorParams[i].m_Value;
  }

  // Version 3
  s << m_OnFinishedAction;

  // version 4
  s << m_bIgnoreOwnerRotation;

  // version 5
  s << m_SpawnDirection;

  /// \todo store effect state
}

void ezParticleComponent::DeserializeComponent(ezWorldReader& stream)
{
  auto& s = stream.GetStream();
  const ezUInt32 uiVersion = stream.GetComponentTypeVersion(GetStaticRTTI());

  s >> m_hEffectResource;
  s >> m_bSpawnAtStart;

  // Version 1
  {
    bool bAutoRestart = false;
    s >> bAutoRestart;
  }

  s >> m_MinRestartDelay;
  s >> m_RestartDelayRange;
  s >> m_RestartTime;
  s >> m_uiRandomSeed;
  s >> m_sSharedInstanceName;

  if (uiVersion >= 2)
  {
    ezUInt32 numFloats, numColors;

    s >> numFloats;
    m_FloatParams.SetCountUninitialized(numFloats);

    for (ezUInt32 i = 0; i < m_FloatParams.GetCount(); ++i)
    {
      s >> m_FloatParams[i].m_sName;
      s >> m_FloatParams[i].m_Value;
    }

    m_bFloatParamsChanged = numFloats > 0;

    s >> numColors;
    m_ColorParams.SetCountUninitialized(numColors);

    for (ezUInt32 i = 0; i < m_ColorParams.GetCount(); ++i)
    {
      s >> m_ColorParams[i].m_sName;
      s >> m_ColorParams[i].m_Value;
    }

    m_bColorParamsChanged = numColors > 0;
  }

  if (uiVersion >= 3)
  {
    s >> m_OnFinishedAction;
  }

  if (uiVersion >= 4)
  {
    s >> m_bIgnoreOwnerRotation;
  }

  if (uiVersion >= 5)
  {
    s >> m_SpawnDirection;
  }
}

bool ezParticleComponent::StartEffect()
{
  // stop any previous effect
  m_EffectController.Invalidate();

  if (m_hEffectResource.IsValid())
  {
    ezParticleWorldModule* pModule = GetWorld()->GetOrCreateModule<ezParticleWorldModule>();

    m_EffectController.Create(m_hEffectResource, pModule, m_uiRandomSeed, m_sSharedInstanceName, this, m_FloatParams, m_ColorParams);

    SetPfxTransform();

    m_bFloatParamsChanged = false;
    m_bColorParamsChanged = false;

    return true;
  }

  return false;
}

void ezParticleComponent::StopEffect()
{
  m_EffectController.Invalidate();
}

void ezParticleComponent::InterruptEffect()
{
  m_EffectController.StopImmediate();
}

bool ezParticleComponent::IsEffectActive() const
{
  return m_EffectController.IsAlive();
}


void ezParticleComponent::OnMsgSetPlaying(ezMsgSetPlaying& msg)
{
  if (msg.m_bPlay)
  {
    StartEffect();
  }
  else
  {
    StopEffect();
  }
}

void ezParticleComponent::SetParticleEffect(const ezParticleEffectResourceHandle& hEffect)
{
  m_EffectController.Invalidate();

  m_hEffectResource = hEffect;

  TriggerLocalBoundsUpdate();
}


void ezParticleComponent::SetParticleEffectFile(const char* szFile)
{
  ezParticleEffectResourceHandle hEffect;

  if (!ezStringUtils::IsNullOrEmpty(szFile))
  {
    hEffect = ezResourceManager::LoadResource<ezParticleEffectResource>(szFile);
  }

  SetParticleEffect(hEffect);
}


const char* ezParticleComponent::GetParticleEffectFile() const
{
  if (!m_hEffectResource.IsValid())
    return "";

  return m_hEffectResource.GetResourceID();
}


ezResult ezParticleComponent::GetLocalBounds(ezBoundingBoxSphere& bounds, bool& bAlwaysVisible)
{
  if (m_EffectController.IsAlive())
  {
    ezBoundingBoxSphere volume;
    m_uiBVolumeUpdateCounter = m_EffectController.GetBoundingVolume(volume);

    if (m_SpawnDirection != ezBasisAxis::PositiveZ)
    {
      const ezQuat qRot = ezBasisAxis::GetBasisRotation(ezBasisAxis::PositiveZ, m_SpawnDirection);
      volume.Transform(qRot.GetAsMat4());
    }

    if (m_bIgnoreOwnerRotation)
    {
      volume.Transform((-GetOwner()->GetGlobalRotation()).GetAsMat4());
    }

    if (m_uiBVolumeUpdateCounter != 0)
    {
      bounds.ExpandToInclude(volume);
      return EZ_SUCCESS;
    }
  }

  return EZ_FAILURE;
}


void ezParticleComponent::OnMsgExtractRenderData(ezMsgExtractRenderData& msg) const
{
  // do not extract particles during shadow map rendering
  if (msg.m_pView->GetCameraUsageHint() == ezCameraUsageHint::Shadow)
    return;

  m_EffectController.SetIsInView();
}

void ezParticleComponent::OnMsgDeleteGameObject(ezMsgDeleteGameObject& msg)
{
  ezOnComponentFinishedAction2::HandleDeleteObjectMsg(msg, m_OnFinishedAction);
}

void ezParticleComponent::Update()
{
  if (!m_EffectController.IsAlive() && m_bSpawnAtStart)
  {
    if (StartEffect())
    {
      m_bSpawnAtStart = false;

      if (m_EffectController.IsContinuousEffect())
      {
        if (m_bIfContinuousStopRightAway)
        {
          StopEffect();
        }
        else
        {
          m_bSpawnAtStart = true;
        }
      }
    }
  }

  if (!m_EffectController.IsAlive() && (m_OnFinishedAction == ezOnComponentFinishedAction2::Restart))
  {
    const ezTime tNow = GetWorld()->GetClock().GetAccumulatedTime();

    if (m_RestartTime == ezTime())
    {
      const ezTime tDiff =
        ezTime::Seconds(GetWorld()->GetRandomNumberGenerator().DoubleInRange(m_MinRestartDelay.GetSeconds(), m_RestartDelayRange.GetSeconds()));

      m_RestartTime = tNow + tDiff;
    }
    else if (m_RestartTime <= tNow)
    {
      m_RestartTime.SetZero();
      StartEffect();
    }
  }

  if (m_EffectController.IsAlive())
  {
    if (m_bFloatParamsChanged)
    {
      m_bFloatParamsChanged = false;

      for (ezUInt32 i = 0; i < m_FloatParams.GetCount(); ++i)
      {
        const auto& e = m_FloatParams[i];
        m_EffectController.SetParameter(e.m_sName, e.m_Value);
      }
    }

    if (m_bColorParamsChanged)
    {
      m_bColorParamsChanged = false;

      for (ezUInt32 i = 0; i < m_ColorParams.GetCount(); ++i)
      {
        const auto& e = m_ColorParams[i];
        m_EffectController.SetParameter(e.m_sName, e.m_Value);
      }
    }

    SetPfxTransform();

    CheckBVolumeUpdate();
  }
  else
  {
    ezOnComponentFinishedAction2::HandleFinishedAction(this, m_OnFinishedAction);
  }
}

void ezParticleComponent::UpdateTransform()
{
  if (m_EffectController.IsAlive())
  {
    SetPfxTransform();
  }
}

void ezParticleComponent::CheckBVolumeUpdate()
{
  ezBoundingBoxSphere bvol;
  if (m_uiBVolumeUpdateCounter < m_EffectController.GetBoundingVolume(bvol))
  {
    TriggerLocalBoundsUpdate();
  }
}

const ezRangeView<const char*, ezUInt32> ezParticleComponent::GetParameters() const
{
  return ezRangeView<const char*, ezUInt32>([this]() -> ezUInt32 { return 0; },
    [this]() -> ezUInt32 { return m_FloatParams.GetCount() + m_ColorParams.GetCount(); }, [this](ezUInt32& it) { ++it; },
    [this](const ezUInt32& it) -> const char* {
      if (it < m_FloatParams.GetCount())
        return m_FloatParams[it].m_sName.GetData();
      else
        return m_ColorParams[it - m_FloatParams.GetCount()].m_sName.GetData();
    });
}

void ezParticleComponent::SetParameter(const char* szKey, const ezVariant& var)
{
  const ezTempHashedString th(szKey);
  if (var.CanConvertTo<float>())
  {
    float value = var.ConvertTo<float>();

    for (ezUInt32 i = 0; i < m_FloatParams.GetCount(); ++i)
    {
      if (m_FloatParams[i].m_sName == th)
      {
        if (m_FloatParams[i].m_Value != value)
        {
          m_bFloatParamsChanged = true;
          m_FloatParams[i].m_Value = value;
        }
        return;
      }
    }

    m_bFloatParamsChanged = true;
    auto& e = m_FloatParams.ExpandAndGetRef();
    e.m_sName.Assign(szKey);
    e.m_Value = value;

    return;
  }

  if (var.CanConvertTo<ezColor>())
  {
    ezColor value = var.ConvertTo<ezColor>();

    for (ezUInt32 i = 0; i < m_ColorParams.GetCount(); ++i)
    {
      if (m_ColorParams[i].m_sName == th)
      {
        if (m_ColorParams[i].m_Value != value)
        {
          m_bColorParamsChanged = true;
          m_ColorParams[i].m_Value = value;
        }
        return;
      }
    }

    m_bColorParamsChanged = true;
    auto& e = m_ColorParams.ExpandAndGetRef();
    e.m_sName.Assign(szKey);
    e.m_Value = value;

    return;
  }
}

void ezParticleComponent::RemoveParameter(const char* szKey)
{
  const ezTempHashedString th(szKey);

  for (ezUInt32 i = 0; i < m_FloatParams.GetCount(); ++i)
  {
    if (m_FloatParams[i].m_sName == th)
    {
      m_FloatParams.RemoveAtAndSwap(i);
      return;
    }
  }

  for (ezUInt32 i = 0; i < m_ColorParams.GetCount(); ++i)
  {
    if (m_ColorParams[i].m_sName == th)
    {
      m_ColorParams.RemoveAtAndSwap(i);
      return;
    }
  }
}

bool ezParticleComponent::GetParameter(const char* szKey, ezVariant& out_value) const
{
  const ezTempHashedString th(szKey);

  for (const auto& e : m_FloatParams)
  {
    if (e.m_sName == th)
    {
      out_value = e.m_Value;
      return true;
    }
  }
  for (const auto& e : m_ColorParams)
  {
    if (e.m_sName == th)
    {
      out_value = e.m_Value;
      return true;
    }
  }
  return false;
}

void ezParticleComponent::SetPfxTransform()
{
  auto pOwner = GetOwner();
  ezTransform transform = pOwner->GetGlobalTransform();

  const ezQuat qRot = ezBasisAxis::GetBasisRotation(ezBasisAxis::PositiveZ, m_SpawnDirection);

  if (m_bIgnoreOwnerRotation)
  {
    transform.m_qRotation = qRot;
  }
  else
  {
    transform.m_qRotation = transform.m_qRotation * qRot;
  }


  m_EffectController.SetTransform(transform, pOwner->GetVelocity());
}

EZ_STATICLINK_FILE(ParticlePlugin, ParticlePlugin_Components_ParticleComponent);
