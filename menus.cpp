#include "menus.h"
#include "admin_system.h"
#include "CCSPlayerController.h"

extern IMenusApi* g_pMenus;
extern IUtilsApi* g_pUtils;
extern IMySQLClient *g_pMysqlClient;
extern IMySQLConnection* g_pConnection;
extern const char* g_szDatabasePrefix;
extern std::unordered_map<std::string, Category> mCategories;
extern AdminApi* g_pAdminApi;
extern IPlayersApi* g_pPlayers;
extern IVEngineServer2* engine;
extern std::map<std::string, std::string> g_vecPhrases;
extern ISmmAPI *g_SMAPI;
extern ISmmPlugin *g_PLAPI;

extern bool HasAccessInItem(int iSlot, const char* szCategory, const char* szItem);
extern bool HasAccessInCategory(int iSlot, const char* szCategory);
extern int g_iImmunityType;
extern int g_iUnpunishType;
extern Admin g_pAdmins[64];
extern int g_iUnpunishOfflineCount;

extern bool g_bOwnReasons;
extern bool g_bOwnReason[64];
extern std::string g_szOwnReason[64];
extern uint64 g_iTarget[64];
extern int g_iTargetType[64];
extern bool g_bTargetOffline[64];

extern int g_iTime_Reason_Type;
extern uint64 g_iAdminPunish[64][4];

//if g_iTime_Reason_Type = 0
extern std::vector<std::string> g_vReasons[4];
extern std::vector<std::pair<int, std::string>> g_mTimes[4];
//if g_iTime_Reason_Type = 1
extern std::vector<std::pair<std::string, std::vector<std::pair<int, std::string>>>> g_mReasons[4];;

extern std::vector<std::pair<uint64, OfflineUser>> g_mOfflineUsers;

extern std::map<std::string, std::vector<std::string>> g_mSortItems;
extern std::vector<std::string> g_vSortCategories;

std::string g_szLastCategory[64];

void ShowPunishOfflineMenu(int iSlot, int iType);
void ShowReasonsMenu(int iSlot, uint64 iTarget, int iType, bool bOffline);

