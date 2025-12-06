//========== Copyright (C) 2025, Team HL2SB++, All rights reserved. ===========//
//
// Purpose: HUD notification & hint panel
//
//===========================================================================//

#ifndef HUD_HINTS_H
#define HUD_HINTS_H
#ifdef _WIN32
#pragma once
#endif

// 必须先包含 STL（防止 DECLARE_HUDELEMENT 在不完整类型下实例化 vector<int>）
#include <string>
#include <vector>
#include <unordered_map>

// 再包含 VGUI
#include <vgui_controls/Panel.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/ImagePanel.h>
#include <vgui_controls/AnimationController.h>

#include <hud.h>

using namespace vgui;

enum NotifyType
{
    NOTIFY_GENERIC = 0,
    NOTIFY_ERROR,
    NOTIFY_UNDO,
    NOTIFY_HINT,
    NOTIFY_CLEANUP
};

class NoticePanel : public Panel
{
    DECLARE_CLASS_SIMPLE( NoticePanel, Panel );

public:
    NoticePanel( Panel *parent );
    virtual ~NoticePanel();

    void SetText( const char *text );
    void SetLegacyType( int t );
    void SetProgress( float frac );
    bool KillSelf();            // returns true if removed
    void Start( float length ); // start time/length

    virtual void PerformLayout() override;
    virtual void Paint() override;

    float fx = 0.0f, fy = 0.0f;
    float VelX = 0.0f, VelY = 0.0f;
    float StartTime = 0.0f;
    float Length = -1.0f; // seconds; negative = infinite
    bool  Progress = false;
    float ProgressFrac = 0.0f;

private:
    Label       *m_hint_pLabel = nullptr;
    ImagePanel  *m_hint_pImage = nullptr;
    int          m_nType = 0;
};

#endif // HUD_HINTS_H