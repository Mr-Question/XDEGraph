#include <Draw_Main.hxx>
#include <DDocStd.hxx>
#include <TDocStd_Document.hxx>
#include <DDocStd_DrawDocument.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDF_Tool.hxx>
#include <TDF_AttributeIterator.hxx>
#include <NCollection_IndexedMap.hxx>
#include <NCollection_DataMap.hxx>
#include <TDataStd_Name.hxx>

#include <functional>
#include <stack>
#include <list>

//=======================================================================
//function : XGraph
//purpose  :
//=======================================================================
static Standard_Integer XGraph (Draw_Interpretor&      theDI,
                                const Standard_Integer theArgc,
                                const char**           theArgv)
{
  if ( theArgc < 3 )
  {
    theDI << "Usage:\t XGraph DocName FileName\n";
    return 1;
  }

  Standard_CString              aDocName  = theArgv[1];
  const TCollection_AsciiString aFileName = theArgv[2];

  Handle(TDocStd_Document) aD;
  if (!DDocStd::GetDocument (aDocName, aD, Standard_False))
  {
    theDI << "Error: there is no document " << aDocName << "\n";
    return 1;
  }

  Handle(XCAFDoc_ShapeTool) aShapeTool = XCAFDoc_DocumentTool::ShapeTool (aD->Main());

  TDF_LabelSequence aLabels;
  aShapeTool->GetFreeShapes (aLabels);

  Standard_SStream aStream;

  aStream << "ISO-10303-21;" << std::endl;
  aStream << "HEADER;" << std::endl;
  aStream << "FILE_DESCRIPTION(('Open CASCADE Model'),'2;1');" << std::endl;
  aStream << "ENDSEC;" << std::endl;
  aStream << "DATA;" << std::endl;

  NCollection_IndexedDataMap<TCollection_AsciiString, std::pair<TDF_Label, std::list<Standard_Integer>>> aLabelMap;
  NCollection_DataMap<Handle(TDF_Attribute), Standard_Integer> aAttMap;

  auto AddEntry = [&](
    const Standard_Integer  theId,
    const Standard_CString& theTag,
    const Standard_CString& theName,
    std::function<void()>   theParamFunc)
  {
    aStream << "#" << theId << " = " << theTag << "(\'" << theName << "\'";

    if (theParamFunc)
    {
      aStream << ", ";
      theParamFunc();
    }

    aStream << ");" << std::endl;
  };

  auto AddLabelToMap = [&](TDF_Label& theLabel, const Standard_Integer theParentId, TCollection_AsciiString& theEntry) -> bool
  {
    TDF_Tool::Entry (theLabel, theEntry);

    if (auto aItem = aLabelMap.ChangeSeek (theEntry))
    {
      aItem->second.push_back (theParentId);
      return false;
    }

    return aLabelMap.Add (theEntry, std::make_pair(theLabel, std::list{theParentId}));
    return true;
  };

  std::stack<std::pair<TDF_Label, Standard_Integer>> aStack;

  for (TDF_LabelSequence::Iterator aIt (aLabels); aIt.More(); aIt.Next())
  {
    aStack.push (std::make_pair (aIt.Value(), 0));
  }

  while (!aStack.empty())
  {
    std::pair<TDF_Label, Standard_Integer> aLabel = aStack.top();
    aStack.pop();

    TCollection_AsciiString aEntry;
    if (!AddLabelToMap (aLabel.first, aLabel.second, aEntry))
    {
      continue;
    }

    const Standard_Integer aParentId = aLabelMap.FindIndex (aEntry);

    TDF_Label aRefLabel;
    if (aShapeTool->GetReferredShape (aLabel.first, aRefLabel))
    {
      aStack.push (std::make_pair (aRefLabel, aParentId));
      continue;
    }

    TDF_LabelSequence aComps;
    if (aShapeTool->GetComponents (aLabel.first, aComps))
    {
      for (TDF_LabelSequence::Iterator aCompIt (aComps); aCompIt.More(); aCompIt.Next())
      {
        aStack.push (std::make_pair (aCompIt.Value(), aParentId));
      }
    }
  }

  for (Standard_Integer aId = 1; aId <= aLabelMap.Size(); ++aId)
  {
    const std::pair<TDF_Label, std::list<Standard_Integer>>& aLabel = aLabelMap (aId);

    const TCollection_AsciiString& aEntry = aLabelMap.FindKey (aId);

    TCollection_AsciiString aTag;
    if (aShapeTool->IsReference (aLabel.first))
    {
      aTag = "REFERENCE";
    }
    else if (aShapeTool->IsAssembly (aLabel.first))
    {
      aTag = "ASSEMBLY";
    }
    else if (aShapeTool->IsComponent (aLabel.first))
    {
      aTag = "COMPONENT";
    }
    else if (aShapeTool->IsExternRef (aLabel.first))
    {
      aTag = "EXTERN_REF";
    }
    else if (aShapeTool->IsSimpleShape (aLabel.first))
    {
      aTag = "SHAPE";
    }
//    else if (aShapeTool->IsShape (aLabel.first))
//    {
//      aTag = "SHAPE";
//      if (aShapeTool->IsCompound (aLabel.first))
//      {
//        aTag = "COMPOUND";
//      }
//    }
//    else if (aShapeTool->IsSubShape (aLabel.first))
//    {
//      aTag = "SUBSHAPE";
//    }

    AddEntry (aId, aTag.ToCString(), aEntry.ToCString(), [&](){
      // PARENT
      if (!aLabel.second.empty())
      {
        Standard_Integer aParentCount = 0;
        for (auto aParentId : aLabel.second)
        {
          if (aParentId > 0)
          {
            if (aParentCount == 0)
            {
              aStream << "(";
            }
            else
            {
              aStream << ", ";
            }

            aStream << "#" << aParentId;
            ++aParentCount;
          }
        }

        if (aParentCount > 0)
        {
          aStream << "), ";
        }
      }

      // ATTS
      aStream << "(";

      Standard_Integer aAttCount = 1;
      for (TDF_AttributeIterator aAttIt (aLabel.first); aAttIt.More(); aAttIt.Next(), ++aAttCount)
      {
        Handle(TDF_Attribute) aAtt = aAttIt.Value();

        Standard_Integer aAttId = aLabelMap.Size() + aAttMap.Size() + 1;
        if (!aAttMap.IsBound (aAtt))
        {
          aAttMap.Bind (aAtt, aAttId);
        }
        else
        {
          aAttId = aAttMap.Find (aAtt);
        }

        if (aAttCount > 1)
        {
          aStream << ", ";
        }
        aStream << "#" << aAttId;
      }
      aStream << ")";
    });
  }

  for (NCollection_DataMap<Handle(TDF_Attribute), Standard_Integer>::Iterator aAttIt (aAttMap); aAttIt.More(); aAttIt.Next())
  {
    const Handle(TDF_Attribute)& aAtt = aAttIt.Key();

    TCollection_AsciiString aValue;
    if (auto aAttObj = Handle(TDataStd_Name)::DownCast(aAtt))
    {
      aValue = TCollection_AsciiString (aAttObj->Get());
    }

    AddEntry (aAttIt.Value(), "ATTRIBUTE", aValue.ToCString(), [&](){
      aStream << "\'";
      aStream << aAtt->DynamicType()->Name();
      aStream << "\'";

      if (aValue.IsEmpty())
      {
        aStream << ", \'";
        Standard_SStream aAttStream;
        aAtt->DumpJson (aAttStream, 5);

        aStream << aAttStream.rdbuf();
        aStream << "\'";
      }
    });
  }

  aStream << "ENDSEC;" << std::endl;
  aStream << "END-ISO-10303-21;" << std::endl;

  std::ofstream aFile;
  aFile.open (aFileName.ToCString());
  if (!aFile.is_open())
  {
    theDI << "Error: cannot open output file\n";
    return 1;
  }

  aFile << aStream.rdbuf();
  aFile.close();

  return 0;
}

//=======================================================================
//function : Draw_InitAppli
//purpose  :
//=======================================================================
void Draw_InitAppli (Draw_Interpretor& theDI)
{
  Draw::Commands (theDI);

  theDI.Eval ("pload XDE");

  const Standard_CString aGroupName = "XDE pew commands";

  theDI.Add ("XGraph","DocName FileName \t: Dump XDE document graph strucutre to the file",
    __FILE__, XGraph, aGroupName);
}

DRAW_MAIN
