#include <EditorPluginAssetsPCH.h>

#include <EditorFramework/Assets/AssetCurator.h>
#include <EditorFramework/EditorApp/EditorApp.moc.h>
#include <EditorPluginAssets/AnimationControllerAsset/AnimationControllerAsset.h>
#include <EditorPluginAssets/AnimationControllerAsset/AnimationControllerAssetManager.h>
#include <Foundation/Serialization/AbstractObjectGraph.h>
#include <Foundation/Serialization/BinarySerializer.h>
#include <Foundation/Serialization/RttiConverter.h>
#include <GuiFoundation/NodeEditor/NodeScene.moc.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationController.h>
#include <RendererCore/AnimationSystem/AnimationController/AnimationControllerNode.h>
#include <RendererCore/Pipeline/RenderPipelineNode.h>
#include <ToolsFoundation/Command/NodeCommands.h>
#include <ToolsFoundation/Serialization/DocumentObjectConverter.h>

EZ_BEGIN_DYNAMIC_REFLECTED_TYPE(ezAnimationControllerAssetDocument, 3, ezRTTINoAllocator)
EZ_END_DYNAMIC_REFLECTED_TYPE;

bool ezAnimationControllerNodeManager::InternalIsNode(const ezDocumentObject* pObject) const
{
  auto pType = pObject->GetTypeAccessor().GetType();
  return pType->IsDerivedFrom<ezAnimationControllerNode>();
}

void ezAnimationControllerNodeManager::InternalCreatePins(const ezDocumentObject* pObject, NodeInternal& node)
{
  // currently no connections in the graph

  // auto pType = pObject->GetTypeAccessor().GetType();
  // if (!pType->IsDerivedFrom<ezAnimationGraphNode>())
  //  return;

  // ezHybridArray<ezAbstractProperty*, 32> properties;
  // pType->GetAllProperties(properties);

  // for (ezAbstractProperty* pProp : properties)
  //{
  //  if (pProp->GetCategory() != ezPropertyCategory::Member)
  //    continue;

  //  if (!pProp->GetSpecificType()->IsDerivedFrom<ezRenderPipelineNodePin>())
  //    continue;

  //  ezColor pinColor = ezColor::Grey;
  //  if (const ezColorAttribute* pAttr = pProp->GetAttributeByType<ezColorAttribute>())
  //  {
  //    pinColor = pAttr->GetColor();
  //  }

  //  if (pProp->GetSpecificType()->IsDerivedFrom<ezRenderPipelineNodeInputPin>())
  //  {
  //    ezPin* pPin = EZ_DEFAULT_NEW(ezPin, ezPin::Type::Input, pProp->GetPropertyName(), pinColor, pObject);
  //    node.m_Inputs.PushBack(pPin);
  //  }
  //  else if (pProp->GetSpecificType()->IsDerivedFrom<ezRenderPipelineNodeOutputPin>())
  //  {
  //    ezPin* pPin = EZ_DEFAULT_NEW(ezPin, ezPin::Type::Output, pProp->GetPropertyName(), pinColor, pObject);
  //    node.m_Outputs.PushBack(pPin);
  //  }
  //  else if (pProp->GetSpecificType()->IsDerivedFrom<ezRenderPipelineNodePassThrougPin>())
  //  {
  //    ezPin* pPinIn = EZ_DEFAULT_NEW(ezPin, ezPin::Type::Input, pProp->GetPropertyName(), pinColor, pObject);
  //    node.m_Inputs.PushBack(pPinIn);
  //    ezPin* pPinOut = EZ_DEFAULT_NEW(ezPin, ezPin::Type::Output, pProp->GetPropertyName(), pinColor, pObject);
  //    node.m_Outputs.PushBack(pPinOut);
  //  }
  //}
}

void ezAnimationControllerNodeManager::InternalDestroyPins(const ezDocumentObject* pObject, NodeInternal& node)
{
  // for (ezPin* pPin : node.m_Inputs)
  //{
  //  EZ_DEFAULT_DELETE(pPin);
  //}
  // node.m_Inputs.Clear();
  // for (ezPin* pPin : node.m_Outputs)
  //{
  //  EZ_DEFAULT_DELETE(pPin);
  //}
  // node.m_Outputs.Clear();
}


