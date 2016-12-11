#include <Core/PCH.h>
#include <Core/Application/Config/FileSystemConfig.h>
#include <Foundation/IO/OpenDdlWriter.h>
#include <Foundation/IO/OpenDdlUtils.h>
#include <Foundation/IO/OpenDdlReader.h>

EZ_BEGIN_STATIC_REFLECTED_TYPE(ezApplicationFileSystemConfig, ezNoBase, 1, ezRTTIDefaultAllocator<ezApplicationFileSystemConfig>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_ARRAY_MEMBER_PROPERTY("DataDirs", m_DataDirs),
  }
  EZ_END_PROPERTIES
}
EZ_END_STATIC_REFLECTED_TYPE

EZ_BEGIN_STATIC_REFLECTED_TYPE(ezApplicationFileSystemConfig_DataDirConfig, ezNoBase, 1, ezRTTIDefaultAllocator<ezApplicationFileSystemConfig_DataDirConfig>)
{
  EZ_BEGIN_PROPERTIES
  {
    EZ_MEMBER_PROPERTY("RelativePath", m_sSdkRootRelativePath),
    EZ_MEMBER_PROPERTY("Writable", m_bWritable),
    EZ_MEMBER_PROPERTY("RootName", m_sRootName),
  }
  EZ_END_PROPERTIES
}
EZ_END_STATIC_REFLECTED_TYPE


ezResult ezApplicationFileSystemConfig::Save()
{
  ezStringBuilder sPath;
  sPath = ezApplicationConfig::GetProjectDirectory();
  sPath.AppendPath("DataDirectories.ddl");

  ezFileWriter file;
  if (file.Open(sPath).Failed())
    return EZ_FAILURE;

  ezOpenDdlWriter writer;
  writer.SetOutputStream(&file);
  writer.SetCompactMode(false);
  writer.SetPrimitiveTypeStringMode(ezOpenDdlWriter::TypeStringMode::Compliant);

  for (ezUInt32 i = 0; i < m_DataDirs.GetCount(); ++i)
  {
    writer.BeginObject("DataDir");

    ezOpenDdlUtils::StoreString(writer, m_DataDirs[i].m_sSdkRootRelativePath, "Path");
    ezOpenDdlUtils::StoreString(writer, m_DataDirs[i].m_sRootName, "RootName");
    ezOpenDdlUtils::StoreBool(writer, m_DataDirs[i].m_bWritable, "Writable");

    writer.EndObject();
  }

  return EZ_SUCCESS;
}

void ezApplicationFileSystemConfig::Load()
{
  EZ_LOG_BLOCK("ezApplicationFileSystemConfig::Load()");

  m_DataDirs.Clear();

  ezStringBuilder sPath;
  sPath = ezApplicationConfig::GetProjectDirectory();
  sPath.AppendPath("DataDirectories.ddl");

  ezFileReader file;
  if (file.Open(sPath).Failed())
  {
    ezLog::WarningPrintf("Could not open file-system config file '%s'", sPath.GetData());
    return;
  }

  ezOpenDdlReader reader;
  if (reader.ParseDocument(file, 0, ezGlobalLog::GetOrCreateInstance()).Failed())
  {
    ezLog::ErrorPrintf("Failed to parse file-system config file '%s'", sPath.GetData());
    return;
  }

  const ezOpenDdlReaderElement* pTree = reader.GetRootElement();

  for (const ezOpenDdlReaderElement* pDirs = pTree->GetFirstChild(); pDirs != nullptr; pDirs = pDirs->GetSibling())
  {
    if (!pDirs->IsCustomType("DataDir"))
      continue;

    DataDirConfig cfg;
    cfg.m_bWritable = false;

    const ezOpenDdlReaderElement* pPath = pDirs->FindChildOfType(ezOpenDdlPrimitiveType::String, "Path");
    const ezOpenDdlReaderElement* pRoot = pDirs->FindChildOfType(ezOpenDdlPrimitiveType::String, "RootName");
    const ezOpenDdlReaderElement* pWrite = pDirs->FindChildOfType(ezOpenDdlPrimitiveType::Bool, "Writable");

    if (pPath)
      cfg.m_sSdkRootRelativePath = pPath->GetPrimitivesString()[0];
    if (pRoot)
      cfg.m_sRootName = pRoot->GetPrimitivesString()[0];
    if (pWrite)
      cfg.m_bWritable = pWrite->GetPrimitivesBool()[0];

    m_DataDirs.PushBack(cfg);
  }
}

void ezApplicationFileSystemConfig::Apply()
{
  EZ_LOG_BLOCK("ezApplicationFileSystemConfig::Apply");

  ezStringBuilder s;

  // Make sure previous calls to Apply do not accumulate
  Clear();

  for (const auto& var : m_DataDirs)
  {
    s = ezApplicationConfig::GetSdkRootDirectory();
    s.AppendPath(var.m_sSdkRootRelativePath);
    s.MakeCleanPath();

    ezFileSystem::AddDataDirectory(s, "AppFileSystemConfig", var.m_sRootName, (!var.m_sRootName.IsEmpty() && var.m_bWritable) ? ezFileSystem::DataDirUsage::AllowWrites : ezFileSystem::DataDirUsage::ReadOnly);
  }
}


void ezApplicationFileSystemConfig::Clear()
{
  ezFileSystem::RemoveDataDirectoryGroup("AppFileSystemConfig");
}

ezResult ezApplicationFileSystemConfig::CreateDataDirStubFiles()
{
  EZ_LOG_BLOCK("ezApplicationFileSystemConfig::CreateDataDirStubFiles");

  ezStringBuilder s;
  ezResult res = EZ_SUCCESS;

  for (const auto& var : m_DataDirs)
  {
    s = ezApplicationConfig::GetSdkRootDirectory();
    s.AppendPath(var.m_sSdkRootRelativePath);
    s.AppendPath("DataDir.ezManifest");
    s.MakeCleanPath();

    ezOSFile file;
    if (file.Open(s, ezFileMode::Write).Failed())
    {
      ezLog::ErrorPrintf("Failed to create stub file '%s'", s.GetData());
      res = EZ_FAILURE;
    }
  }

  return EZ_SUCCESS;
}



EZ_STATICLINK_FILE(Core, Core_Application_Config_Implementation_FileSystemConfig);

