#include "cbase.h"
#include "hands_menu.h"
#include "filesystem.h"
#include "KeyValues.h"
#include "vgui/ISurface.h"
#include "tier1/fmtstr.h"
#include "vgui_controls/Controls.h"

using namespace vgui;

// 静态指针管理菜单实例
static CHandsMenu *g_pHandsMenu = NULL;

CHandsMenu::CHandsMenu( Panel *parent ) : BaseClass( parent, "CHandsMenu" )
{
    SetSize( 300, 150 );
    SetTitle( "Select Hand Model", false );
    SetMoveable( false );
    SetSizeable( false );
    SetPaintBackgroundEnabled( true );

    m_pComboBox = new ComboBox( this, "ModelCombo", 5, false );
    m_pComboBox->SetBounds( 20, 40, 260, 25 );

    m_pApplyButton = new Button( this, "ApplyBtn", "Apply", this, "apply" );
    m_pApplyButton->SetBounds( 100, 90, 100, 30 );

    LoadModels();
}

void CHandsMenu::LoadModels()
{
    KeyValues *kv = new KeyValues( "HandModels" );
    if ( kv->LoadFromFile( filesystem, "scripts/handmodels.txt" ) )
    {
        for ( KeyValues *sub = kv->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey() )
        {
            const char *name = sub->GetString( "name" );
            const char *path = sub->GetString( "path" );
            
            KeyValues *data = new KeyValues( "data" );
            data->SetString( "path", path );
            m_pComboBox->AddItem( name, data );
        }
    }
    kv->deleteThis();
}

void CHandsMenu::OnCommand( const char *command )
{
    if ( Q_stricmp( command, "apply" ) == 0 )
    {
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
    // 居中显示
    int screenW, screenH;
    surface()->GetScreenSize( screenW, screenH );
    SetPos( (screenW - GetWide()) / 2, (screenH - GetTall()) / 2 );
}

// 控制台指令实现
void CC_ToggleHandsMenu()
{
    if ( !g_pHandsMenu )
    {
        // 传入 NULL 作为父面板通常会自动使用根面板
        g_pHandsMenu = new CHandsMenu( NULL );
    }

    if ( g_pHandsMenu->IsVisible() )
    {
        g_pHandsMenu->SetVisible( false );
        engine->ClientCmd( "hideconsole" ); // 可选：关闭菜单时隐藏控制台
    }
    else
    {
        g_pHandsMenu->SetVisible( true );
        g_pHandsMenu->Activate();
    }
}

static ConCommand toggle_handsmenu( "toggle_handsmenu", CC_ToggleHandsMenu, "Toggle the hand model selection menu" );
