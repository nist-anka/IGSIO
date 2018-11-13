/*==============================================================================

Copyright (c) Laboratory for Percutaneous Surgery (PerkLab)
Queen's University, Kingston, ON, Canada. All Rights Reserved.

See COPYRIGHT.txt
or http://www.slicer.org/copyright/copyright.txt for details.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

This file was originally developed by Kyle Sunderland, PerkLab, Queen's University

==============================================================================*/

// std includes
#include <sstream>

// libwebm includes
#include <mkvreader.h>

// VTK includes
#include <vtkObjectFactory.h>

// vtkVideoIO includes
#include "vtkMKVReader.h"

//----------------------------------------------------------------------------
vtkStandardNewMacro(vtkMKVReader);

class vtkMKVReader::vtkInternal
{
public:
  vtkMKVReader* External;

  double FrameRate;
  mkvparser::MkvReader* MKVReader;
  mkvparser::EBMLHeader* EBMLHeader;
  mkvparser::Segment* MKVReadSegment;

  vtkMKVUtil::VideoTrackMap    VideoTracks;
  vtkMKVUtil::MetadataTrackMap MetadataTracks;

  //----------------------------------------------------------------------------
  vtkInternal(vtkMKVReader* external)
    : External(external)
    , MKVReader(new mkvparser::MkvReader())
    , EBMLHeader(NULL)
    , MKVReadSegment(NULL)
  {
  }

  //----------------------------------------------------------------------------
  virtual ~vtkInternal()
  {
    delete this->MKVReader;
    this->MKVReader = NULL;
  }

};

//---------------------------------------------------------------------------
vtkMKVReader::vtkMKVReader()
  : Internal(new vtkInternal(this))
{
}

//---------------------------------------------------------------------------
vtkMKVReader::~vtkMKVReader()
{
  delete this->Internal;
  this->Internal = NULL;
}

//----------------------------------------------------------------------------
bool vtkMKVReader::ReadFile()
{
  if (!this->TrackedFrameList)
  {
    vtkErrorMacro("ReadFile: Tracked frame list not set");
    return false;
  }
  this->TrackedFrameList->Clear();

  if (!this->ReadHeader())
  {
    this->Close();
    return false;
  }

  if (!this->ReadContents())
  {
    this->Close();
    return false;
  }

  std::string codec;
  vtkMKVUtil::VideoTrackMap videoTracks = this->GetVideoTracks();
  for (vtkMKVUtil::VideoTrackMap::iterator videoTrackIt = videoTracks.begin(); videoTrackIt != videoTracks.end(); ++videoTrackIt)
  {
    codec = videoTrackIt->second.Encoding;
    this->TrackedFrameList->SetImageName(videoTrackIt->second.Name);
    this->TrackedFrameList->SetCodecFourCC(codec);
    FrameSizeType frameSize;
    frameSize[0] = videoTrackIt->second.Width;
    frameSize[1] = videoTrackIt->second.Height;
    frameSize[2] = 1;

    if (!vtkMKVUtil::UseCompressionFourCC(videoTrackIt->second.Encoding))
    {
      this->TrackedFrameList->SetUseCompression(false);
    }
    else
    {
      this->TrackedFrameList->SetUseCompression(true);
      this->TrackedFrameList->SetCompressedFrameSize(frameSize);
    }

    std::vector<vtkMKVUtil::FrameInfo> frames = videoTrackIt->second.Frames;
    for (std::vector<vtkMKVUtil::FrameInfo>::iterator frameIt = frames.begin(); frameIt != frames.end(); ++frameIt)
    {
      igsioTrackedFrame trackedFrame;
      trackedFrame.SetTimestamp(frameIt->TimestampSeconds);

      igsioVideoFrame videoFrame;

      if (!this->TrackedFrameList->GetUseCompression())
      {
        videoFrame.AllocateFrame(frameSize, VTK_UNSIGNED_CHAR, 3); // TODO get from frame (greyscale vs color)

        vtkImageData* imageData = videoFrame.GetImage();
        if (imageData && imageData->GetScalarPointer())
        {
          unsigned long size = frameIt->Data->GetSize();
          memcpy(imageData->GetScalarPointer(), frameIt->Data->GetPointer(0), size);
        }
      }
      else
      {
        videoFrame.SetCompressedFrameData(frameIt->Data);
        videoFrame.SetFrameType(frameIt->IsKey ? FRAME_TYPE_IFRAME : FRAME_TYPE_PFRAME); // TODO: handle additional frame types
      }
      trackedFrame.SetImageData(videoFrame);
      this->TrackedFrameList->AddTrackedFrame(&trackedFrame);
    }
  }

  vtkMKVUtil::MetadataTrackMap metadataTracks = this->GetMetadataTracks();
  for (vtkMKVUtil::MetadataTrackMap::iterator metadataTrackIt = metadataTracks.begin(); metadataTrackIt != metadataTracks.end(); ++metadataTrackIt)
  {
    for (vtkMKVUtil::FrameInfoList::iterator frameIt = metadataTrackIt->second.Frames.begin(); frameIt != metadataTrackIt->second.Frames.end(); ++frameIt)
    {
      for (unsigned int i = 0; i < this->TrackedFrameList->GetNumberOfTrackedFrames(); ++i)
      {
        igsioTrackedFrame* trackedFrame = this->TrackedFrameList->GetTrackedFrame(i);
        if (trackedFrame->GetTimestamp() != frameIt->TimestampSeconds)
        {
          continue;
        }
        std::string frameField = (char*)frameIt->Data->GetPointer(0);
        trackedFrame->SetCustomFrameField(metadataTrackIt->second.Name, frameField);
      }
    }
  }
  this->Close();
  return true;
}

