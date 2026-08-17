#ifndef HANDS_MENU_H
#define HANDS_MENU_H

#include "vgui_controls/Frame.h"
#include "vgui_controls/ComboBox.h"
#include "vgui_controls/CheckButton.h"
#include "vgui_controls/Button.h"
#include "utlvector.h"
#include "utlstring.h"

class CHandsMenu : public vgui::Frame
{
    DECLARE_CLASS_SIMPLE( CHandsMenu, vgui::Frame );

public:
    CHandsMenu( vgui::Panel *parent );
    virtual ~CHandsMenu() {}

protected:
    virtual void OnCommand( const char *command );
    virtual void PerformLayout();

private:
    void LoadModels();
    void LoadFromPath( const char *pszFilePath, const char *pszModTag = NULL );
    bool IsPathLoaded( const char *pszPath ) const;

    vgui::ComboBox *m_pComboBox;
    vgui::Button *m_pApplyButton;
    vgui::CheckButton *m_pEnableCheck;

    CUtlVector<CUtlString> m_LoadedPaths;
};

#endif // HANDS_MENU_H