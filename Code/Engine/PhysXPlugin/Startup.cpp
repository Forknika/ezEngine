#include <PhysXPlugin/PCH.h>
#include <Foundation/Configuration/Startup.h>
#include <PhysXPlugin/PhysXWorldModule.h>
#include <PhysXPlugin/Resources/PxMeshResource.h>

EZ_BEGIN_SUBSYSTEM_DECLARATION(PhysX, PhysXPlugin)

  BEGIN_SUBSYSTEM_DEPENDENCIES
    "Foundation",
    "Core"
  END_SUBSYSTEM_DEPENDENCIES

  ON_CORE_STARTUP
  {
    ezPhysX::GetSingleton()->Startup();

    ezResourceManager::RegisterResourceForAssetType("Collision Mesh", ezGetStaticRTTI<ezPxMeshResource>());
    ezResourceManager::RegisterResourceForAssetType("Collision Mesh (Convex)", ezGetStaticRTTI<ezPxMeshResource>());
  }

  ON_CORE_SHUTDOWN
  {
    ezPhysX::GetSingleton()->Shutdown();
  }

  ON_ENGINE_STARTUP
  {
  }

  ON_ENGINE_SHUTDOWN
  {
  }

EZ_END_SUBSYSTEM_DECLARATION

void ezPxErrorCallback::reportError(PxErrorCode::Enum code, const char* message, const char* file, int line)
{
  OutputDebugStringA(message);
  return;

  /// \todo This can happen on a thread that was created by PhysX, thus our thread-local system is not initialized and ezLog will crash!

  switch (code)
  {
  case PxErrorCode::eABORT:
    ezLog::ErrorPrintf("PhysX: %s", message);
    break;
  case PxErrorCode::eDEBUG_INFO:
    ezLog::DevPrintf("PhysX: %s", message);
    break;
  case PxErrorCode::eDEBUG_WARNING:
    ezLog::WarningPrintf("PhysX: %s", message);
    break;
  case PxErrorCode::eINTERNAL_ERROR:
    ezLog::ErrorPrintf("PhysX Internal: %s", message);
    break;
  case PxErrorCode::eINVALID_OPERATION:
    ezLog::ErrorPrintf("PhysX Invalid Operation: %s", message);
    break;
  case PxErrorCode::eINVALID_PARAMETER:
    ezLog::ErrorPrintf("PhysX Invalid Parameter: %s", message);
    break;
  case PxErrorCode::eOUT_OF_MEMORY:
    ezLog::ErrorPrintf("PhysX Out-of-Memory: %s", message);
    break;
  case PxErrorCode::ePERF_WARNING:
    ezLog::WarningPrintf("PhysX Performance: %s", message);
    break;

  default:
    ezLog::ErrorPrintf("PhysX: Unknown error type '%i': %s", code, message);
    break;
  }
}

ezPxAllocatorCallback::ezPxAllocatorCallback()
  : m_Allocator("PhysX", ezFoundation::GetDefaultAllocator())
{

}

void* ezPxAllocatorCallback::allocate(size_t size, const char* typeName, const char* filename, int line)
{
  //return new unsigned char[size];
  void* pPtr = m_Allocator.Allocate(size, 16);

#if EZ_ENABLED(EZ_COMPILE_FOR_DEBUG)
  ezStringBuilder s;
  s.Set(typeName, " - ", filename);
  m_Allocations[pPtr] = s;
#endif

  return pPtr;
}

void ezPxAllocatorCallback::deallocate(void* ptr)
{
  // apparently this happens
  if (ptr == nullptr)
    return;

#if EZ_ENABLED(EZ_COMPILE_FOR_DEBUG)
  m_Allocations.Remove(ptr);
#endif

  m_Allocator.Deallocate(ptr);
}