void ShowTimesMenu(int iSlot, uint64 iTarget, int iType, std::string szReason, bool bOffline)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectTime"].c_str());
    int iCount = 0;
    if(g_iTime_Reason_Type)
    {
        for (const auto& reason : g_mReasons[iType]) {
            if (reason.first == szReason) {
                for (const auto& time : reason.second) {
                    g_pMenus->AddItemMenu(hMenu, std::to_string(time.first).c_str(), time.second.c_str());
                    iCount++;
                }
                break;
            }
        }
    }
    else
    {
        for (const auto& time : g_mTimes[iType]) {
            g_pMenus->AddItemMenu(hMenu, std::to_string(time.first).c_str(), time.second.c_str());
            iCount++;
        }
    }
    if(iCount == 0)
    {
        g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["NoTimes"].c_str(), ITEM_DISABLED);
    }
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [iType, iTarget, szReason, bOffline](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            std::string sReason = szReason;
            if(szReason == "own")
                sReason = g_szOwnReason[iSlot];
            if(bOffline)
            {
                auto it = std::find_if(g_mOfflineUsers.begin(), g_mOfflineUsers.end(), [iTarget](const std::pair<uint64, OfflineUser>& p) {
                    return p.first == iTarget;
                });
                std::string szName = "";
                if(it != g_mOfflineUsers.end()) {
                    it->second.bPunished[iType] = true;
                    it->second.iAdminID[iType] = g_pPlayers->GetSteamID64(iSlot);
                    szName = it->second.szName;
                }
                g_pAdminApi->AddOfflinePlayerPunishment(std::to_string(iTarget).c_str(), szName.c_str(), iType, std::atoi(szBack), sReason.c_str(), iSlot);
            }
            else
                g_pAdminApi->AddPlayerPunishment(iTarget, iType, std::atoi(szBack), sReason.c_str(), iSlot, true, true);
            g_pMenus->ClosePlayerMenu(iSlot);
        }
        else if(iItem == 7)
        {
            ShowReasonsMenu(iSlot, iTarget, iType, bOffline);
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void ShowReasonsMenu(int iSlot, uint64 iTarget, int iType, bool bOffline)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectReason"].c_str());
    if(g_iTime_Reason_Type)
    {
        for (const auto& reason : g_mReasons[iType]) {
            if(reason.first == "own") {
                if(g_bOwnReasons && g_pAdminApi->HasPermission(iSlot, "@admin/own_reason")) {
                    g_pMenus->AddItemMenu(hMenu, "own", g_vecPhrases["Item_OwnReason"].c_str());
                }
            }
            else g_pMenus->AddItemMenu(hMenu, reason.first.c_str(), reason.first.c_str());
        }
    }
    else
    {
        if(g_bOwnReasons && g_pAdminApi->HasPermission(iSlot, "@admin/own_reason"))
        {
            g_pMenus->AddItemMenu(hMenu, "own", g_vecPhrases["Item_OwnReason"].c_str());
        }
        for (const auto& reason : g_vReasons[iType]) {
            g_pMenus->AddItemMenu(hMenu, reason.c_str(), reason.c_str());
        }
    }
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [iType, iTarget, bOffline](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            if(std::string(szBack) == "own")
            {
                g_iTarget[iSlot] = iTarget;
                g_iTargetType[iSlot] = iType;
                g_bTargetOffline[iSlot] = bOffline;

                g_bOwnReason[iSlot] = true;
                g_pMenus->ClosePlayerMenu(iSlot);
                g_pUtils->PrintToChat(iSlot, g_vecPhrases["EnterOwnReason"].c_str());

                return;
            }
            ShowTimesMenu(iSlot, iTarget, iType, szBack, bOffline);
        }
        else if(iItem == 7)
        {
            if(bOffline) ShowPunishOfflineMenu(iSlot, iType);
            else ShowPunishMenu(iSlot, iType);
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void ShowPunishMenu(int iSlot, int iType)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectPlayer"].c_str());
    bool bFound = false;
    int iImmunity = g_pAdminApi->GetAdminImmunity(iSlot);
    switch (g_iImmunityType)
    {
    case 0:
        for (int i = 0; i < 64; i++)
        {
            CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
            if(!pPlayer || pPlayer->m_steamID == 0) continue;
            CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
            if(!pPawn) continue;
            if(g_pAdminApi->IsPlayerPunished(i, iType)) continue;
            g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pMenus->escapeString(engine->GetClientConVarValue(i, "name")).c_str());
            bFound = true;
        }
        break;
    case 1:
        for (int i = 0; i < 64; i++)
        {
            CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
            if(!pPlayer || pPlayer->m_steamID == 0) continue;
            CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
            if(!pPawn) continue;
            if(g_pAdminApi->IsPlayerPunished(i, iType)) continue;
            if(iImmunity < g_pAdminApi->GetAdminImmunity(i)) continue;
            g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pMenus->escapeString(engine->GetClientConVarValue(i, "name")).c_str());
            bFound = true;
        }
        break;
    case 2:
        for (int i = 0; i < 64; i++)
        {
            CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
            if(!pPlayer || pPlayer->m_steamID == 0) continue;
            CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
            if(!pPawn) continue;
            if(g_pAdminApi->IsPlayerPunished(i, iType)) continue;
            if(iImmunity <= g_pAdminApi->GetAdminImmunity(i)) continue;
            g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pMenus->escapeString(engine->GetClientConVarValue(i, "name")).c_str());
            bFound = true;
        }
        break;
    case 3:
        for (int i = 0; i < 64; i++)
        {
            CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
            if(!pPlayer || pPlayer->m_steamID == 0) continue;
            CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
            if(!pPawn) continue;
            if(g_pAdminApi->IsPlayerPunished(i, iType)) continue;
            if(g_pAdminApi->IsAdmin(i)) continue;
            g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pMenus->escapeString(engine->GetClientConVarValue(i, "name")).c_str());
            bFound = true;
        }
        break;
    }
    if(!bFound)
    {
        g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["NoPlayers"].c_str(), ITEM_DISABLED);
    }
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [iType](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            ShowReasonsMenu(iSlot, std::atoi(szBack), iType, 0);
        }
        else if(iItem == 7)
        {
            OnPunishSelect(iSlot, "", "", "");
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void ShowUnPunishMenu(int iSlot, int iType)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectPlayer"].c_str());
    bool bUnpunishAll = g_pAdminApi->HasPermission(iSlot, "@admin/unpunish_all");
    bool bFound = false;
    uint64 iSteamID = g_pPlayers->GetSteamID64(iSlot); 
    for (int i = 0; i < 64; i++)
    {
        CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
        if(!pPlayer || pPlayer->m_steamID == 0) continue;
        CCSPlayerPawn* pPawn = pPlayer->GetPlayerPawn();
        if(!pPawn) continue;
        if(!g_pAdminApi->IsPlayerPunished(i, iType)) continue;
        if(g_iUnpunishType == 0 && g_iAdminPunish[i][iType] != iSteamID && !bUnpunishAll) continue;
        g_pMenus->AddItemMenu(hMenu, std::to_string(i).c_str(), g_pMenus->escapeString(engine->GetClientConVarValue(i, "name")).c_str());
        bFound = true;
    }
    if(!bFound)
    {
        g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["NoPlayers"].c_str(), ITEM_DISABLED);
    }
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [iType](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            g_pAdminApi->RemovePlayerPunishment(std::atoi(szBack), iType, iSlot, true);
            g_pUtils->PrintToChat(iSlot, g_vecPhrases["UnPunishSuccess"].c_str(), szFront);
            g_pMenus->ClosePlayerMenu(iSlot);
        }
        else if(iItem == 7)
        {
            OnUnPunishSelect(iSlot, "", "", "");
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void ShowPunishOfflineMenu(int iSlot, int iType)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectOfflinePlayer"].c_str());
    bool bFound = false;
    for (const auto& user : g_mOfflineUsers) {
        if(user.second.bPunished[iType] || user.second.iAdminID[iType] != 0) continue;
        g_pMenus->AddItemMenu(hMenu, std::to_string(user.first).c_str(), user.second.szName.c_str());
        bFound = true;
    }
    if(!bFound)
    {
        g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["NoPlayers"].c_str(), ITEM_DISABLED);
    }
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [iType](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            ShowReasonsMenu(iSlot, std::stoll(szBack), iType, 1);
        }
        else if(iItem == 7)
        {
            OnPunishOfflineSelect(iSlot, "", "", "");
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void OnPunishOfflineSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectPunishType"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/ban")) g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["Item_BanPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/mute")) g_pMenus->AddItemMenu(hMenu, "1", g_vecPhrases["Item_MutePlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/gag")) g_pMenus->AddItemMenu(hMenu, "2", g_vecPhrases["Item_GagPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/silence")) g_pMenus->AddItemMenu(hMenu, "3", g_vecPhrases["Item_SilencePlayer"].c_str());
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            ShowPunishOfflineMenu(iSlot, std::atoi(szBack));
        }
        else if(iItem == 7)
        {
            ShowLastCategoryMenu(iSlot);
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void ShowUnPunishOfflineMenu(int iSlot, int iType)
{
    char szQuery[256], szSteamID[64];
    bool bUnpunishAll = g_pAdminApi->HasPermission(iSlot, "@admin/unpunish_all");
    g_SMAPI->Format(szSteamID, sizeof(szSteamID), "admin_id = '%i' AND ", g_pAdmins[iSlot].iID);
    g_SMAPI->Format(szQuery, sizeof(szQuery), 
        "SELECT * FROM %spunishments WHERE %spunish_type = %d AND (expires > %d OR expires = 0) AND unpunish_admin_id IS NULL ORDER BY `id` DESC LIMIT %i",
        g_szDatabasePrefix,
        g_iUnpunishType == 0 && !bUnpunishAll ?szSteamID:"",
        iType,
        std::time(0),
        g_iUnpunishOfflineCount
    );
    g_pConnection->Query(szQuery, [iSlot, iType](ISQLQuery* query) {
        ISQLResult *result = query->GetResultSet();
        Menu hMenu;
        g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectOfflinePlayer"].c_str());
        bool bFound = false;
        if (result->GetRowCount() > 0) {
            while (result->FetchRow()) {
                const char* szSteamID = result->GetString(2);
                const char* szName = result->GetString(1);
                g_pMenus->AddItemMenu(hMenu, szSteamID, szName);
                bFound = true;
            }
        }
        else
        {
            g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["NoPlayers"].c_str(), ITEM_DISABLED);
        }
        g_pMenus->SetExitMenu(hMenu, true);
        g_pMenus->SetBackMenu(hMenu, true);
        g_pMenus->SetCallback(hMenu, [iType](const char* szBack, const char* szFront, int iItem, int iSlot) {
            if(iItem < 7)
            {
                g_pAdminApi->RemoveOfflinePlayerPunishment(szBack, iType, iSlot);
                g_pUtils->PrintToChat(iSlot, g_vecPhrases["UnPunishOfflineSuccess"].c_str(), szFront);
                g_pMenus->ClosePlayerMenu(iSlot);
            }
            else if(iItem == 7)
            {
                OnUnPunishOfflineSelect(iSlot, "", "", "");
            }
        });
        g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
    });
}

void OnUnPunishOfflineSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectUnPunishType"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/unban")) g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["Item_UnBanPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/unmute")) g_pMenus->AddItemMenu(hMenu, "1", g_vecPhrases["Item_UnMutePlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/ungag")) g_pMenus->AddItemMenu(hMenu, "2", g_vecPhrases["Item_UnGagPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/unsilence")) g_pMenus->AddItemMenu(hMenu, "3", g_vecPhrases["Item_UnSilencePlayer"].c_str());
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            ShowUnPunishOfflineMenu(iSlot, std::atoi(szBack));
        }
        else if(iItem == 7)
        {
            ShowLastCategoryMenu(iSlot);
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void OnUnPunishSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectUnPunishType"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/unban")) g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["Item_UnBanPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/unmute")) g_pMenus->AddItemMenu(hMenu, "1", g_vecPhrases["Item_UnMutePlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/ungag")) g_pMenus->AddItemMenu(hMenu, "2", g_vecPhrases["Item_UnGagPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/unsilence")) g_pMenus->AddItemMenu(hMenu, "3", g_vecPhrases["Item_UnSilencePlayer"].c_str());
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            ShowUnPunishMenu(iSlot, std::atoi(szBack));
        }
        else if(iItem == 7)
        {
            ShowLastCategoryMenu(iSlot);
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void OnPunishSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)
{
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_vecPhrases["SelectPunishType"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/ban")) g_pMenus->AddItemMenu(hMenu, "0", g_vecPhrases["Item_BanPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/mute")) g_pMenus->AddItemMenu(hMenu, "1", g_vecPhrases["Item_MutePlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/gag")) g_pMenus->AddItemMenu(hMenu, "2", g_vecPhrases["Item_GagPlayer"].c_str());
    if(g_pAdminApi->HasPermission(iSlot, "@admin/silence")) g_pMenus->AddItemMenu(hMenu, "3", g_vecPhrases["Item_SilencePlayer"].c_str());
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, [](const char* szBack, const char* szFront, int iItem, int iSlot) {
        if(iItem < 7)
        {
            ShowPunishMenu(iSlot, std::atoi(szBack));
        }
        else if(iItem == 7)
        {
            ShowLastCategoryMenu(iSlot);
        }
    });
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void ShowItemMenu(int iSlot, const char* szCategory, const char* szIdentity)
{
    auto catIt = mCategories.find(szCategory);
    if (catIt == mCategories.end()) return;
    Category& category = catIt->second;
    auto itemIt = category.mItems.find(szIdentity);
    if (itemIt == category.mItems.end()) return;
    Item& item = itemIt->second;
    if(item.hCallbackSelect) item.hCallbackSelect(iSlot, szCategory, szIdentity, item.szName);
}

void OnAdminItemMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
    if(iItem < 7)
    {
        if (!szBack || !g_szLastCategory[iSlot].size()) return;
        const char* szItem = szBack;
        ShowItemMenu(iSlot, g_szLastCategory[iSlot].c_str(), szItem);
    }
    else if(iItem == 7)
    {
        if (!szBack) return;
        OnAdminMenu(iSlot, szBack);
    }
}