//----------------------------------------------------------------------------
bool vtkMKVReader::ReadHeader()
{
  this->Close();
  this->Internal->MKVReader->Open(this->Filename.c_str());

  if (!this->Internal->EBMLHeader)
  {
    this->Internal->EBMLHeader = new mkvparser::EBMLHeader();
  }
  long long readerPosition;
  this->Internal->EBMLHeader->Parse(this->Internal->MKVReader, readerPosition);

  mkvparser::Segment::CreateInstance(this->Internal->MKVReader, readerPosition, this->Internal->MKVReadSegment);
  if (!this->Internal->MKVReadSegment)
  {
    vtkErrorMacro("Could not read mkv segment!");
    this->Close();
    return false;
  }

  this->Internal->MKVReadSegment->Load();
  this->Internal->MKVReadSegment->ParseHeaders();

  const mkvparser::Tracks* tracks = this->Internal->MKVReadSegment->GetTracks();
  if (!tracks)
  {
    vtkErrorMacro("Could not retrieve tracks!");
    this->Close();
    return false;
  }

  unsigned long numberOfTracks = tracks->GetTracksCount();
  for (unsigned long trackIndex = 0; trackIndex < numberOfTracks; ++trackIndex)
  {
    const mkvparser::Track* track = tracks->GetTrackByIndex(trackIndex);
    if (!track)
    {
      continue;
    }

    const long trackType = track->GetType();
    const long trackNumber = track->GetNumber();
    if (trackType == mkvparser::Track::kVideo)
    {
      const mkvparser::VideoTrack* const videoTrack = static_cast<const mkvparser::VideoTrack*>(track);

      std::string trackName;
      if (videoTrack->GetNameAsUTF8())
      {
        trackName = videoTrack->GetNameAsUTF8();
      }
      else
      {
        std::stringstream ss;
        ss << "Video_" << trackNumber;
        trackName = ss.str();
      }

      std::string encoding;
      if (videoTrack->GetCodecId())
      {
        encoding = videoTrack->GetCodecId();
      }
      else
      {
        encoding = "Unknown";
      }

      std::string encodingFourCC;
      if (encoding != VTKVIDEOIO_MKV_UNCOMPRESSED_CODECID)
      {
        encodingFourCC = vtkMKVUtil::CodecIdToFourCC(encoding);
      }
      else if (videoTrack->GetColourSpace())
      {
        encodingFourCC = videoTrack->GetColourSpace();
      }

      unsigned long width = videoTrack->GetWidth();
      unsigned long height = videoTrack->GetHeight();
      double frameRate = videoTrack->GetFrameRate();
      bool isGreyScale = false;

      vtkMKVUtil::VideoTrackInfo videoTrackInfo(trackName, encodingFourCC, width, height, frameRate, isGreyScale);
      this->Internal->VideoTracks[trackNumber] = videoTrackInfo;
    }
    else if (trackType == mkvparser::Track::kMetadata || trackType == mkvparser::Track::kSubtitle)
    {
      std::string trackName = track->GetNameAsUTF8();
      std::string encoding;
      if (track->GetCodecId())
      {
        encoding = track->GetCodecId();
      }
      else
      {
        encoding = "Unknown";
      }
      vtkMKVUtil::MetadataTrackInfo metadataTrackInfo(trackName, encoding);
      this->Internal->MetadataTracks[trackNumber] = metadataTrackInfo;
    }
  }

  return true;
}

