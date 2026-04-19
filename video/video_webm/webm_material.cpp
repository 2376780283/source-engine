//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: WebM Video Material Implementation
//
//=============================================================================

#include "filesystem.h"
#include "tier1/strtools.h"
#include "tier1/utllinkedlist.h"
#include "tier1/KeyValues.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"
#include "materialsystem/itexture.h"
#include "vtf/vtf.h"
#include "pixelwriter.h"
#include "tier3/tier3.h"
#include "platform.h"

#include "webm_material.h"

#include "tier0/memdbgon.h"
#include "materialsystem/MaterialSystemUtil.h"

extern "C" {
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

// ===========================================================================
// CWebMMaterialRGBTextureRegenerator
// ===========================================================================
CWebMMaterialRGBTextureRegenerator::CWebMMaterialRGBTextureRegenerator() :
m_SrcImage(nullptr),
m_nSourceWidth(0),
m_nSourceHeight(0)
{
}

CWebMMaterialRGBTextureRegenerator::~CWebMMaterialRGBTextureRegenerator()
{
}

void CWebMMaterialRGBTextureRegenerator::SetSourceImage(uint8_t *SrcImage, int nWidth, int nHeight)
{
    m_SrcImage = SrcImage;
    m_nSourceWidth = nWidth;
    m_nSourceHeight = nHeight;
}

void CWebMMaterialRGBTextureRegenerator::RegenerateTextureBits(ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect)
{
    AssertExit(pVTFTexture != nullptr);

    if ((pVTFTexture->FrameCount() > 1) || (pVTFTexture->FaceCount() > 1) || (pVTFTexture->MipCount() > 1) || (pVTFTexture->Depth() > 1))
    {
        WarningAssert("Texture Properties Incorrect");
        memset(pVTFTexture->ImageData(), 0xAA, pVTFTexture->ComputeTotalSize());
        return;
    }

    if (m_SrcImage == nullptr)
    {
        WarningAssert("Video texture source not set");
        memset(pVTFTexture->ImageData(), 0xCC, pVTFTexture->ComputeTotalSize());
        return;
    }

#ifdef ANDROID
    Assert(pVTFTexture->Format() == IMAGE_FORMAT_BGR888);
#else
    Assert(pVTFTexture->Format() == IMAGE_FORMAT_BGR888);
#endif
    Assert(pVTFTexture->RowSizeInBytes(0) >= pVTFTexture->Width() * 3);
    Assert(pVTFTexture->Width() >= m_nSourceWidth);
    Assert(pVTFTexture->Height() >= m_nSourceHeight);

    uint8_t *pImageData = pVTFTexture->ImageData();
    int dstStride = pVTFTexture->RowSizeInBytes(0);
    uint8_t *pSrcData = m_SrcImage;
    int srcStride = m_nSourceWidth * 3;
    int rowSize = m_nSourceWidth * 3;

    for (int y = 0; y < m_nSourceHeight; y++)
    {
        memcpy(pImageData, pSrcData, rowSize);
        pImageData += dstStride;
        pSrcData += srcStride;
    }
}

void CWebMMaterialRGBTextureRegenerator::Release()
{
    // nothing to do
}

// ===========================================================================
// CWebMMaterial
// ===========================================================================
CWebMMaterial::CWebMMaterial() :
m_pFileName(nullptr),
m_bInitCalled(false),
m_bMovieInitialized(false),
m_bMoviePlaying(false),
m_bMovieFinishedPlaying(false),
m_bMoviePaused(false),
m_bLoopMovie(false),
m_bHasAudio(false),
m_bMuted(false),
m_CurrentVolume(1.0f),
m_pMkvReader(nullptr),
m_pSegment(nullptr),
m_pVideoTrack(nullptr),
m_pFormatContext(nullptr),
m_pCodecContext(nullptr),
m_pPacket(nullptr),
m_pFrame(nullptr),
m_pRGBFrame(nullptr),
m_VideoStreamIndex(-1),
m_StartTime(0),
m_Duration(0),
m_FrameRate(VideoFrameRate_t(30.0f)),
m_FrameCount(0),
m_CurrentFrameTimestamp(0),
m_CurrentFrameNumber(0),
m_pRGBBuffer(nullptr),
m_LastResult(VideoResult::SUCCESS)
{
    m_VideoFrameWidth = 0;
    m_VideoFrameHeight = 0;
    m_TexCordU = 1.0f;
    m_TexCordV = 1.0f;
}

CWebMMaterial::~CWebMMaterial()
{
    Shutdown();
}

bool CWebMMaterial::Init(const char *pMaterialName, const char *pFileName, VideoPlaybackFlags_t flags)
{
    if (pMaterialName == nullptr && pFileName == nullptr)
        return false;

    Shutdown();

    m_PlaybackFlags = flags;
    m_bInitCalled = true;

    SetWebMFileName(pFileName);

    if (!OpenWebMMovie(pFileName))
    {
        m_LastResult = VideoResult::VIDEO_ERROR_OCCURED;
        return false;
    }

    CreateProceduralTexture(pMaterialName);
    CreateProceduralMaterial(pMaterialName);

    m_bMovieInitialized = true;

    // Start movie playback (same as bink implementation)
    if ( !BITFLAGS_SET( m_PlaybackFlags, VideoPlaybackFlags::DONT_AUTO_START_VIDEO ) )
        StartVideo();

    m_LastResult = VideoResult::SUCCESS;
    return true;
}

void CWebMMaterial::Shutdown()
{
    if (!m_bInitCalled)
        return;

    CloseWebMFile();
    DestroyProceduralTexture();
    DestroyProceduralMaterial();

    Reset();

    m_bInitCalled = false;
    m_bMovieInitialized = false;
}

void CWebMMaterial::Reset()
{
    if (m_pFileName)
    {
        delete[] m_pFileName;
        m_pFileName = nullptr;
    }

    m_bMoviePlaying = false;
    m_bMovieFinishedPlaying = false;
    m_bMoviePaused = false;
    m_bLoopMovie = false;
    m_bHasAudio = false;
    m_bMuted = false;
    m_CurrentVolume = 1.0f;

    m_CurrentFrameTimestamp = 0;
    m_CurrentFrameNumber = 0;
}

void CWebMMaterial::SetWebMFileName(const char *pWebMFileName)
{
    if (m_pFileName)
    {
        delete[] m_pFileName;
    }

    size_t len = V_strlen(pWebMFileName) + 1;
    m_pFileName = new char[len];
    V_strncpy(m_pFileName, pWebMFileName, len);
}

VideoResult_t CWebMMaterial::SetResult(VideoResult_t status)
{
    m_LastResult = status;
    return status;
}

bool CWebMMaterial::OpenWebMMovie(const char *pWebMFileName)
{
    // Open WebM file using libwebm
    m_pMkvReader = new mkvparser::MkvReader();
    int result = m_pMkvReader->Open(pWebMFileName);
    if (result != 0)
    {
        Warning("Failed to open WebM file: %s\n", pWebMFileName);
        delete m_pMkvReader;
        m_pMkvReader = nullptr;
        return false;
    }

    mkvparser::EBMLHeader ebmlHeader;
    long long pos = 0;
    result = ebmlHeader.Parse(m_pMkvReader, pos);
    if (result != 0)
    {
        Warning("Failed to parse EBML header\n");
        CloseWebMFile();
        return false;
    }

    result = mkvparser::Segment::CreateInstance(m_pMkvReader, pos, m_pSegment);
    if (result != 0)
    {
        Warning("Failed to create segment\n");
        CloseWebMFile();
        return false;
    }

    result = m_pSegment->Load();
    if (result != 0)
    {
        Warning("Failed to load segment\n");
        CloseWebMFile();
        return false;
    }

    // Get video track
    const mkvparser::Tracks *pTracks = m_pSegment->GetTracks();
    if (!pTracks)
    {
        Warning("No tracks found\n");
        CloseWebMFile();
        return false;
    }

    for (unsigned int i = 0; i < pTracks->GetTracksCount(); i++)
    {
        const mkvparser::Track *pTrack = pTracks->GetTrackByIndex(i);
        if (pTrack && pTrack->GetType() == mkvparser::Track::kVideo)
        {
            m_pVideoTrack = (mkvparser::VideoTrack *)pTrack;
            break;
        }
    }

    if (!m_pVideoTrack)
    {
        Warning("No video track found\n");
        CloseWebMFile();
        return false;
    }

    // Get video info
    m_VideoFrameWidth = (int)m_pVideoTrack->GetWidth();
    m_VideoFrameHeight = (int)m_pVideoTrack->GetHeight();
    m_FrameRate = VideoFrameRate_t(m_pVideoTrack->GetFrameRate());
    if (m_FrameRate.GetFPS() <= 0)
        m_FrameRate = VideoFrameRate_t(30.0f);

    // Calculate duration
    const mkvparser::SegmentInfo* info = m_pSegment->GetInfo();
    if (info)
    {
        m_Duration = (double)info->GetDuration() / 1000000000.0;  // Convert ns to seconds
    }
    else
    {
        m_Duration = 0;
    }
    m_FrameCount = (int)(m_Duration * m_FrameRate.GetFPS());

    // Initialize FFmpeg for decoding
    avformat_open_input(&m_pFormatContext, pWebMFileName, nullptr, nullptr);
    if (!m_pFormatContext)
    {
        Warning("Failed to open format context\n");
        CloseWebMFile();
        return false;
    }

    // Find video stream
    m_VideoStreamIndex = -1;
    for (unsigned int i = 0; i < m_pFormatContext->nb_streams; i++)
    {
        if (m_pFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            m_VideoStreamIndex = i;
            break;
        }
    }

    if (m_VideoStreamIndex < 0)
    {
        Warning("No video stream found in FFmpeg\n");
        CloseWebMFile();
        return false;
    }

    // Find decoder
    const AVCodec *pCodec = avcodec_find_decoder(m_pFormatContext->streams[m_VideoStreamIndex]->codecpar->codec_id);
    if (!pCodec)
    {
        Warning("Failed to find decoder\n");
        CloseWebMFile();
        return false;
    }

    // Allocate codec context
    m_pCodecContext = avcodec_alloc_context3(pCodec);
    if (!m_pCodecContext)
    {
        Warning("Failed to allocate codec context\n");
        CloseWebMFile();
        return false;
    }

    // Copy codec parameters
    avcodec_parameters_to_context(m_pCodecContext, m_pFormatContext->streams[m_VideoStreamIndex]->codecpar);

    // Open codec
    if (avcodec_open2(m_pCodecContext, pCodec, nullptr) < 0)
    {
        Warning("Failed to open codec\n");
        CloseWebMFile();
        return false;
    }

    // Allocate frames and packet
    m_pFrame = av_frame_alloc();
    m_pRGBFrame = av_frame_alloc();
    m_pPacket = av_packet_alloc();

    if (!m_pFrame || !m_pRGBFrame || !m_pPacket)
    {
        Warning("Failed to allocate frames\n");
        CloseWebMFile();
        return false;
    }

    // Allocate RGB buffer
    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, m_VideoFrameWidth, m_VideoFrameHeight, 1);
    m_pRGBBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(m_pRGBFrame->data, m_pRGBFrame->linesize, m_pRGBBuffer, AV_PIX_FMT_BGR24, m_VideoFrameWidth, m_VideoFrameHeight, 1);

    m_pRGBFrame->width = m_VideoFrameWidth;
    m_pRGBFrame->height = m_VideoFrameHeight;

    return true;
}