void ezAnimationControllerNodeManager::GetCreateableTypes(ezHybridArray<const ezRTTI*, 32>& Types) const
{
  ezSet<const ezRTTI*> typeSet;
  ezReflectionUtils::GatherTypesDerivedFromClass(ezGetStaticRTTI<ezAnimationControllerNode>(), typeSet, false);

  Types.Clear();
  for (auto pType : typeSet)
  {
    if (pType->GetTypeFlags().IsAnySet(ezTypeFlags::Abstract))
      continue;

    Types.PushBack(pType);
  }
}

ezStatus ezAnimationControllerNodeManager::InternalCanConnect(const ezPin* pSource, const ezPin* pTarget, CanConnectResult& out_Result) const
{
  out_Result = CanConnectResult::ConnectNever;

  // out_Result = CanConnectResult::ConnectNto1;

  // if (!pTarget->GetConnections().IsEmpty())
  //  return ezStatus("Only one connection can be made to in input pin!");

  return ezStatus(EZ_SUCCESS);
}

ezAnimationControllerAssetDocument::ezAnimationControllerAssetDocument(const char* szDocumentPath)
  : ezAssetDocument(szDocumentPath, EZ_DEFAULT_NEW(ezAnimationControllerNodeManager), ezAssetDocEngineConnection::None)
{
}

ezStatus ezAnimationControllerAssetDocument::InternalTransformAsset(ezStreamWriter& stream, const char* szOutputTag, const ezPlatformProfile* pAssetProfile, const ezAssetFileHeader& AssetHeader, ezBitflags<ezTransformFlags> transformFlags)
{
  ezAbstractObjectGraph graph;

  // serialize all the document objects to the abstract graph
  {
    ezDocumentObjectConverterWriter writer(&graph, GetObjectManager());

    auto& children = GetObjectManager()->GetRootObject()->GetChildren();
    for (ezDocumentObject* pObject : children)
    {
      auto pType = pObject->GetTypeAccessor().GetType();
      if (pType->IsDerivedFrom<ezAnimationControllerNode>())
      {
        writer.AddObjectToGraph(pObject, "AnimationControllerNode");
      }
    }
  }

  ezAnimationController animController;

  // create the actual types in the animController
  {
    ezRttiConverterContext context;
    ezRttiConverterReader reader(&graph, &context);

    for (auto itNode : graph.GetAllNodes())
    {
      ezAnimationControllerNode* pNode = (ezAnimationControllerNode*)reader.CreateObjectFromNode(itNode.Value());
      ezUniquePtr<ezAnimationControllerNode> pUniqueNode;
      ezInternal::NewInstance newInstance(pNode, ezFoundation::GetDefaultAllocator());

      animController.m_Nodes.PushBack(newInstance);
    }
  }

  ezMemoryStreamStorage storage;
  ezMemoryStreamWriter writer(&storage);
  EZ_SUCCEED_OR_RETURN(animController.Serialize(writer));

  stream << storage.GetStorageSize();
  return stream.WriteBytes(storage.GetData(), storage.GetStorageSize());
}

void ezAnimationControllerAssetDocument::InternalGetMetaDataHash(const ezDocumentObject* pObject, ezUInt64& inout_uiHash) const
{
  // const ezDocumentNodeManager* pManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());
  // if (pManager->IsNode(pObject))
  //{
  //  auto outputs = pManager->GetOutputPins(pObject);
  //  for (const ezPin* pPinSource : outputs)
  //  {
  //    auto inputs = pPinSource->GetConnections();
  //    for (const ezConnection* pConnection : inputs)
  //    {
  //      const ezPin* pPinTarget = pConnection->GetTargetPin();

  //      inout_uiHash = ezHashingUtils::xxHash64(&pPinSource->GetParent()->GetGuid(), sizeof(ezUuid), inout_uiHash);
  //      inout_uiHash = ezHashingUtils::xxHash64(&pPinTarget->GetParent()->GetGuid(), sizeof(ezUuid), inout_uiHash);
  //      inout_uiHash = ezHashingUtils::xxHash64(pPinSource->GetName(), ezStringUtils::GetStringElementCount(pPinSource->GetName()), inout_uiHash);
  //      inout_uiHash = ezHashingUtils::xxHash64(pPinTarget->GetName(), ezStringUtils::GetStringElementCount(pPinTarget->GetName()), inout_uiHash);
  //    }
  //  }
  //}
}

