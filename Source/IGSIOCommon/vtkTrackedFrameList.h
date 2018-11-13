/*=Plus=header=begin======================================================
  Program: Plus
  Copyright (c) Laboratory for Percutaneous Surgery. All rights reserved.
  See License.txt for details.
=========================================================Plus=header=end*/

#ifndef __VTKIGSIOTRACKEDFRAMELIST_h
#define __VTKIGSIOTRACKEDFRAMELIST_h

#include "vtkigsiocommon_export.h"

// IGSIO includes
#include "igsioCommon.h"
#include "igsioTrackedFrame.h"

// VTK includes
#include "vtkObject.h"

// STL includes
#include <deque>

class vtkXMLDataElement;
class igsioTrackedFrame;
class vtkMatrix4x4;


/*!
  \class vtkTrackedFrameList
  \brief Stores a list of tracked frames (image + pose information)

  Validation threshold values: If the threshold==0 it means that no
  checking is needed (the frame is always accepted). If the threshold>0
  then the frame is considered valid only if the position/angle
  difference compared to all previously acquired frames is larger than
  the position/angle minimum value and the translation/rotation speed is lower
  than the maximum allowed translation/rotation.

  \ingroup IGSIOCommon
*/
class VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList : public vtkObject
{

public:
  typedef std::deque<igsioTrackedFrame*> TrackedFrameListType;
  typedef std::map<std::string, std::string> FieldMapType;

  static vtkTrackedFrameList* New();
  vtkTypeMacro(vtkTrackedFrameList, vtkObject);
  virtual void PrintSelf(ostream& os, vtkIndent indent) VTK_OVERRIDE;

  /*!
    Action performed after AddTrackedFrame got invalid frame.
    Invalid frame can be a TrackedFrame if the validation requirement didn't meet the expectation.
  */
  enum InvalidFrameAction
  {
    ADD_INVALID_FRAME_AND_REPORT_ERROR = 0, /*!< Add invalid frame to the list and report an error */
    ADD_INVALID_FRAME, /*!< Add invalid frame to the list wihout notification */
    SKIP_INVALID_FRAME_AND_REPORT_ERROR, /*!< Skip invalid frame and report an error */
    SKIP_INVALID_FRAME /*!< Skip invalid frame wihout notification */
  };

  /*! Add tracked frame to container. If the frame is invalid then it may not actually add it to the list. */
  virtual igsioStatus AddTrackedFrame(igsioTrackedFrame* trackedFrame, InvalidFrameAction action = ADD_INVALID_FRAME_AND_REPORT_ERROR);

  /*! Add tracked frame to container by taking ownership of the passed pointer. If the frame is invalid then it may not actually add it to the list (it will be deleted immediately). */
  virtual igsioStatus TakeTrackedFrame(igsioTrackedFrame* trackedFrame, InvalidFrameAction action = ADD_INVALID_FRAME_AND_REPORT_ERROR);

  /*! Add all frames from a tracked frame list to the container. It adds all invalid frames as well, but an error is reported. */
  virtual igsioStatus AddTrackedFrameList(vtkTrackedFrameList* inTrackedFrameList, InvalidFrameAction action = ADD_INVALID_FRAME_AND_REPORT_ERROR);

  /*! Get tracked frame from container */
  virtual igsioTrackedFrame* GetTrackedFrame(int frameNumber);
  virtual igsioTrackedFrame* GetTrackedFrame(unsigned int frameNumber);

  /*! Get number of tracked frames */
  virtual unsigned int GetNumberOfTrackedFrames()
  {
    return this->Size();
  }
  virtual unsigned int Size() { return this->TrackedFrameList.size(); }

  /*! Save the tracked data to sequence metafile */
  igsioStatus SaveToSequenceMetafile(const std::string& filename, US_IMAGE_ORIENTATION orientationInFile = US_IMG_ORIENT_MF, bool useCompression = true, bool enableImageDataWrite = true);

  /*! Read the tracked data from sequence metafile */
  virtual igsioStatus ReadFromSequenceMetafile(const std::string& trackedSequenceDataFileName);

  /*! Save the tracked data to Nrrd file */
  igsioStatus SaveToNrrdFile(const std::string& filename, US_IMAGE_ORIENTATION orientationInFile = US_IMG_ORIENT_MF, bool useCompression = true, bool enableImageDataWrite = true);

  /*! Read the tracked data from Nrrd file */
  virtual igsioStatus ReadFromNrrdFile(const std::string& trackedSequenceDataFileName);

  /*! Get the tracked frame list */
  TrackedFrameListType GetTrackedFrameList()
  {
    return this->TrackedFrameList;
  }