//----------------------------------------------------------------------------
bool vtkMKVReader::ReadContents()
{
  const mkvparser::Tracks* tracks = this->Internal->MKVReadSegment->GetTracks();

  double lastTimestamp = -1;

  unsigned int totalFrameNumber = 0;
  const mkvparser::Cluster* cluster = this->Internal->MKVReadSegment->GetFirst();
  while (cluster != NULL && !cluster->EOS())
  {
    const long long timeCode = cluster->GetTimeCode();
    const long long timestampNanoSeconds = cluster->GetTime();
    // Convert nanoseconds to seconds
    double timestampSeconds = timestampNanoSeconds / NANOSECONDS_IN_SECOND ; // Timestamp of the current cluster in seconds

    const mkvparser::BlockEntry* blockEntry;
    long status = cluster->GetFirst(blockEntry);
    if (status < 0)
    {
      vtkErrorMacro("Could not find block!");
      return false;
    }

    while (blockEntry != NULL && !blockEntry->EOS())
    {
      const mkvparser::Block* const block = blockEntry->GetBlock();
      unsigned long trackNumber = static_cast<unsigned long>(block->GetTrackNumber());
      const mkvparser::Track* const track = tracks->GetTrackByNumber(trackNumber);

      if (!track)
      {
        vtkErrorMacro("Could not find track!");
        return false;
      }

      const int trackType = track->GetType();
      const int frameCount = block->GetFrameCount();
      for (int i = 0; i < frameCount; ++i)
      {
        const mkvparser::Block::Frame& frame = block->GetFrame(i);
        const long size = frame.len;
        const long long offset = frame.pos;

        vtkSmartPointer<vtkUnsignedCharArray> bitstream = vtkSmartPointer<vtkUnsignedCharArray>::New();
        bitstream->Allocate(size);
        frame.Read(this->Internal->MKVReader, bitstream->GetPointer(0));

        // Handle video frame
        if (trackType == mkvparser::Track::kVideo)
        {
          vtkMKVUtil::VideoTrackInfo* videoTrack = &this->Internal->VideoTracks[trackNumber];
          if (lastTimestamp != -1 && lastTimestamp == timestampSeconds)
          {
            if (videoTrack->FrameRate > 0.0)
            {
              timestampSeconds = lastTimestamp + (1. / videoTrack->FrameRate);
            }
            else
            {
              // TODO: Not all files seem to be encoded with either timestamp or framerate
              // Timestamp hasn't changed and there is no framerate tag, assuming 25 fps
              timestampSeconds = lastTimestamp + (1. / 25.);
            }
          }

          vtkMKVUtil::FrameInfo frameInfo;
          frameInfo.Data = bitstream;
          frameInfo.TimestampSeconds = timestampSeconds;
          frameInfo.IsKey = block->IsKey();
          videoTrack->Frames.push_back(frameInfo);

          lastTimestamp = timestampSeconds;
          totalFrameNumber++;
        }
        else if (trackType == mkvparser::Track::kMetadata || trackType == mkvparser::Track::kSubtitle)
        {
          vtkMKVUtil::MetadataTrackInfo* metaDataTrack = &this->Internal->MetadataTracks[trackNumber];
          vtkMKVUtil::FrameInfo frameInfo;
          frameInfo.Data = bitstream;
          frameInfo.TimestampSeconds = timestampSeconds;
          metaDataTrack->Frames.push_back(frameInfo);
        }
      }

      status = cluster->GetNext(blockEntry, blockEntry);
      if (status < 0)  // error
      {
        this->Close();
        vtkErrorMacro("Error reading MKV: " << status << "!");
        return false;
      }
    }
    cluster = this->Internal->MKVReadSegment->GetNext(cluster);
  }

  return true;
}

