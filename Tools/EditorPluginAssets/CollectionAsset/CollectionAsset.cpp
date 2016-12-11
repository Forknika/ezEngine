#include <PCH.h>
#include <EditorPluginAssets/CollectionAsset/CollectionAsset.h>
#include <EditorPluginAssets/CollectionAsset/CollectionAssetManager.h>
#include <ToolsFoundation/Reflection/PhantomRttiManager.h>
#include <EditorFramework/Assets/AssetCurator.h>
#include <Foundation/IO/FileSystem/FileWriter.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <CoreUtils/Image/Image.h>
#include <Core/WorldSerializer/ResourceHandleWriter.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezCollectionAssetEntry, 1, ezRTTIDefaultAllocator<ezCollectionAssetEntry>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("Name", m_sLookupName),
    EZ_MEMBER_PROPERTY("Asset", m_sRedirectionAsset)->AddAttributes(new ezAssetBrowserAttribute(""))
  }
    EZ_END_PROPERTIES
}
EZ_END_DYNAMIC_REFLECTED_TYPE

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezCollectionAssetData, 1, ezRTTIDefaultAllocator<ezCollectionAssetData>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ARRAY_MEMBER_PROPERTY("Entries", m_Entries),
  }
  EZ_END_PROPERTIES
}
EZ_END_DYNAMIC_REFLECTED_TYPE

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezCollectionAssetDocument, 1, ezRTTINoAllocator);
EZ_END_DYNAMIC_REFLECTED_TYPE

ezCollectionAssetDocument::ezCollectionAssetDocument(const char* szDocumentPath) : ezSimpleAssetDocument<ezCollectionAssetData>(szDocumentPath)
{
}

ezStatus ezCollectionAssetDocument::InternalTransformAsset(ezStreamWriter& stream, const char* szPlatform, const ezAssetFileHeader& AssetHeader)
{
  const ezCollectionAssetData* pProp = GetProperties();

  ezCollectionResourceDescriptor desc;
  ezCollectionEntry entry;

  for (const auto& e : pProp->m_Entries)
  {
    if (e.m_sRedirectionAsset.IsEmpty())
      continue;

    ezAssetCurator::ezLockedAssetInfo pInfo = ezAssetCurator::GetSingleton()->FindAssetInfo(e.m_sRedirectionAsset);

    if (pInfo == nullptr)
    {
      ezLog::WarningPrintf("Asset in Collection is unknown: '%s'", e.m_sRedirectionAsset.GetData());
      continue;
    }

    entry.m_sLookupName = e.m_sLookupName;
    entry.m_sRedirectionName = e.m_sRedirectionAsset;
    entry.m_sResourceTypeName = pInfo->m_Info.m_sAssetTypeName;

    desc.m_Resources.PushBack(entry);
  }

  desc.Save(stream);

  return ezStatus(EZ_SUCCESS);
}