  /* Retrieve the latest timestamp in the tracked frame list */
  double GetMostRecentTimestamp();

  /*! Remove a tracked frame from the list and free up memory
    \param frameNumber Index of tracked frame to remove (from 0 to NumberOfFrames-1)
  */
  virtual igsioStatus RemoveTrackedFrame(int frameNumber);

  /*! Remove a range of tracked frames from the list and free up memory
    \param frameNumberFrom First frame to be removed (inclusive)
    \param frameNumberTo Last frame to be removed (inclusive)
  */
  virtual igsioStatus RemoveTrackedFrameRange(unsigned int frameNumberFrom, unsigned int frameNumberTo);

  /*! Clear tracked frame list and free memory */
  virtual void Clear();

  /*! Set the number of following unique frames needed in the tracked frame list */
  vtkSetMacro(NumberOfUniqueFrames, int);

  /*! Get the number of following unique frames needed in the tracked frame list */
  vtkGetMacro(NumberOfUniqueFrames, int);

  /*! Set the threshold of acceptable speed of position change */
  vtkSetMacro(MinRequiredTranslationDifferenceMm, double);

  /*!Get the threshold of acceptable speed of position change */
  vtkGetMacro(MinRequiredTranslationDifferenceMm, double);

  /*! Set the threshold of acceptable speed of orientation change in degrees */
  vtkSetMacro(MinRequiredAngleDifferenceDeg, double);

  /*! Get the threshold of acceptable speed of orientation change in degrees */
  vtkGetMacro(MinRequiredAngleDifferenceDeg, double);

  /*! Set the maximum allowed translation speed in mm/sec */
  vtkSetMacro(MaxAllowedTranslationSpeedMmPerSec, double);

  /*! Get the maximum allowed translation speed in mm/sec */
  vtkGetMacro(MaxAllowedTranslationSpeedMmPerSec, double);

  /*! Set the maximum allowed rotation speed in degree/sec */
  vtkSetMacro(MaxAllowedRotationSpeedDegPerSec, double);

  /*! Get the maximum allowed rotation speed in degree/sec */
  vtkGetMacro(MaxAllowedRotationSpeedDegPerSec, double);

  /*! Set validation requirements
  \sa TrackedFrameValidationRequirements
  */
  vtkSetMacro(ValidationRequirements, long);

  /*! Get validation requirements
  \sa TrackedFrameValidationRequirements
  */
  vtkGetMacro(ValidationRequirements, long);

  /*! Set frame transform name used for transform validation */
  void SetFrameTransformNameForValidation(const igsioTransformName& aTransformName)
  {
    this->FrameTransformNameForValidation = aTransformName;
  }

  /*! Get frame transform name used for transform validation */
  igsioTransformName GetFrameTransformNameForValidation()
  {
    return this->FrameTransformNameForValidation;
  }

  /*! Get tracked frame scalar size in bits */
  virtual int GetNumberOfBitsPerScalar();

  /*! Get tracked frame pixel size in bits (scalar size * number of scalar components) */
  virtual int GetNumberOfBitsPerPixel();

  /*! Get tracked frame pixel type */
  igsioCommon::VTKScalarPixelType GetPixelType();

  /*! Get number of components */
  int GetNumberOfScalarComponents();

  /*! Get tracked frame image orientation */
  US_IMAGE_ORIENTATION GetImageOrientation();

  /*! Get tracked frame image type */
  US_IMAGE_TYPE GetImageType();

  /*! Get tracked frame image size*/
  igsioStatus GetFrameSize(FrameSizeType& outFrameSize);

  /*! Get tracked frame image size for compressed frames */
  FrameSizeType GetCompressedFrameSize() { return this->CompressedFrameSize; };
  
  /*! Set tracked frame image size for compressed frames */
  void SetCompressedFrameSize(FrameSizeType frameSize) { this->CompressedFrameSize = frameSize; };

  /*! Get the value of the custom field. If we couldn't find it, return NULL */
  virtual const char* GetCustomString(const char* fieldName);
  virtual std::string GetCustomString(const std::string& fieldName) const;

  /*! Set custom string value to \c fieldValue. If \c fieldValue is NULL then the field is deleted. */
  virtual igsioStatus SetCustomString(const char* fieldName, const char* fieldValue);
  virtual igsioStatus SetCustomString(const std::string& fieldName, const std::string& fieldValue);

  /*! Get the custom transformation matrix from metafile by custom frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName]
  * Return false if the the field is missing */
  virtual igsioStatus GetCustomTransform(const char* frameTransformName, vtkMatrix4x4* transformMatrix);

  /*! Get the custom transformation matrix from metafile by custom frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName]
  * Return false if the the field is missing */
  virtual igsioStatus GetCustomTransform(const char* frameTransformName, double* transformMatrix);