//----------------------------------------------------------------------------
vtkMKVUtil::VideoTrackMap vtkMKVReader::GetVideoTracks()
{
  return this->Internal->VideoTracks;
}

//----------------------------------------------------------------------------
vtkMKVUtil::MetadataTrackMap vtkMKVReader::GetMetadataTracks()
{
  return this->Internal->MetadataTracks;
}

//----------------------------------------------------------------------------
vtkSmartPointer<vtkUnsignedCharArray> vtkMKVReader::GetRawVideoFrame(int videoTrackNumber, unsigned int frameNumber, double &timestampSec, bool &isKeyFrame)
{
  vtkMKVUtil::VideoTrackInfo* videoTrack = NULL;
  for (std::map <int, vtkMKVUtil::VideoTrackInfo>::iterator videoTrackIt = this->Internal->VideoTracks.begin(); videoTrackIt != this->Internal->VideoTracks.end(); ++videoTrackIt)
  {
    if (videoTrackIt->first == videoTrackNumber)
    {
      videoTrack = &videoTrackIt->second;
      break;
    }
  }

  if (!videoTrack)
  {
    vtkErrorMacro("Could not find requested video track: " << videoTrackNumber);
    return NULL;
  }

  if (frameNumber >= videoTrack->Frames.size())
  {
    vtkErrorMacro("Requested frame is out of range: " << videoTrack->Frames.size())
      return NULL;
  }

  vtkMKVUtil::FrameInfo frame = videoTrack->Frames[frameNumber];
  timestampSec = frame.TimestampSeconds;
  isKeyFrame = frame.IsKey;
  return frame.Data;
}

//----------------------------------------------------------------------------
void vtkMKVReader::Close()
{
  this->Internal->MKVReader->Close();
  if (this->Internal->MKVReadSegment)
  {
    delete this->Internal->MKVReadSegment;
    this->Internal->MKVReadSegment = NULL;
  }

  if (this->Internal->EBMLHeader)
  {
    delete this->Internal->EBMLHeader;
    this->Internal->EBMLHeader = NULL;
  }

}

//---------------------------------------------------------------------------
bool vtkMKVReader::CanReadFile(std::string filename)
{
  std::string extension = vtksys::SystemTools::GetFilenameExtension(filename);
  extension = vtksys::SystemTools::LowerCase(extension);

  if (extension == ".mkv")
  {
    return true;
  }
  else if (extension == ".webm")
  {
    return true;
  }

  return false;
}

//---------------------------------------------------------------------------
void vtkMKVReader::PrintSelf(ostream& os, vtkIndent indent)
{
  Superclass::PrintSelf(os, indent);
}
