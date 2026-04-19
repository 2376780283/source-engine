//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: WebM Video Material
//
//=============================================================================

#ifndef WEBM_MATERIAL_H
#define WEBM_MATERIAL_H

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IFileSystem;
class IMaterialSystem;
class CWebMMaterial;

//-----------------------------------------------------------------------------
// Global interfaces
//-----------------------------------------------------------------------------
extern IFileSystem *g_pFileSystem;
extern IMaterialSystem *materials;

//-----------------------------------------------------------------------------
// WebM includes
//-----------------------------------------------------------------------------
extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "mkvparser/mkvreader.h"
#include "mkvparser/mkvparser.h"

#include "video/ivideoservices.h"
#include "video_macros.h"

#include "materialsystem/itexture.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/MaterialSystemUtil.h"

#include "tier1/utlvector.h"

// -----------------------------------------------------------------------------
// Texture regenerator - callback to get new movie pixels into the texture
// -----------------------------------------------------------------------------
class CWebMMaterialRGBTextureRegenerator : public ITextureRegenerator
{
public:
    CWebMMaterialRGBTextureRegenerator();
    ~CWebMMaterialRGBTextureRegenerator();

    void SetSourceImage(uint8_t *SrcImage, int nWidth, int nHeight);

    virtual void RegenerateTextureBits(ITexture *pTexture, IVTFTexture *pVTFTexture, Rect_t *pRect);
    virtual void Release();

private:
    uint8_t *m_SrcImage;
    int m_nSourceWidth;
    int m_nSourceHeight;
};

// -----------------------------------------------------------------------------
// Class used to play a WebM video onto a texture
// -----------------------------------------------------------------------------
class CWebMMaterial : public IVideoMaterial
{
public:
    CWebMMaterial();
    ~CWebMMaterial();

    static const int MAX_FILENAME_LEN = 255;
    static const int MAX_MATERIAL_NAME_LEN = 255;
    static const int TEXTURE_SIZE_ALIGNMENT = 8;

    bool Init(const char *pMaterialName, const char *pFileName, VideoPlaybackFlags_t flags);
    void Shutdown();

    virtual const char *GetVideoFileName();
    virtual VideoResult_t GetLastResult();

    virtual VideoFrameRate_t &GetVideoFrameRate();

    virtual bool HasAudio();

    virtual bool SetVolume(float fVolume);
    virtual float GetVolume();
    virtual void SetMuted(bool bMuteState);
    virtual bool IsMuted();

    virtual VideoResult_t SoundDeviceCommand(VideoSoundDeviceOperation_t operation, void *pDevice = nullptr, void *pData = nullptr);

    virtual bool IsVideoReadyToPlay();
    virtual bool IsVideoPlaying();
    virtual bool IsNewFrameReady();
    virtual bool IsFinishedPlaying();

    virtual bool StartVideo();
    virtual bool StopVideo();

    virtual void SetLooping(bool bLoopVideo);
    virtual bool IsLooping();

    virtual void SetPaused(bool bPauseState);
    virtual bool IsPaused();

    virtual float GetVideoDuration();
    virtual int GetFrameCount();

    virtual bool SetFrame(int FrameNum);
    virtual int GetCurrentFrame();

    virtual bool SetTime(float flTime);
    virtual float GetCurrentVideoTime();

    virtual bool Update();

    virtual IMaterial *GetMaterial();

    virtual void GetVideoTexCoordRange(float *pMaxU, float *pMaxV);
    virtual void GetVideoImageSize(int *pWidth, int *pHeight);

private:
    friend class CWebMMaterialRGBTextureRegenerator;

    void Reset();
    void SetWebMFileName(const char *pWebMFileName);
    VideoResult_t SetResult(VideoResult_t status);

    bool OpenWebMMovie(const char *pWebMFileName);
    void CloseWebMFile();

    bool CreateProceduralTexture(const char *pTextureName);
    void DestroyProceduralTexture();

    bool CreateProceduralMaterial(const char *pMaterialName);
    void DestroyProceduralMaterial();

    bool DecodeNextFrame();

    CWebMMaterialRGBTextureRegenerator m_TextureRegen;

    VideoResult_t m_LastResult;

    CMaterialReference m_Material;
    CTextureReference m_Texture;

    float m_TexCordU;
    float m_TexCordV;

    int m_VideoFrameWidth;
    int m_VideoFrameHeight;

    char *m_pFileName;
    VideoPlaybackFlags_t m_PlaybackFlags;

    bool m_bInitCalled;
    bool m_bMovieInitialized;
    bool m_bMoviePlaying;
    bool m_bMovieFinishedPlaying;
    bool m_bMoviePaused;
    bool m_bLoopMovie;

    bool m_bHasAudio;
    bool m_bMuted;

    float m_CurrentVolume;

    // WebM/FFmpeg stuff
    mkvparser::MkvReader *m_pMkvReader;
    mkvparser::Segment *m_pSegment;
    mkvparser::VideoTrack *m_pVideoTrack;

    AVFormatContext *m_pFormatContext;
    AVCodecContext *m_pCodecContext;
    AVPacket *m_pPacket;
    AVFrame *m_pFrame;
    AVFrame *m_pRGBFrame;

    int m_VideoStreamIndex;
    int64_t m_StartTime;
    double m_Duration;
    VideoFrameRate_t m_FrameRate;
    int m_FrameCount;

    int64_t m_CurrentFrameTimestamp;
    int m_CurrentFrameNumber;

    uint8_t *m_pRGBBuffer;
};

#endif // WEBM_MATERIAL_H