void ezAnimationControllerAssetDocument::AttachMetaDataBeforeSaving(ezAbstractObjectGraph& graph) const
{
  const ezDocumentNodeManager* pManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());
  pManager->AttachMetaDataBeforeSaving(graph);
}

void ezAnimationControllerAssetDocument::RestoreMetaDataAfterLoading(const ezAbstractObjectGraph& graph, bool bUndoable)
{
  ezDocumentNodeManager* pManager = static_cast<ezDocumentNodeManager*>(GetObjectManager());
  pManager->RestoreMetaDataAfterLoading(graph, bUndoable);
}



void ezAnimationControllerAssetDocument::GetSupportedMimeTypesForPasting(ezHybridArray<ezString, 4>& out_MimeTypes) const
{
  out_MimeTypes.PushBack("application/ezEditor.AnimationControllerGraph");
}

bool ezAnimationControllerAssetDocument::CopySelectedObjects(ezAbstractObjectGraph& out_objectGraph, ezStringBuilder& out_MimeType) const
{
  out_MimeType = "application/ezEditor.AnimationControllerGraph";

  const auto& selection = GetSelectionManager()->GetSelection();

  if (selection.IsEmpty())
    return false;

  const ezDocumentNodeManager* pManager = static_cast<const ezDocumentNodeManager*>(GetObjectManager());

  ezDocumentObjectConverterWriter writer(&out_objectGraph, pManager);

  for (const ezDocumentObject* pNode : selection)
  {
    // objects are required to be named root but this is not enforced or obvious by the interface.
    writer.AddObjectToGraph(pNode, "root");
  }

  pManager->AttachMetaDataBeforeSaving(out_objectGraph);

  return true;
}

bool ezAnimationControllerAssetDocument::Paste(const ezArrayPtr<PasteInfo>& info, const ezAbstractObjectGraph& objectGraph, bool bAllowPickedPosition, const char* szMimeType)
{
  bool bAddedAll = true;

  ezDeque<const ezDocumentObject*> AddedNodes;

  for (const PasteInfo& pi : info)
  {
    // only add nodes that are allowed to be added
    if (GetObjectManager()->CanAdd(pi.m_pObject->GetTypeAccessor().GetType(), nullptr, "Children", pi.m_Index).m_Result.Succeeded())
    {
      AddedNodes.PushBack(pi.m_pObject);
      GetObjectManager()->AddObject(pi.m_pObject, nullptr, "Children", pi.m_Index);
    }
    else
    {
      bAddedAll = false;
    }
  }

  m_DocumentObjectMetaData.RestoreMetaDataFromAbstractGraph(objectGraph);

  RestoreMetaDataAfterLoading(objectGraph, true);

  if (!AddedNodes.IsEmpty() && bAllowPickedPosition)
  {
    ezDocumentNodeManager* pManager = static_cast<ezDocumentNodeManager*>(GetObjectManager());

    ezVec2 vAvgPos(0);
    for (const ezDocumentObject* pNode : AddedNodes)
    {
      vAvgPos += pManager->GetNodePos(pNode);
    }

    vAvgPos /= AddedNodes.GetCount();

    const ezVec2 vMoveNode = -vAvgPos + ezQtNodeScene::GetLastMouseInteractionPos();

    for (const ezDocumentObject* pNode : AddedNodes)
    {
      ezMoveNodeCommand move;
      move.m_Object = pNode->GetGuid();
      move.m_NewPos = pManager->GetNodePos(pNode) + vMoveNode;
      GetCommandHistory()->AddCommand(move);
    }

    if (!bAddedAll)
    {
      ezLog::Info("[EditorStatus]Not all nodes were allowed to be added to the document");
    }
  }

  GetSelectionManager()->SetSelection(AddedNodes);
  return true;
}
