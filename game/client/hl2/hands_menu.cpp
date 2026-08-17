#include "cbase.h"
#include "hands_menu.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "vgui/ISurface.h"
#include "tier1/fmtstr.h"
#include "tier1/strtools.h"
#include "vgui_controls/Controls.h"

using namespace vgui;

static CHandsMenu *g_pHandsMenu = NULL;

extern ConVar c_hand_enabled;

CHandsMenu::CHandsMenu( Panel *parent ) : BaseClass( parent, "CHandsMenu" )
{
    SetSize( 300, 180 );
    SetTitle( "Select Hand Model", false );
    SetMoveable( false );
    SetSizeable( false );
    SetPaintBackgroundEnabled( true );

    m_pComboBox = new ComboBox( this, "ModelCombo", 5, false );
    m_pComboBox->SetBounds( 20, 40, 260, 25 );

    m_pApplyButton = new Button( this, "ApplyBtn", "Apply", this, "apply" );
    m_pEnableCheck = new CheckButton( this, "EnableCheck", "Enable Custom Hand Model" );
    m_pEnableCheck->SetBounds( 20, 90, 260, 20 );
    m_pEnableCheck->SetSelected( c_hand_enabled.GetBool() );
    m_pApplyButton->SetBounds( 100, 120, 100, 30 );

    LoadModels();
}

//-----------------------------------------------------------------------------
// purpose: 检查该模型路径是否已加载过（防止多个 mod 配置重复）
//-----------------------------------------------------------------------------
bool CHandsMenu::IsPathLoaded( const char *pszPath ) const
{
    for ( int i = 0; i < m_LoadedPaths.Count(); i++ )
    {
        if ( Q_stricmp( m_LoadedPaths[i].String(), pszPath ) == 0 )
            return true;
    }
    return false;
}

//-----------------------------------------------------------------------------
// purpose: 从指定文件路径加载并添加到下拉框
// pszModTag: 用于 UI 标注来源，如 "mod1"
//-----------------------------------------------------------------------------
void CHandsMenu::LoadFromPath( const char *pszFilePath, const char *pszModTag )
{
    KeyValues *kv = new KeyValues( "HandModels" );
    if ( !kv->LoadFromFile( filesystem, pszFilePath ) )
    {
        kv->deleteThis();
        return;
    }
    for ( KeyValues *sub = kv->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
    {
        const char *name = sub->GetString( "name" );
        const char *path = sub->GetString( "path" );
        
        if ( !path || !path[0] )
            continue;
        if ( IsPathLoaded( path ) )
            continue;

        m_LoadedPaths.AddToTail( path );
        char displayName[128];
        if ( pszModTag && pszModTag[0] )
        {
            V_snprintf( displayName, sizeof(displayName), "%s [%s]", name, pszModTag );
        }
        else
        {
            V_strncpy( displayName, name, sizeof(displayName) );
        }
        KeyValues *data = new KeyValues( "data" );
        data->SetString( "path", path );
        m_pComboBox->AddItem( displayName, data );
    }

    kv->deleteThis();
}

//-----------------------------------------------------------------------------
// purpose: 从 gameinfo.txt 配置的 SearchPaths 中自动发现所有 handmodels.txt
//-----------------------------------------------------------------------------
void CHandsMenu::LoadModels()
{
    m_LoadedPaths.RemoveAll();
    m_pComboBox->DeleteAllItems();

    char searchPaths[4096];
    filesystem->GetSearchPath( "GAME", false, searchPaths, sizeof(searchPaths) );

    char *token = strtok( searchPaths, ";" );
    while ( token != NULL )
    {
        while ( *token == ' ' || *token == '\t' ) token++;        
        char pathBuf[MAX_PATH];
        V_strncpy( pathBuf, token, sizeof(pathBuf) );
        
        int len = V_strlen( pathBuf );
        while ( len > 0 && (pathBuf[len-1] == ' ' || pathBuf[len-1] == '\t') )
            pathBuf[--len] = '\0';

        if ( len > 0 )
        {
            V_FixSlashes( pathBuf );

            char configPath[MAX_PATH];
            V_snprintf( configPath, sizeof(configPath), "%s/scripts/handmodels.txt", pathBuf );

            if ( filesystem->FileExists( configPath, NULL ) )
            {
                const char *modTag = NULL;
                char modNameBuf[64] = {0};

                char *scriptsPos = V_stristr( pathBuf, "/scripts" );
                if ( !scriptsPos )
                    scriptsPos = V_stristr( pathBuf, "\\scripts" );
                
                if ( scriptsPos )
                {
                    *scriptsPos = '\0';
                    char *lastSlash = V_strrchr( pathBuf, '/' );
                    if ( !lastSlash )
                        lastSlash = V_strrchr( pathBuf, '\\' );
                    
                    if ( lastSlash && V_strlen( lastSlash + 1 ) > 0 )
                    {
                        V_strncpy( modNameBuf, lastSlash + 1, sizeof(modNameBuf) );
                        modTag = modNameBuf;
                    }
                    *scriptsPos = '/'; // restore
                }
                LoadFromPath( configPath, modTag );
            }
        }

        token = strtok( NULL, ";" );
    }
    if ( m_pComboBox->GetItemCount() > 0 )
    {
        m_pComboBox->ActivateItem( 0 );
    }
}

void CHandsMenu::OnCommand( const char *command )
{
    if ( Q_stricmp( command, "apply" ) == 0 )
    {
        c_hand_enabled.SetValue( m_pEnableCheck->IsSelected() ? 1 : 0 );
        KeyValues *kv = m_pComboBox->GetActiveItemUserData();
        if ( kv )
        {
            const char *path = kv->GetString( "path" );
            if ( path && path[0] )
            {
                engine->ClientCmd( CFmtStr( "c_hand %s", path ) );
                Close();
            }
        }
    }
    else
    {
        BaseClass::OnCommand( command );
    }
}

void CHandsMenu::PerformLayout()
{
    BaseClass::PerformLayout();
    int screenW, screenH;
    surface()->GetScreenSize( screenW, screenH );
    SetPos( (screenW - GetWide()) / 2, (screenH - GetTall()) / 2 );
}

void CC_ToggleHandsMenu()
{
    if ( !g_pHandsMenu )
    {
        g_pHandsMenu = new CHandsMenu( NULL );
    }

    if ( g_pHandsMenu->IsVisible() )
    {
        g_pHandsMenu->SetVisible( false );
        engine->ClientCmd( "hideconsole" );
    }
    else
    {
        g_pHandsMenu->SetVisible( true );
        g_pHandsMenu->Activate();
    }
}

static ConCommand toggle_handsmenu( "toggle_handsmenu", CC_ToggleHandsMenu, "Toggle the hand model selection menu" );