void CWebMMaterial::CloseWebMFile()
{
    if (m_pRGBBuffer)
    {
        av_free(m_pRGBBuffer);
        m_pRGBBuffer = nullptr;
    }

    if (m_pFrame)
    {
        av_frame_free(&m_pFrame);
        m_pFrame = nullptr;
    }

    if (m_pRGBFrame)
    {
        av_frame_free(&m_pRGBFrame);
        m_pRGBFrame = nullptr;
    }

    if (m_pPacket)
    {
        av_packet_free(&m_pPacket);
        m_pPacket = nullptr;
    }

    if (m_pCodecContext)
    {
        avcodec_free_context(&m_pCodecContext);
        m_pCodecContext = nullptr;
    }

    if (m_pFormatContext)
    {
        avformat_close_input(&m_pFormatContext);
        m_pFormatContext = nullptr;
    }

    if (m_pSegment)
    {
        delete m_pSegment;
        m_pSegment = nullptr;
    }

    if (m_pMkvReader)
    {
        m_pMkvReader->Close();
        delete m_pMkvReader;
        m_pMkvReader = nullptr;
    }

    m_pVideoTrack = nullptr;
}

bool CWebMMaterial::CreateProceduralTexture(const char *pTextureName)
{
    char textureName[MAX_MATERIAL_NAME_LEN];
    V_snprintf(textureName, sizeof(textureName), "%s_texture", pTextureName);

    // Round up to nearest block size
    int txWidth = (m_VideoFrameWidth + TEXTURE_SIZE_ALIGNMENT - 1) & ~(TEXTURE_SIZE_ALIGNMENT - 1);
    int txHeight = (m_VideoFrameHeight + TEXTURE_SIZE_ALIGNMENT - 1) & ~(TEXTURE_SIZE_ALIGNMENT - 1);

    // Initialize the procedural texture as 32-bit RGB, without mipmaps
    m_Texture.InitProceduralTexture(textureName, "VideoCacheTextures", txWidth, txHeight,
        IMAGE_FORMAT_BGR888, TEXTUREFLAGS_CLAMPS | TEXTUREFLAGS_CLAMPT | TEXTUREFLAGS_NOMIP |
        TEXTUREFLAGS_PROCEDURAL | TEXTUREFLAGS_SINGLECOPY | TEXTUREFLAGS_NOLOD);

    m_Texture->SetTextureRegenerator(&m_TextureRegen);
    m_TextureRegen.SetSourceImage(m_pRGBBuffer, m_VideoFrameWidth, m_VideoFrameHeight);

    // Set the max texture coordinate range to match the aspect ratio of the video
    m_TexCordU = (float)m_VideoFrameWidth / (float)txWidth;
    m_TexCordV = (float)m_VideoFrameHeight / (float)txHeight;

    return true;
}

