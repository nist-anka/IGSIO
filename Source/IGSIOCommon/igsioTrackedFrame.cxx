/*=igsio=header=begin======================================================
Program: igsio
Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
See License.txt for details.
=========================================================igsio=header=end*/

//#include "igsioConfigure.h"
//#include "igsioMath.h"
#include "igsioTrackedFrame.h"
#include "vtkImageData.h"
#include "vtkMatrix4x4.h"
#include "vtkPoints.h"
#include "vtkXMLUtilities.h"
#include <vtkSmartPointer.h>
#include <vtkXMLDataElement.h>

//----------------------------------------------------------------------------
// ************************* TrackedFrame ************************************
//----------------------------------------------------------------------------

const char* igsioTrackedFrame::FIELD_FRIENDLY_DEVICE_NAME = "FriendlyDeviceName";
const std::string igsioTrackedFrame::TransformPostfix = "Transform";
const std::string igsioTrackedFrame::TransformStatusPostfix = "TransformStatus";
const int FLOATING_POINT_PRECISION = 16; // Number of digits used when writing transforms and timestamps

//----------------------------------------------------------------------------
igsioTrackedFrame::igsioTrackedFrame()
{
  this->Timestamp = 0;
  this->FrameSize[0] = 0;
  this->FrameSize[1] = 0;
  this->FrameSize[2] = 1; // single-slice frame by default
  this->FiducialPointsCoordinatePx = NULL;
}

//----------------------------------------------------------------------------
igsioTrackedFrame::~igsioTrackedFrame()
{
  this->SetFiducialPointsCoordinatePx(NULL);
}

//----------------------------------------------------------------------------
igsioTrackedFrame::igsioTrackedFrame(const igsioTrackedFrame& frame)
{
  this->Timestamp = 0;
  this->FrameSize[0] = 0;
  this->FrameSize[1] = 0;
  this->FrameSize[2] = 1; // single-slice frame by default
  this->FiducialPointsCoordinatePx = NULL;

  *this = frame;
}

//----------------------------------------------------------------------------
igsioTrackedFrame& igsioTrackedFrame::operator=(igsioTrackedFrame const& trackedFrame)
{
  // Handle self-assignment
  if (this == &trackedFrame)
  {
    return *this;
  }

  this->CustomFrameFields = trackedFrame.CustomFrameFields;
  this->ImageData = trackedFrame.ImageData;
  this->Timestamp = trackedFrame.Timestamp;
  this->FrameSize[0] = trackedFrame.FrameSize[0];
  this->FrameSize[1] = trackedFrame.FrameSize[1];
  this->FrameSize[2] = trackedFrame.FrameSize[2];
  this->SetFiducialPointsCoordinatePx(trackedFrame.FiducialPointsCoordinatePx);

  return *this;
}