  /*! Set the custom transformation matrix from metafile by custom frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName] */
  virtual void SetCustomTransform(const char* frameTransformName, vtkMatrix4x4* transformMatrix);

  /*! Set the custom transformation matrix from metafile by custom frame transform name
  * It will search for a field like: Seq_Frame[frameNumber]_[frameTransformName] */
  virtual void SetCustomTransform(const char* frameTransformName, double* transformMatrix);

  /*! Get custom field name list */
  void GetCustomFieldNameList(std::vector<std::string>& fieldNames);

  /*! Get global transform (stored in the Offset and TransformMatrix fields) */
  igsioStatus GetGlobalTransform(vtkMatrix4x4* globalTransform);

  /*! Set global transform (stored in the Offset and TransformMatrix fields) */
  igsioStatus SetGlobalTransform(vtkMatrix4x4* globalTransform);

  /*!
    Verify properties of a tracked frame list. If the tracked frame list pointer is invalid or the expected properties
    (image orientation, type) are different from the actual values then the method returns with failure.
    It is a static method so that the validity of the pointer can be easily checked as well.
  */
  static igsioStatus VerifyProperties(vtkTrackedFrameList* trackedFrameList, US_IMAGE_ORIENTATION expectedOrientation, US_IMAGE_TYPE expectedType);

  /*! Return true if the list contains at least one valid image frame */
  bool IsContainingValidImageData();

  /*! Implement support for C++11 ranged for loops */
  TrackedFrameListType::iterator begin();
  TrackedFrameListType::iterator end();
  TrackedFrameListType::const_iterator begin() const;
  TrackedFrameListType::const_iterator end() const;

  TrackedFrameListType::reverse_iterator rbegin();
  TrackedFrameListType::reverse_iterator rend();
  TrackedFrameListType::const_reverse_iterator rbegin() const;
  TrackedFrameListType::const_reverse_iterator rend() const;


  void SetUseCompression(bool useCompression) { this->UseCompression = useCompression; };
  bool GetUseCompression() { return this->UseCompression; };
  void SetCodecFourCC(std::string codecFourCC) { this->CodecFourCC = codecFourCC; };
  std::string GetCodecFourCC() { return this->CodecFourCC; };

  vtkSetStdStringMacro(ImageName);
  vtkGetStdStringMacro(ImageName);

  double GetFPS();

protected:
  vtkTrackedFrameList();
  virtual ~vtkTrackedFrameList();

  /*!
    Perform validation on a tracked frame . If any of the requested requirement is not fulfilled then the validation fails.
    \param trackedFrame Input tracked frame
    \return True if the frame is valid
    \sa TrackedFrameValidationRequirements
  */
  virtual bool ValidateData(igsioTrackedFrame* trackedFrame);

  bool ValidateTimestamp(igsioTrackedFrame* trackedFrame);
  bool ValidateTransform(igsioTrackedFrame* trackedFrame);
  bool ValidateStatus(igsioTrackedFrame* trackedFrame);
  bool ValidateEncoderPosition(igsioTrackedFrame* trackedFrame);
  bool ValidateSpeed(igsioTrackedFrame* trackedFrame);

  TrackedFrameListType TrackedFrameList;
  FieldMapType CustomFields;

  int NumberOfUniqueFrames;

  /*! Validation threshold value */
  double MinRequiredTranslationDifferenceMm;
  /*! Validation threshold value */
  double MinRequiredAngleDifferenceDeg;
  /*! Validation threshold value */
  double MaxAllowedTranslationSpeedMmPerSec;
  /*! Validation threshold value */
  double MaxAllowedRotationSpeedDegPerSec;

  long ValidationRequirements;
  igsioTransformName FrameTransformNameForValidation;

  // TODO rename UseCompression
  bool UseCompression;
  FrameSizeType CompressedFrameSize;
  std::string CodecFourCC;
  
  std::string ImageName;

private:
  vtkTrackedFrameList(const vtkTrackedFrameList&);
  void operator=(const vtkTrackedFrameList&);
};

/// Implement support for C++11 ranged for loops
VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::iterator begin(vtkTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::iterator end(vtkTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::const_iterator begin(const vtkTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::const_iterator end(const vtkTrackedFrameList& list);

VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::reverse_iterator rbegin(vtkTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::reverse_iterator rend(vtkTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::const_reverse_iterator rbegin(const vtkTrackedFrameList& list);
VTKIGSIOCOMMON_EXPORT vtkTrackedFrameList::TrackedFrameListType::const_reverse_iterator rend(const vtkTrackedFrameList& list);

#endif