void CWebMMaterial::DestroyProceduralTexture()
{
    if (m_Texture)
    {
        m_Texture->SetTextureRegenerator(nullptr);
        m_Texture.Shutdown();
    }
}

bool CWebMMaterial::CreateProceduralMaterial(const char *pMaterialName)
{
    KeyValues *pVMTKeyValues = new KeyValues("UnlitGeneric");
    pVMTKeyValues->SetString("$basetexture", m_Texture->GetName());
    pVMTKeyValues->SetString("$vertexcolor", "1");
    pVMTKeyValues->SetString("$no_fullbright", "1");
    pVMTKeyValues->SetInt("$ignorez", 1);
    pVMTKeyValues->SetInt("$decal", 1);

    m_Material.Init(materials->CreateMaterial(pMaterialName, pVMTKeyValues));

    if (!m_Material)
    {
        Warning("Failed to create video material\n");
        return false;
    }

    return true;
}

void CWebMMaterial::DestroyProceduralMaterial()
{
    m_Material = nullptr;
}

bool CWebMMaterial::DecodeNextFrame()
{
    if (!m_pSegment || !m_pVideoTrack || m_VideoStreamIndex < 0)
        return false;

    // Use FFmpeg to decode frames
    int ret = av_read_frame(m_pFormatContext, m_pPacket);
    if (ret < 0)
    {
        if (m_bLoopMovie)
        {
            av_seek_frame(m_pFormatContext, m_VideoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
            ret = av_read_frame(m_pFormatContext, m_pPacket);
            if (ret < 0)
                return false;
        }
        else
        {
            m_bMovieFinishedPlaying = true;
            return false;
        }
    }

    if (m_pPacket->stream_index != m_VideoStreamIndex)
    {
        av_packet_unref(m_pPacket);
        return DecodeNextFrame();
    }

    ret = avcodec_send_packet(m_pCodecContext, m_pPacket);
    if (ret < 0)
    {
        av_packet_unref(m_pPacket);
        return false;
    }

    while (ret >= 0)
    {
        ret = avcodec_receive_frame(m_pCodecContext, m_pFrame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_packet_unref(m_pPacket);
            return DecodeNextFrame();
        }
        else if (ret < 0)
        {
            av_packet_unref(m_pPacket);
            return false;
        }

        // Convert to BGR24
        SwsContext *swsCtx = sws_getContext(m_pCodecContext->width, m_pCodecContext->height, m_pCodecContext->pix_fmt,
                                              m_VideoFrameWidth, m_VideoFrameHeight, AV_PIX_FMT_BGR24,
                                              SWS_BILINEAR, nullptr, nullptr, nullptr);

        if (swsCtx)
        {
            sws_scale(swsCtx, (uint8_t const *const *)m_pFrame->data, m_pFrame->linesize, 0, m_pCodecContext->height,
                      m_pRGBFrame->data, m_pRGBFrame->linesize);
            sws_freeContext(swsCtx);
        }

        m_CurrentFrameNumber++;
        m_pRGBFrame->pts = m_pFrame->pts;

        // Update texture
        m_Texture->SetTextureRegenerator(&m_TextureRegen);
        m_Texture->Download();

        av_packet_unref(m_pPacket);
        return true;
    }

    av_packet_unref(m_pPacket);
    return false;
}

// ===========================================================================
// IVideoMaterial implementation
// ===========================================================================

const char *CWebMMaterial::GetVideoFileName()
{
    return m_pFileName ? m_pFileName : "";
}

VideoResult_t CWebMMaterial::GetLastResult()
{
    return m_LastResult;
}

VideoFrameRate_t &CWebMMaterial::GetVideoFrameRate()
{
    return m_FrameRate;
}

bool CWebMMaterial::HasAudio()
{
    return m_bHasAudio;
}

bool CWebMMaterial::SetVolume(float fVolume)
{
    m_CurrentVolume = fVolume;
    return true;
}

float CWebMMaterial::GetVolume()
{
    return m_CurrentVolume;
}

void CWebMMaterial::SetMuted(bool bMuteState)
{
    m_bMuted = bMuteState;
}

bool CWebMMaterial::IsMuted()
{
    return m_bMuted;
}

VideoResult_t CWebMMaterial::SoundDeviceCommand(VideoSoundDeviceOperation_t operation, void *pDevice, void *pData)
{
    return SetResult(VideoResult::OPERATION_NOT_SUPPORTED);
}

bool CWebMMaterial::IsVideoReadyToPlay()
{
    return m_bMovieInitialized;
}

bool CWebMMaterial::IsVideoPlaying()
{
    return m_bMoviePlaying;
}

bool CWebMMaterial::IsNewFrameReady()
{
    return m_bMoviePlaying && !m_bMoviePaused;
}

bool CWebMMaterial::IsFinishedPlaying()
{
    return m_bMovieFinishedPlaying;
}

bool CWebMMaterial::StartVideo()
{
    if (!m_bMovieInitialized)
        return false;

    m_bMoviePlaying = true;
    m_bMoviePaused = false;
    m_bMovieFinishedPlaying = false;
    m_CurrentFrameNumber = 0;

    // Seek to beginning
    if (m_pFormatContext)
    {
        av_seek_frame(m_pFormatContext, m_VideoStreamIndex, 0, AVSEEK_FLAG_BACKWARD);
    }

    return true;
}

bool CWebMMaterial::StopVideo()
{
    m_bMoviePlaying = false;
    m_bMoviePaused = false;
    return true;
}

void CWebMMaterial::SetLooping(bool bLoopVideo)
{
    m_bLoopMovie = bLoopVideo;
}

bool CWebMMaterial::IsLooping()
{
    return m_bLoopMovie;
}

void CWebMMaterial::SetPaused(bool bPauseState)
{
    m_bMoviePaused = bPauseState;
}

bool CWebMMaterial::IsPaused()
{
    return m_bMoviePaused;
}

float CWebMMaterial::GetVideoDuration()
{
    return (float)m_Duration;
}

int CWebMMaterial::GetFrameCount()
{
    return m_FrameCount;
}

bool CWebMMaterial::SetFrame(int FrameNum)
{
    if (!m_pFormatContext || m_VideoStreamIndex < 0)
        return false;

    int64_t targetTimestamp = (int64_t)((float)FrameNum / m_FrameRate.GetFPS() * AV_TIME_BASE);
    av_seek_frame(m_pFormatContext, m_VideoStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD);
    m_CurrentFrameNumber = FrameNum;

    return true;
}

int CWebMMaterial::GetCurrentFrame()
{
    return m_CurrentFrameNumber;
}

bool CWebMMaterial::SetTime(float flTime)
{
    if (!m_pFormatContext || m_VideoStreamIndex < 0)
        return false;

    int64_t targetTimestamp = (int64_t)(flTime * AV_TIME_BASE);
    av_seek_frame(m_pFormatContext, m_VideoStreamIndex, targetTimestamp, AVSEEK_FLAG_BACKWARD);
    m_CurrentFrameNumber = (int)(flTime * m_FrameRate.GetFPS());

    return true;
}

float CWebMMaterial::GetCurrentVideoTime()
{
    return (float)m_CurrentFrameNumber / m_FrameRate.GetFPS();
}

bool CWebMMaterial::Update()
{
    if (!m_bMoviePlaying || m_bMoviePaused || m_bMovieFinishedPlaying)
        return false;

    return DecodeNextFrame();
}

IMaterial *CWebMMaterial::GetMaterial()
{
    return m_Material;
}

void CWebMMaterial::GetVideoTexCoordRange(float *pMaxU, float *pMaxV)
{
    if (pMaxU)
        *pMaxU = m_TexCordU;
    if (pMaxV)
        *pMaxV = m_TexCordV;
}

void CWebMMaterial::GetVideoImageSize(int *pWidth, int *pHeight)
{
    if (pWidth)
        *pWidth = m_VideoFrameWidth;
    if (pHeight)
        *pHeight = m_VideoFrameHeight;
}