std::vector<AdminMenuEntry> CollectCategoryItems(int iSlot, const char* szCategory)
{
    std::vector<AdminMenuEntry> vecItems;
    if (!szCategory || !g_pAdminApi) return vecItems;
    auto catIt = mCategories.find(szCategory);
    if (catIt == mCategories.end()) return vecItems;
    Category& category = catIt->second;
    auto addItem = [&](const Item& _item) {
        if(!_item.szCategory || !_item.szIdentity || !_item.szCategory[0] || !_item.szIdentity[0]) return;
        if(!HasAccessInItem(iSlot, _item.szCategory, _item.szIdentity)) return;
        std::string szName = g_pAdminApi->GetTranslation(_item.szName);
        if(_item.hCallbackDisplay) _item.hCallbackDisplay(iSlot, _item.szCategory, _item.szIdentity, szName);
        vecItems.push_back({_item.szIdentity, szName});
    };
    auto sortIt = g_mSortItems.find(szCategory);
    if(sortIt != g_mSortItems.end())
    {
        for (const auto& item : sortIt->second) {
            auto itemIt = category.mItems.find(item);
            if(itemIt != category.mItems.end()) addItem(itemIt->second);
        }
    }
    for (const auto& item : category.mItems) {
        if(sortIt != g_mSortItems.end() && std::find(sortIt->second.begin(), sortIt->second.end(), item.first) != sortIt->second.end()) continue;
        addItem(item.second);
    }
    return vecItems;
}