//----------------------------------------------------------------------------
//igsioStatus igsioTrackedFrame::GetTrackedFrameInXmlData(std::string& strXmlData, const std::vector<igsioTransformName>& requestedTransforms)
//{
//  vtkSmartPointer<vtkXMLDataElement> xmlData = vtkSmartPointer<vtkXMLDataElement>::New();
//  igsioStatus status = this->PrintToXML(xmlData, requestedTransforms);
//
//  std::ostringstream os;
//  igsioCommon::XML::PrintXML(os, vtkIndent(0), xmlData);
//
//  strXmlData = os.str();
//
//  return status;
//}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::PrintToXML(vtkXMLDataElement* trackedFrame, const std::vector<igsioTransformName>& requestedTransforms)
{
  if (trackedFrame == NULL)
  {
    //LOG_ERROR("Unable to print tracked frame to XML - input XML data is NULL");
    return IGSIO_FAIL;
  }

  trackedFrame->SetName("TrackedFrame");
  trackedFrame->SetDoubleAttribute("Timestamp", this->Timestamp);
  trackedFrame->SetAttribute("ImageDataValid", (this->GetImageData()->IsImageValid() ? "true" : "false"));

  if (this->GetImageData()->IsImageValid())
  {
    trackedFrame->SetIntAttribute("NumberOfBits", this->GetNumberOfBitsPerScalar());
    unsigned int numberOfScalarComponents(1);
    if (this->GetNumberOfScalarComponents(numberOfScalarComponents) == IGSIO_FAIL)
    {
      //LOG_ERROR("Unable to retrieve number of scalar components.");
      return IGSIO_FAIL;
    }
    trackedFrame->SetIntAttribute("NumberOfScalarComponents", numberOfScalarComponents);
    if (FrameSize[0] > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        FrameSize[1] > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        FrameSize[2] > static_cast<unsigned int>(std::numeric_limits<int>::max()))
    {
      //LOG_ERROR("Unable to save frame size elements larger than: " << std::numeric_limits<int>::max());
      return IGSIO_FAIL;
    }
    int frameSizeSigned[3] = { static_cast<int>(FrameSize[0]), static_cast<int>(FrameSize[1]), static_cast<int>(FrameSize[2]) };
    trackedFrame->SetVectorAttribute("FrameSize", 3, frameSizeSigned);
  }

  for (auto fieldIter = CustomFrameFields.begin(); fieldIter != CustomFrameFields.end(); ++fieldIter)
  {
    // Only use requested transforms mechanism if the vector is not empty
    if (!requestedTransforms.empty() && (IsTransform(fieldIter->first) || IsTransformStatus(fieldIter->first)))
    {
      if (IsTransformStatus(fieldIter->first))
      {
        continue;
      }
      if (std::find(requestedTransforms.begin(), requestedTransforms.end(), igsioTransformName(fieldIter->first)) == requestedTransforms.end())
      {
        continue;
      }
      auto statusName = fieldIter->first;
      statusName = statusName.substr(0, fieldIter->first.length() - TransformPostfix.length());
      statusName = statusName.append(TransformStatusPostfix);
      vtkSmartPointer<vtkXMLDataElement> customField = vtkSmartPointer<vtkXMLDataElement>::New();
      customField->SetName("CustomFrameField");
      customField->SetAttribute("Name", statusName.c_str());
      customField->SetAttribute("Value", CustomFrameFields[statusName].c_str());
      trackedFrame->AddNestedElement(customField);
    }
    vtkSmartPointer<vtkXMLDataElement> customField = vtkSmartPointer<vtkXMLDataElement>::New();
    customField->SetName("CustomFrameField");
    customField->SetAttribute("Name", fieldIter->first.c_str());
    customField->SetAttribute("Value", fieldIter->second.c_str());
    trackedFrame->AddNestedElement(customField);
  }

  if (FiducialPointsCoordinatePx != NULL)
  {
    vtkSmartPointer<vtkXMLDataElement> segmentation = vtkSmartPointer<vtkXMLDataElement>::New();
    segmentation->SetName("Segmentation");

    if (FiducialPointsCoordinatePx->GetNumberOfPoints() == 0)
    {
      segmentation->SetAttribute("SegmentationStatus", "Failed");
    }
    else if (FiducialPointsCoordinatePx->GetNumberOfPoints() % 3 != 0)
    {
      segmentation->SetAttribute("SegmentationStatus", "InvalidPatterns");
    }
    else
    {
      segmentation->SetAttribute("SegmentationStatus", "OK");
    }

    vtkSmartPointer<vtkXMLDataElement> segmentedPoints = vtkSmartPointer<vtkXMLDataElement>::New();
    segmentedPoints->SetName("SegmentedPoints");

    for (int i = 0; i < FiducialPointsCoordinatePx->GetNumberOfPoints(); i++)
    {
      double point[3] = {0};
      FiducialPointsCoordinatePx->GetPoint(i, point);

      vtkSmartPointer<vtkXMLDataElement> pointElement = vtkSmartPointer<vtkXMLDataElement>::New();
      pointElement->SetName("Point");
      pointElement->SetIntAttribute("ID", i);
      pointElement->SetVectorAttribute("Position", 3, point);
      segmentedPoints->AddNestedElement(pointElement);
    }

    segmentation->AddNestedElement(segmentedPoints);
    trackedFrame->AddNestedElement(segmentation);
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
//igsioStatus igsioTrackedFrame::SetTrackedFrameFromXmlData(const std::string& xmlData)
//{
//  return this->SetTrackedFrameFromXmlData(xmlData.c_str());
//}

//----------------------------------------------------------------------------
//igsioStatus igsioTrackedFrame::SetTrackedFrameFromXmlData(const char* strXmlData)
//{
//  if (strXmlData == NULL)
//  {
//    //LOG_ERROR("Failed to set TrackedFrame from xml data - input xml data string is NULL!");
//    return IGSIO_FAIL;
//  }
//
//  vtkSmartPointer<vtkXMLDataElement> trackedFrame = vtkSmartPointer<vtkXMLDataElement>::Take(vtkXMLUtilities::ReadElementFromString(strXmlData));
//
//  if (trackedFrame == NULL)
//  {
//    //LOG_ERROR("Failed to set TrackedFrame from xml data - invalid xml data string!");
//    return IGSIO_FAIL;
//  }
//
//  // Add custom fields to tracked frame
//  for (int i = 0; i < trackedFrame->GetNumberOfNestedElements(); ++i)
//  {
//    vtkXMLDataElement* nestedElement = trackedFrame->GetNestedElement(i);
//    if (STRCASECMP(nestedElement->GetName(), "CustomFrameField") != 0)
//    {
//      continue;
//    }
//
//    const char* fieldName = nestedElement->GetAttribute("Name");
//    if (fieldName == NULL)
//    {
//      //LOG_WARNING("Unable to find CustomFrameField Name attribute");
//      continue;
//    }
//
//    const char* fieldValue = nestedElement->GetAttribute("Value");
//    if (fieldValue == NULL)
//    {
//      //LOG_WARNING("Unable to find CustomFrameField Value attribute");
//      continue;
//    }
//
//    this->SetCustomFrameField(fieldName, fieldValue);
//  }
//
//  vtkXMLDataElement* segmentation = trackedFrame->FindNestedElementWithName("Segmentation");
//
//  if (segmentation != NULL)
//  {
//    this->FiducialPointsCoordinatePx = vtkSmartPointer<vtkPoints>::New();
//
//    vtkXMLDataElement* segmentedPoints = segmentation->FindNestedElementWithName("SegmentedPoints");
//    if (segmentedPoints != NULL)   // Segmentation was successful
//    {
//      for (int i = 0; i < segmentedPoints->GetNumberOfNestedElements(); ++i)
//      {
//        vtkXMLDataElement* pointElement = segmentedPoints->GetNestedElement(i);
//        if (STRCASECMP(pointElement->GetName(), "Point") != 0)
//        {
//          continue;
//        }
//
//        double pos[3] = {0};
//        if (pointElement->GetVectorAttribute("Position", 3, pos))
//        {
//          this->FiducialPointsCoordinatePx->InsertNextPoint(pos);
//        }
//      }
//    }
//  }
//
//  return IGSIO_SUCCESS;
//}

//----------------------------------------------------------------------------
FrameSizeType igsioTrackedFrame::GetFrameSize()
{
  this->ImageData.GetFrameSize(this->FrameSize);
  return this->FrameSize;
}

//----------------------------------------------------------------------------
void igsioTrackedFrame::SetFrameSize(FrameSizeType frameSize)
{
  this->FrameSize = frameSize;
}

//----------------------------------------------------------------------------
void igsioTrackedFrame::SetImageData(const igsioVideoFrame& value)
{
  this->ImageData = value;

  // Update our cached frame size
  this->ImageData.GetFrameSize(this->FrameSize);
}

//----------------------------------------------------------------------------
void igsioTrackedFrame::SetTimestamp(double value)
{
  this->Timestamp = value;
  std::ostringstream strTimestamp;
  strTimestamp << std::setprecision(FLOATING_POINT_PRECISION) << this->Timestamp;
  this->CustomFrameFields["Timestamp"] = strTimestamp.str();
}

//----------------------------------------------------------------------------
int igsioTrackedFrame::GetNumberOfBitsPerScalar()
{
  int numberOfBitsPerScalar(0);
  numberOfBitsPerScalar = this->ImageData.GetNumberOfBytesPerScalar() * 8;
  return numberOfBitsPerScalar;
}

//----------------------------------------------------------------------------
int igsioTrackedFrame::GetNumberOfBitsPerPixel()
{
  int numberOfBitsPerScalar(0);
  unsigned int numberOfScalarComponents(1);
  if (this->GetNumberOfScalarComponents(numberOfScalarComponents) == IGSIO_FAIL)
  {
    //LOG_ERROR("Unable to retrieve number of scalar components.");
    return -1;
  }
  numberOfBitsPerScalar = this->ImageData.GetNumberOfBytesPerScalar() * 8 * numberOfScalarComponents;
  return numberOfBitsPerScalar;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::GetNumberOfScalarComponents(unsigned int& numberOfScalarComponents)
{
  return this->ImageData.GetNumberOfScalarComponents(numberOfScalarComponents);
}

//----------------------------------------------------------------------------
void igsioTrackedFrame::SetFiducialPointsCoordinatePx(vtkPoints* fiducialPoints)
{
  if (this->FiducialPointsCoordinatePx != fiducialPoints)
  {
    vtkPoints* tempFiducialPoints = this->FiducialPointsCoordinatePx;

    this->FiducialPointsCoordinatePx = fiducialPoints;
    if (this->FiducialPointsCoordinatePx != NULL)
    {
      this->FiducialPointsCoordinatePx->Register(NULL);
    }

    if (tempFiducialPoints != NULL)
    {
      tempFiducialPoints->UnRegister(NULL);
    }
  }
}

//----------------------------------------------------------------------------
void igsioTrackedFrame::SetCustomFrameField(std::string name, std::string value)
{
  if (STRCASECMP(name.c_str(), "Timestamp") == 0)
  {
    double timestamp(0);
    if (igsioCommon::StringToDouble(value.c_str(), timestamp) != IGSIO_SUCCESS)
    {
      //LOG_ERROR("Unable to convert Timestamp '" << value << "' to double");
    }
    else
    {
      this->Timestamp = timestamp;
    }
  }

  this->CustomFrameFields[name] = value;
}

//----------------------------------------------------------------------------
const char* igsioTrackedFrame::GetCustomFrameField(const char* fieldName)
{
  if (fieldName == NULL)
  {
    //LOG_ERROR("Unable to get custom frame field: field name is NULL!");
    return NULL;
  }

  FieldMapType::iterator fieldIterator;
  fieldIterator = this->CustomFrameFields.find(fieldName);
  if (fieldIterator != this->CustomFrameFields.end())
  {
    return fieldIterator->second.c_str();
  }
  return NULL;
}

//----------------------------------------------------------------------------
const char* igsioTrackedFrame::GetCustomFrameField(const std::string& fieldName)
{
  return this->GetCustomFrameField(fieldName.c_str());
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::DeleteCustomFrameField(const char* fieldName)
{
  if (fieldName == NULL)
  {
    //LOG_DEBUG("Failed to delete custom frame field - field name is NULL!");
    return IGSIO_FAIL;
  }

  FieldMapType::iterator field = this->CustomFrameFields.find(fieldName);
  if (field != this->CustomFrameFields.end())
  {
    this->CustomFrameFields.erase(field);
    return IGSIO_SUCCESS;
  }
  //LOG_DEBUG("Failed to delete custom frame field - could find field " << fieldName);
  return IGSIO_FAIL;
}


//----------------------------------------------------------------------------
bool igsioTrackedFrame::IsCustomFrameTransformNameDefined(const igsioTransformName& transformName)
{
  std::string toolTransformName;
  if (transformName.GetTransformName(toolTransformName) != IGSIO_SUCCESS)
  {
    return false;
  }
  // Append Transform to the end of the transform name
  if (!IsTransform(toolTransformName))
  {
    toolTransformName.append(TransformPostfix);
  }

  return this->IsCustomFrameFieldDefined(toolTransformName.c_str());
}

//----------------------------------------------------------------------------
bool igsioTrackedFrame::IsCustomFrameFieldDefined(const char* fieldName)
{
  if (fieldName == NULL)
  {
    //LOG_ERROR("Unable to find custom frame field: field name is NULL!");
    return false;
  }

  FieldMapType::iterator fieldIterator;
  fieldIterator = this->CustomFrameFields.find(fieldName);
  if (fieldIterator != this->CustomFrameFields.end())
  {
    // field is found
    return true;
  }
  // field is undefined
  return false;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::GetCustomFrameTransform(const igsioTransformName& frameTransformName, double transform[16])
{
  std::string transformName;
  if (frameTransformName.GetTransformName(transformName) != IGSIO_SUCCESS)
  {
    //LOG_ERROR("Unable to get custom transform, transform name is wrong!");
    return IGSIO_FAIL;
  }

  // Append Transform to the end of the transform name
  if (!IsTransform(transformName))
  {
    transformName.append(TransformPostfix);
  }

  const char* frameTransformStr = GetCustomFrameField(transformName.c_str());
  if (frameTransformStr == NULL)
  {
    //LOG_ERROR("Unable to get custom transform from name: " << transformName);
    return IGSIO_FAIL;
  }

  // Find default frame transform
  std::istringstream transformFieldValue(frameTransformStr);
  double item;
  int i = 0;
  while (transformFieldValue >> item && i < 16)
  {
    transform[i++] = item;
  }
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::GetCustomFrameTransform(const igsioTransformName& frameTransformName, vtkMatrix4x4* transformMatrix)
{
  double transform[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
  const igsioStatus retValue = this->GetCustomFrameTransform(frameTransformName, transform);
  transformMatrix->DeepCopy(transform);

  return retValue;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::GetCustomFrameTransformStatus(const igsioTransformName& frameTransformName, igsioTrackedFrameFieldStatus& status)
{
  status = FIELD_INVALID;
  std::string transformStatusName;
  if (frameTransformName.GetTransformName(transformStatusName) != IGSIO_SUCCESS)
  {
    //LOG_ERROR("Unable to get custom transform status, transform name is wrong!");
    return IGSIO_FAIL;
  }

  // Append TransformStatus to the end of the transform name
  if (IsTransform(transformStatusName))
  {
    transformStatusName.append("Status");
  }
  else if (!IsTransformStatus(transformStatusName))
  {
    transformStatusName.append(TransformStatusPostfix);
  }

  const char* strStatus = this->GetCustomFrameField(transformStatusName.c_str());
  if (strStatus == NULL)
  {
    //LOG_ERROR("Unable to get custom transform status from name: " << transformStatusName);
    return IGSIO_FAIL;
  }

  status = igsioTrackedFrame::ConvertFieldStatusFromString(strStatus);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::SetCustomFrameTransformStatus(const igsioTransformName& frameTransformName, igsioTrackedFrameFieldStatus status)
{
  std::string transformStatusName;
  if (frameTransformName.GetTransformName(transformStatusName) != IGSIO_SUCCESS)
  {
    //LOG_ERROR("Unable to set custom transform status, transform name is wrong!");
    return IGSIO_FAIL;
  }

  // Append TransformStatus to the end of the transform name
  if (IsTransform(transformStatusName))
  {
    transformStatusName.append("Status");
  }
  else if (!IsTransformStatus(transformStatusName))
  {
    transformStatusName.append(TransformStatusPostfix);
  }

  std::string strStatus = igsioTrackedFrame::ConvertFieldStatusToString(status);

  this->SetCustomFrameField(transformStatusName, strStatus);

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::SetCustomFrameTransform(const igsioTransformName& frameTransformName, double transform[16])
{
  std::ostringstream strTransform;
  for (int i = 0; i < 16; ++i)
  {
    strTransform << std::setprecision(FLOATING_POINT_PRECISION) << transform[ i ] << " ";
  }

  std::string transformName;
  if (frameTransformName.GetTransformName(transformName) != IGSIO_SUCCESS)
  {
    //LOG_ERROR("Unable to get custom transform, transform name is wrong!");
    return IGSIO_FAIL;
  }

  // Append Transform to the end of the transform name
  if (!IsTransform(transformName))
  {
    transformName.append(TransformPostfix);
  }

  SetCustomFrameField(transformName, strTransform.str());

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::SetCustomFrameTransform(const igsioTransformName& frameTransformName, vtkMatrix4x4* transform)
{
  double dTransform[ 16 ];
  vtkMatrix4x4::DeepCopy(dTransform, transform);
  return SetCustomFrameTransform(frameTransformName, dTransform);
}

//----------------------------------------------------------------------------
igsioTrackedFrameFieldStatus igsioTrackedFrame::ConvertFieldStatusFromString(const char* statusStr)
{
  if (statusStr == NULL)
  {
    //LOG_ERROR("Failed to get field status from string if it's NULL!");
    return FIELD_INVALID;
  }

  igsioTrackedFrameFieldStatus status = FIELD_INVALID;
  std::string strFlag(statusStr);
  if (STRCASECMP("OK", statusStr) == 0)
  {
    status = FIELD_OK;
  }

  return status;
}

//----------------------------------------------------------------------------
std::string igsioTrackedFrame::ConvertFieldStatusToString(igsioTrackedFrameFieldStatus status)
{
  std::string strStatus;
  if (status == FIELD_OK)
  {
    strStatus = "OK";
  }
  else
  {
    strStatus = "INVALID";
  }
  return strStatus;
}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrame::WriteToFile(const std::string& filename, vtkMatrix4x4* imageToTracker)
{
  //vtkImageData* volumeToSave = this->GetImageData()->GetImage();
  //MET_ValueEnumType scalarType = MET_NONE;
  //switch (volumeToSave->GetScalarType())
  //{
  //  case VTK_UNSIGNED_CHAR:
  //    scalarType = MET_UCHAR;
  //    break;
  //  case VTK_FLOAT:
  //    scalarType = MET_FLOAT;
  //    break;
  //  default:
  //    //LOG_ERROR("Scalar type is not supported!");
  //    return IGSIO_FAIL;
  //}

  //MetaImage metaImage(volumeToSave->GetDimensions()[0], volumeToSave->GetDimensions()[1], volumeToSave->GetDimensions()[2],
  //                    volumeToSave->GetSpacing()[0], volumeToSave->GetSpacing()[1], volumeToSave->GetSpacing()[2],
  //                    scalarType, volumeToSave->GetNumberOfScalarComponents(), volumeToSave->GetScalarPointer());
  //double origin[3];
  //origin[0] = imageToTracker->Element[0][3];
  //origin[1] = imageToTracker->Element[1][3];
  //origin[2] = imageToTracker->Element[2][3];
  //metaImage.Origin(origin);
  //for (int i = 0; i < 3; ++i)
  //{
  //  for (int j = 0; j < 3; ++j)
  //  {
  //    metaImage.Orientation(i, j, imageToTracker->Element[i][j]);
  //  }
  //}
  //// By definition, LPS orientation in DICOM sense = RAI orientation in MetaIO. See details at:
  //// http://www.itk.org/Wiki/Proposals:Orientation#Some_notes_on_the_DICOM_convention_and_current_ITK_usage
  //metaImage.AnatomicalOrientation("RAI");
  //metaImage.BinaryData(true);
  //metaImage.CompressedData(true);
  //metaImage.ElementDataFileName("LOCAL");
  //if (metaImage.Write(filename.c_str()) == false)
  //{
  //  //LOG_ERROR("Failed to save reconstructed volume in sequence metafile!");
  //  return IGSIO_FAIL;
  //}
  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
void igsioTrackedFrame::GetCustomFrameFieldNameList(std::vector<std::string>& fieldNames)
{
  fieldNames.clear();
  for (FieldMapType::const_iterator it = this->CustomFrameFields.begin(); it != this->CustomFrameFields.end(); it++)
  {
    fieldNames.push_back(it->first);
  }
}

//----------------------------------------------------------------------------
void igsioTrackedFrame::GetCustomFrameTransformNameList(std::vector<igsioTransformName>& transformNames)
{
  transformNames.clear();
  for (FieldMapType::const_iterator it = this->CustomFrameFields.begin(); it != this->CustomFrameFields.end(); it++)
  {
    if (IsTransform(it->first))
    {
      igsioTransformName trName;
      trName.SetTransformName(it->first.substr(0, it->first.length() - TransformPostfix.length()).c_str());
      transformNames.push_back(trName);
    }
  }
}

//----------------------------------------------------------------------------
bool igsioTrackedFrame::IsTransform(std::string str)
{
  if (str.length() <= TransformPostfix.length())
  {
    return false;
  }

  return igsioCommon::IsEqualInsensitive(str.substr(str.length() - TransformPostfix.length()), TransformPostfix);
}

//----------------------------------------------------------------------------
bool igsioTrackedFrame::IsTransformStatus(std::string str)
{
  if (str.length() <= TransformStatusPostfix.length())
  {
    return false;
  }

  return igsioCommon::IsEqualInsensitive(str.substr(str.length() - TransformStatusPostfix.length()), TransformStatusPostfix);
}

//----------------------------------------------------------------------------
// ****************** TrackedFrameEncoderPositionFinder **********************
//----------------------------------------------------------------------------
igsioTrackedFrameEncoderPositionFinder::igsioTrackedFrameEncoderPositionFinder(igsioTrackedFrame* frame, double minRequiredTranslationDifferenceMm, double minRequiredAngleDifferenceDeg)
  : mTrackedFrame(frame),
    mMinRequiredTranslationDifferenceMm(minRequiredTranslationDifferenceMm),
    mMinRequiredAngleDifferenceDeg(minRequiredAngleDifferenceDeg)
{

}

//----------------------------------------------------------------------------
igsioTrackedFrameEncoderPositionFinder::~igsioTrackedFrameEncoderPositionFinder()
{

}

//----------------------------------------------------------------------------
igsioStatus igsioTrackedFrameEncoderPositionFinder::GetStepperEncoderValues(igsioTrackedFrame* trackedFrame, double& probePosition, double& probeRotation, double& templatePosition)
{
  if (trackedFrame == NULL)
  {
    //LOG_ERROR("Unable to get stepper encoder values - input tracked frame is NULL!");
    return IGSIO_FAIL;
  }

  // Get the probe position from tracked frame info
  const char* cProbePos = trackedFrame->GetCustomFrameField("ProbePosition");
  if (cProbePos == NULL)
  {
    //LOG_ERROR("Couldn't find ProbePosition field in tracked frame!");
    return IGSIO_FAIL;
  }

  if (igsioCommon::StringToDouble(cProbePos, probePosition) != IGSIO_SUCCESS)
  {
    //LOG_ERROR("Failed to convert probe position " << cProbePos << " to double!");
    return IGSIO_FAIL;
  }

  // Get the probe rotation from tracked frame info
  const char* cProbeRot = trackedFrame->GetCustomFrameField("ProbeRotation");
  if (cProbeRot == NULL)
  {
    //LOG_ERROR("Couldn't find ProbeRotation field in tracked frame!");
    return IGSIO_FAIL;
  }

  if (igsioCommon::StringToDouble(cProbeRot, probeRotation) != IGSIO_SUCCESS)
  {
    //LOG_ERROR("Failed to convert probe rotation " << cProbeRot << " to double!");
    return IGSIO_FAIL;
  }

  // Get the template position from tracked frame info
  const char* cTemplatePos = trackedFrame->GetCustomFrameField("TemplatePosition");
  if (cTemplatePos == NULL)
  {
    //LOG_ERROR("Couldn't find TemplatePosition field in tracked frame!");
    return IGSIO_FAIL;
  }

  if (igsioCommon::StringToDouble(cTemplatePos, templatePosition) != IGSIO_SUCCESS)
  {
    //LOG_ERROR("Failed to convert template position " << cTemplatePos << " to double!");
    return IGSIO_FAIL;
  }

  return IGSIO_SUCCESS;
}

//----------------------------------------------------------------------------
bool igsioTrackedFrameEncoderPositionFinder::operator()(igsioTrackedFrame* newFrame)
{
  if (mMinRequiredTranslationDifferenceMm <= 0 || mMinRequiredAngleDifferenceDeg <= 0)
  {
    // threshold is zero, so the frames are different for sure
    return false;
  }

  double baseProbePos(0), baseProbeRot(0), baseTemplatePos(0);


  if (GetStepperEncoderValues(mTrackedFrame, baseProbePos, baseProbeRot, baseTemplatePos) != IGSIO_SUCCESS)
  {
    //LOG_WARNING("Unable to get raw encoder values from tracked frame!");
    return false;
  }

  double newProbePos(0), newProbeRot(0), newTemplatePos(0);
  if (GetStepperEncoderValues(newFrame, newProbePos, newProbeRot, newTemplatePos) != IGSIO_SUCCESS)
  {
    //LOG_WARNING("Unable to get raw encoder values from tracked frame!");
    return false;
  }

  double positionDifference = fabs(baseProbePos - newProbePos) + fabs(baseTemplatePos - newTemplatePos);
  double rotationDifference = fabs(baseProbeRot - newProbeRot);

  if (positionDifference < this->mMinRequiredTranslationDifferenceMm && rotationDifference < this->mMinRequiredAngleDifferenceDeg)
  {
    // same as the reference frame
    return true;
  }
  return false;
}

//----------------------------------------------------------------------------
// ****************** TrackedFrameTransformFinder ****************************
//----------------------------------------------------------------------------
TrackedFrameTransformFinder::TrackedFrameTransformFinder(igsioTrackedFrame* frame, const igsioTransformName& frameTransformName, double minRequiredTranslationDifferenceMm, double minRequiredAngleDifferenceDeg)
  : mTrackedFrame(frame),
    mMinRequiredTranslationDifferenceMm(minRequiredTranslationDifferenceMm),
    mMinRequiredAngleDifferenceDeg(minRequiredAngleDifferenceDeg),
    mFrameTransformName(frameTransformName)
{

}

//----------------------------------------------------------------------------
TrackedFrameTransformFinder::~TrackedFrameTransformFinder()
{

}

//----------------------------------------------------------------------------
bool TrackedFrameTransformFinder::operator()(igsioTrackedFrame* newFrame)
{
  if (mMinRequiredTranslationDifferenceMm <= 0 || mMinRequiredAngleDifferenceDeg <= 0)
  {
    // threshold is zero, so the frames are different for sure
    return false;
  }

  vtkSmartPointer<vtkMatrix4x4> baseTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  double baseTransMatrix[16] = {0};
  if (mTrackedFrame->GetCustomFrameTransform(mFrameTransformName, baseTransMatrix))
  {
    baseTransformMatrix->DeepCopy(baseTransMatrix);
  }
  else
  {
    //LOG_ERROR("TrackedFramePositionFinder: Unable to find base frame transform name for tracked frame validation!");
    return false;
  }

  vtkSmartPointer<vtkMatrix4x4> newTransformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
  double newTransMatrix[16] = {0};
  if (newFrame->GetCustomFrameTransform(mFrameTransformName, newTransMatrix))
  {
    newTransformMatrix->DeepCopy(newTransMatrix);
  }
  else
  {
    //LOG_ERROR("TrackedFramePositionFinder: Unable to find frame transform name for new tracked frame validation!");
    return false;
  }

  // TODO
  //double positionDifference = igsioMath::GetPositionDifference(baseTransformMatrix, newTransformMatrix);
  //double angleDifference = igsioMath::GetOrientationDifference(baseTransformMatrix, newTransformMatrix);

  //if (fabs(positionDifference) < this->mMinRequiredTranslationDifferenceMm && fabs(angleDifference) < this->mMinRequiredAngleDifferenceDeg)
  //{
  //  // same as the reference frame
  //  return true;
  //}

  return false;
}