//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: Cross-platform ChangeGameDialog
//
//=============================================================================//

#include <vgui_controls/Frame.h>
#include <vgui_controls/ListPanel.h>
#include <KeyValues.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>        // <-- 必须
#include <algorithm>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

#include "ModInfo.h"       // CModInfo 类
#include "EngineInterface.h"  // engine 全局指针

extern IVEngineClient* engine;

#include <tier0/memdbgon.h>

using namespace vgui;

class CChangeGameDialog : public Frame
{
    DECLARE_CLASS_SIMPLE(CChangeGameDialog, Frame);

public:
    CChangeGameDialog(Panel* parent);
    virtual ~CChangeGameDialog();

    virtual void OnCommand(const char* command) override;

private:
    void LoadModList();
    ListPanel* m_pModList;
};

//-----------------------------------------------------------------------------
// Constructor
//-----------------------------------------------------------------------------
CChangeGameDialog::CChangeGameDialog(Panel* parent) : Frame(parent, "ChangeGameDialog")
{
    SetSize(400, 340);
    SetMinimumSize(400, 340);
    SetTitle("#GameUI_ChangeGame", true);

    m_pModList = new ListPanel(this, "ModList");
    m_pModList->SetEmptyListText("#GameUI_NoOtherGamesAvailable");
    m_pModList->AddColumnHeader(0, "ModName", "#GameUI_Game", 128);

    LoadModList();
    LoadControlSettings("Resource/ChangeGameDialog.res");

    if (m_pModList->GetItemCount() > 0)
        m_pModList->SetSingleSelectedItem(m_pModList->GetItemIDFromRow(0));
}

//-----------------------------------------------------------------------------
// Destructor
//-----------------------------------------------------------------------------
CChangeGameDialog::~CChangeGameDialog()
{
}

//-----------------------------------------------------------------------------
// Load mods from filesystem (cross-platform)
//-----------------------------------------------------------------------------
void CChangeGameDialog::LoadModList()
{
#if defined(_WIN32) || defined(_WIN64)
    char szSearchPath[_MAX_PATH + 5];
    strcpy(szSearchPath, "*.*");

    WIN32_FIND_DATA wfd;
    HANDLE hResult = FindFirstFile(szSearchPath, &wfd);
    if (hResult != INVALID_HANDLE_VALUE)
    {
        BOOL bMoreFiles;
        do
        {
            if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                strncmp(wfd.cFileName, ".", 1) != 0)
            {
                char szGameInfo[MAX_PATH];
                snprintf(szGameInfo, sizeof(szGameInfo), "%s\\gameinfo.txt", wfd.cFileName);

                FILE* f = fopen(szGameInfo, "rb");
                if (f)
                {
                    fseek(f, 0, SEEK_END);
                    long size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    char* buf = (char*)malloc(size + 1);
                    if (fread(buf, 1, size, f) == (size_t)size)
                    {
                        buf[size] = 0;
                        CModInfo modInfo;
                        modInfo.LoadGameInfoFromBuffer(buf);
                        if (strcmp(modInfo.GetGameName(), ModInfo().GetGameName()) != 0)
                        {
                            strlwr(wfd.cFileName);
                            KeyValues* itemData = new KeyValues("Mod");
                            itemData->SetString("ModName", modInfo.GetGameName());
                            itemData->SetString("ModDir", wfd.cFileName);
                            m_pModList->AddItem(itemData, 0, false, false);
                        }
                    }
                    free(buf);
                    fclose(f);
                }
            }
            bMoreFiles = FindNextFile(hResult, &wfd);
        } while (bMoreFiles);
        FindClose(hResult);
    }

#else
    const char* scanPath = ".";
    DIR* dir = opendir(scanPath);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        std::string fullPath = std::string(scanPath) + "/" + entry->d_name;

        struct stat st;
        if (stat(fullPath.c_str(), &st) != 0) continue;
        if (!S_ISDIR(st.st_mode)) continue;

        std::string gameinfoPath = fullPath + "/gameinfo.txt";
        FILE* f = fopen(gameinfoPath.c_str(), "rb");
        if (!f) continue;

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char* buf = (char*)malloc(size + 1);
        if (fread(buf, 1, size, f) != (size_t)size) { free(buf); fclose(f); continue; }
        buf[size] = 0;

        CModInfo modInfo;
        modInfo.LoadGameInfoFromBuffer(buf);

        free(buf);
        fclose(f);

        if (strcmp(modInfo.GetGameName(), ModInfo().GetGameName()) != 0)
        {
            std::string modDir = entry->d_name;
            std::transform(modDir.begin(), modDir.end(), modDir.begin(), ::tolower);

            KeyValues* itemData = new KeyValues("Mod");
            itemData->SetString("ModName", modInfo.GetGameName());
            itemData->SetString("ModDir", modDir.c_str());
            m_pModList->AddItem(itemData, 0, false, false);
        }
    }

    closedir(dir);
#endif
}

//-----------------------------------------------------------------------------
// Handle dialog commands
//-----------------------------------------------------------------------------
void CChangeGameDialog::OnCommand(const char* command)
{
    if (!stricmp(command, "OK"))
    {
        if (m_pModList->GetSelectedItemsCount() > 0)
        {
            KeyValues* kv = m_pModList->GetItem(m_pModList->GetSelectedItem(0));
            if (kv)
            {
                char szCmd[256];
                Q_snprintf(szCmd, sizeof(szCmd), "_setgamedir %s\n", kv->GetString("ModDir"));
                engine->ClientCmd_Unrestricted(szCmd);
                engine->ClientCmd_Unrestricted("_restart\n");
            }
        }
    }
    else if (!stricmp(command, "Cancel"))
    {
        Close();
    }
    else
    {
        BaseClass::OnCommand(command);
    }
}