std::vector<AdminMenuEntry> CollectCategories(int iSlot)
{
    std::vector<AdminMenuEntry> vecCategories;
    if (!g_pAdminApi || !g_pAdminApi->IsAdmin(iSlot)) return vecCategories;
    auto addCategory = [&](const std::string& key, const Category& category) {
        if(!category.szIdentity || !category.szIdentity[0]) return;
        if(!HasAccessInCategory(iSlot, key.c_str())) return;
        std::string szName = g_pAdminApi->GetTranslation(category.szName);
        if(category.hCallback) category.hCallback(iSlot, key.c_str(), szName);
        vecCategories.push_back({key, szName});
    };
    for (const auto& category : g_vSortCategories) {
        auto catIt = mCategories.find(category);
        if(catIt != mCategories.end()) addCategory(category, catIt->second);
    }
    for (const auto& item : mCategories) {
        if(std::find(g_vSortCategories.begin(), g_vSortCategories.end(), item.first) != g_vSortCategories.end()) continue;
        addCategory(item.first, item.second);
    }
    return vecCategories;
}

void ShowCategoryMenu(int iSlot, const char* szCategory)
{
    if (!g_pMenus || !g_pAdminApi) return;
    auto catIt = mCategories.find(szCategory);
    if (catIt == mCategories.end()) return;
    Menu hMenu;
    g_pMenus->SetTitleMenu(hMenu, g_pAdminApi->GetTranslation(catIt->second.szName));
    for (const auto& entry : CollectCategoryItems(iSlot, szCategory))
        g_pMenus->AddItemMenu(hMenu, entry.szIdentity.c_str(), entry.szName.c_str());
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, true);
    g_pMenus->SetCallback(hMenu, OnAdminItemMenuCallback);
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
}

void ShowLastCategoryMenu(int iSlot)
{
    if (!g_szLastCategory[iSlot].size()) return;
    ShowCategoryMenu(iSlot, g_szLastCategory[iSlot].c_str());
}

void OnAdminMenuCallback(const char* szBack, const char* szFront, int iItem, int iSlot)
{
    if(iItem < 7)
    {
        if (!szBack) return;
        g_szLastCategory[iSlot] = szBack;
        ShowCategoryMenu(iSlot, g_szLastCategory[iSlot].c_str());
    }
}

bool OnAdminMenu(int iSlot, const char* szContent)
{
    if(!g_pAdminApi || !g_pMenus || !g_pAdminApi->IsAdmin(iSlot)) return true;
    Menu hMenu;
    auto phraseIt = g_vecPhrases.find("MainTitle");
    if (phraseIt == g_vecPhrases.end()) return true;
    g_pMenus->SetTitleMenu(hMenu, phraseIt->second.c_str());
    for (const auto& entry : CollectCategories(iSlot))
        g_pMenus->AddItemMenu(hMenu, entry.szIdentity.c_str(), entry.szName.c_str());
    g_pMenus->SetExitMenu(hMenu, true);
    g_pMenus->SetBackMenu(hMenu, false);
    g_pMenus->SetCallback(hMenu, OnAdminMenuCallback); 
    g_pMenus->DisplayPlayerMenu(hMenu, iSlot, true, true);
    return true;
}