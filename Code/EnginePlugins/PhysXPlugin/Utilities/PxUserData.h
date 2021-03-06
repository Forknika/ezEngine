#pragma once

#include <PhysXPlugin/PhysXPluginDLL.h>

class ezComponent;
class ezPxDynamicActorComponent;
class ezPxStaticActorComponent;
class ezPxTriggerComponent;
class ezPxCharacterShapeComponent;
class ezPxShapeComponent;
class ezSurfaceResource;
class ezBreakableSheetComponent;

class ezPxUserData
{
public:
  EZ_DECLARE_POD_TYPE();

  ezPxUserData() = default;
  ~ezPxUserData() { Invalidate(); }

  EZ_ALWAYS_INLINE void Init(ezPxDynamicActorComponent* pObject)
  {
    m_Type = DynamicActorComponent;
    m_pObject = pObject;
  }

  EZ_ALWAYS_INLINE void Init(ezPxStaticActorComponent* pObject)
  {
    m_Type = StaticActorComponent;
    m_pObject = pObject;
  }

  EZ_ALWAYS_INLINE void Init(ezPxTriggerComponent* pObject)
  {
    m_Type = TriggerComponent;
    m_pObject = pObject;
  }

  EZ_ALWAYS_INLINE void Init(ezPxCharacterShapeComponent* pObject)
  {
    m_Type = CharacterShapeComponent;
    m_pObject = pObject;
  }

  EZ_ALWAYS_INLINE void Init(ezPxShapeComponent* pObject)
  {
    m_Type = ShapeComponent;
    m_pObject = pObject;
  }

  EZ_ALWAYS_INLINE void Init(ezBreakableSheetComponent* pObject)
  {
    m_Type = BreakableSheetComponent;
    m_pObject = pObject;
  }

  EZ_ALWAYS_INLINE ezPxUserData(ezSurfaceResource* pObject)
    : m_Type(SurfaceResource)
    , m_pObject(pObject)
  {
  }

  EZ_FORCE_INLINE void Invalidate()
  {
    m_Type = Invalid;
    m_pObject = nullptr;
    m_pAdditionalUserData = nullptr;
  }

  EZ_FORCE_INLINE static ezPxDynamicActorComponent* GetDynamicActorComponent(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr && pPxUserData->m_Type == DynamicActorComponent)
    {
      return static_cast<ezPxDynamicActorComponent*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static ezPxStaticActorComponent* GetStaticActorComponent(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr && pPxUserData->m_Type == StaticActorComponent)
    {
      return static_cast<ezPxStaticActorComponent*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static ezPxTriggerComponent* GetTriggerComponent(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr && pPxUserData->m_Type == TriggerComponent)
    {
      return static_cast<ezPxTriggerComponent*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static ezPxCharacterShapeComponent* GetCharacterShapeComponent(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr && pPxUserData->m_Type == CharacterShapeComponent)
    {
      return static_cast<ezPxCharacterShapeComponent*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static ezPxShapeComponent* GetShapeComponent(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr && pPxUserData->m_Type == ShapeComponent)
    {
      return static_cast<ezPxShapeComponent*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static ezBreakableSheetComponent* GetBreakableSheetComponent(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr && pPxUserData->m_Type == BreakableSheetComponent)
    {
      return static_cast<ezBreakableSheetComponent*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static ezComponent* GetComponent(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr &&
        (pPxUserData->m_Type == DynamicActorComponent || pPxUserData->m_Type == StaticActorComponent || pPxUserData->m_Type == TriggerComponent ||
          pPxUserData->m_Type == CharacterShapeComponent || pPxUserData->m_Type == ShapeComponent || pPxUserData->m_Type == BreakableSheetComponent))
    {
      return static_cast<ezComponent*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static ezSurfaceResource* GetSurfaceResource(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    if (pPxUserData != nullptr && pPxUserData->m_Type == SurfaceResource)
    {
      return static_cast<ezSurfaceResource*>(pPxUserData->m_pObject);
    }

    return nullptr;
  }

  EZ_FORCE_INLINE static void* GetAdditionalUserData(void* pUserData)
  {
    ezPxUserData* pPxUserData = static_cast<ezPxUserData*>(pUserData);
    return pPxUserData->m_pAdditionalUserData;
  }

  EZ_FORCE_INLINE void SetAdditionalUserData(void* pAdditionalUserData) { m_pAdditionalUserData = pAdditionalUserData; }


private:
  enum Type
  {
    Invalid,
    DynamicActorComponent,
    StaticActorComponent,
    TriggerComponent,
    CharacterShapeComponent,
    ShapeComponent,
    BreakableSheetComponent,
    SurfaceResource
  };

  Type m_Type = Invalid;
  void* m_pObject = nullptr;
  void* m_pAdditionalUserData = nullptr;
};
