#include "database.h"
#include <ISmmPlugin.h>
#include "inetchannelinfo.h"
#include "admin_system.h"
#include "CCSPlayerController.h"

extern IVEngineServer2* engine;
extern IMySQLClient *g_pMysqlClient;
extern IMySQLConnection* g_pConnection;
extern IUtilsApi* g_pUtils;
extern IMenusApi* g_pMenus;
extern IFileSystem* g_pFullFileSystem;
extern ISmmAPI *g_SMAPI;
extern ISmmPlugin *g_PLAPI;
extern const char* g_szDatabasePrefix;
extern int g_iServerID[2];
extern uint64 g_iAdminPunish[64][4];
extern int g_iPunishments[64][4];
extern std::string g_szPunishReasons[64][4];
extern Admin g_pAdmins[64];
extern AdminApi* g_pAdminApi;
extern IPlayersApi* g_pPlayers;
extern int g_iBanDelay;
extern int g_iUnpunishType;
extern bool g_bStaticNames;
extern bool g_bPunishIP;
extern std::map<std::string, std::string> g_vecPhrases;
extern int g_iNofityType;
extern int g_iImmunityType;

void CreateConnection()
{
    KeyValues* pKVConfig = new KeyValues("Databases");
    if (!pKVConfig->LoadFromFile(g_pFullFileSystem, "addons/configs/databases.cfg")) {
        g_pUtils->ErrorLog("[%s] Failed to load databases config addons/configs/databases.cfg", g_PLAPI->GetLogTag());
        return;
    }

    pKVConfig = pKVConfig->FindKey("admin_system", false);
    if (!pKVConfig) {
        g_pUtils->ErrorLog("[%s] No databases.cfg 'admin_system'", g_PLAPI->GetLogTag());
        return;
    }

    MySQLConnectionInfo info;
    info.host = pKVConfig->GetString("host", nullptr);
    info.user = pKVConfig->GetString("user", nullptr);
    info.pass = pKVConfig->GetString("pass", nullptr);
    info.database = pKVConfig->GetString("database", nullptr);
    info.port = pKVConfig->GetInt("port");
    g_pConnection = g_pMysqlClient->CreateMySQLConnection(info);

    g_pConnection->Connect([](bool connect) {
        if (!connect) {
            g_pUtils->ErrorLog("[%s] Failed to connect to the database, verify the data and try again", g_PLAPI->GetLogTag());
        } else {
            char szQuery[1024];
            Transaction txn;

            g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS %sgroups (\
                                        id INT PRIMARY KEY AUTO_INCREMENT,\
                                        name VARCHAR(128),\
                                        flags VARCHAR(64),\
                                        immunity INT,\
                                        UNIQUE (name)\
                                    );", g_szDatabasePrefix);
            txn.queries.push_back(szQuery);

            g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS %sadmins (\
                                        id INT PRIMARY KEY AUTO_INCREMENT,\
                                        name VARCHAR(256),\
                                        steamid VARCHAR(32),\
                                        comment VARCHAR(256) DEFAULT NULL\
                                    );", g_szDatabasePrefix, g_szDatabasePrefix);
            txn.queries.push_back(szQuery);

            g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS %sadmins_servers (\
                                        id INT PRIMARY KEY AUTO_INCREMENT,\
                                        admin_id INT,\
                                        flags VARCHAR(64) DEFAULT NULL,\
                                        immunity INT DEFAULT 0,\
                                        expires INT DEFAULT 0,\
                                        group_id INT DEFAULT NULL,\
                                        server_id INT,\
                                        UNIQUE (admin_id, server_id)\
                                    );", g_szDatabasePrefix);
            txn.queries.push_back(szQuery);

            g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS %spunishments (\
                                        id INT PRIMARY KEY AUTO_INCREMENT,\
                                        name VARCHAR(256),\
                                        steamid VARCHAR(32),\
                                        ip VARCHAR(32),\
                                        admin_id INT,\
                                        created INT,\
                                        expires INT,\
                                        reason VARCHAR(256),\
                                        unpunish_admin_id INT DEFAULT NULL,\
                                        server_id INT,\
                                        punish_type INT\
                                    );", g_szDatabasePrefix, g_szDatabasePrefix, g_szDatabasePrefix);
            txn.queries.push_back(szQuery);
            g_pConnection->ExecuteTransaction(txn, [](std::vector<ISQLQuery *>) {}, [](std::string error, int code) {
                g_pUtils->ErrorLog("[%s] Failed to create tables: %s", g_PLAPI->GetLogTag(), error.c_str());
            });

            g_SMAPI->Format(szQuery, sizeof(szQuery), "SELECT * FROM %sadmins WHERE steamid = '0'", g_szDatabasePrefix);
            g_pConnection->Query(szQuery, [](ISQLQuery* query) {
                ISQLResult *result = query->GetResultSet();
                if (result->GetRowCount() == 0) {
                    char szQuery[256];
                    g_SMAPI->Format(szQuery, sizeof(szQuery), "INSERT INTO %sadmins (id, name, steamid) VALUES (1, 'Console', '0')", g_szDatabasePrefix);
                    g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
                }
            });
	        g_pAdminApi->OnCoreLoadedSend();
        }
    });
}

void CheckPunishments(int iSlot, uint64 xuid)
{
    bool bIP = true;
    auto playerNetInfo = engine->GetPlayerNetInfo(iSlot);
    std::string sIp;
    std::string sIp2;
    if (playerNetInfo == nullptr) bIP = false;
    else {
        sIp2 = std::string(playerNetInfo->GetAddress());
        sIp = sIp2.substr(0, sIp2.find(":"));
    }

    char szQuery[512];
    g_SMAPI->Format(szQuery, sizeof(szQuery),
        "SELECT p.*, a.steamid AS admin_steamid FROM %spunishments p LEFT JOIN %sadmins a ON p.admin_id = a.id WHERE (p.steamid = '%llu'%s) AND p.unpunish_admin_id IS NULL",
        g_szDatabasePrefix,
        g_szDatabasePrefix,
        xuid,
        g_bPunishIP && bIP ? (" OR p.ip = '" + sIp + "'").c_str() : ""
    );

    g_pConnection->Query(szQuery, [iSlot](ISQLQuery* query) {
        ISQLResult *result = query->GetResultSet();
        if (result->GetRowCount() > 0) {
            while (result->FetchRow()) {
                int serverID = result->GetInt(9);
                if (serverID != -1 && serverID != g_iServerID[SID_PUNISH]) continue;

                int iType = result->GetInt(10);
                int iExpired = result->GetInt(6);
                if(iExpired != 0 && iExpired < std::time(0)) continue;
                
                if(iType == RT_BAN) {
                    engine->DisconnectClient(CPlayerSlot(iSlot), NETWORK_DISCONNECT_KICKBANADDED);
                    break;
                }

                if(g_iPunishments[iSlot][iType] != 0 && g_iPunishments[iSlot][iType] < iExpired)
                    g_iPunishments[iSlot][iType] = iExpired;

                g_szPunishReasons[iSlot][iType] = result->GetString(7);
                if(result->IsNull(11)) continue;
                const char* szAdminSteamID = result->GetString(11);
                if(OnlyDigits(szAdminSteamID)) {
                    uint64 adminSteamID = std::stoll(szAdminSteamID);
                    g_iAdminPunish[iSlot][iType] = adminSteamID;
                }
            }
        }
    });
}

std::string FormatSeconds(int iTime)
{
    if (iTime == 0) return g_vecPhrases["Never"];
    int iSeconds = iTime;
    int iDays = iSeconds / 86400;
    iSeconds %= 86400;
    int iHours = iSeconds / 3600;
    iSeconds %= 3600;
    int iMinutes = iSeconds / 60;
    iSeconds %= 60;

    std::string parts;

    if (iDays > 0) 
        parts += std::to_string(iDays) + g_vecPhrases["Days"];
    if (iHours > 0) 
        parts += std::to_string(iHours) + g_vecPhrases["Hours"];
    if (iMinutes > 0)
        parts += std::to_string(iMinutes) + g_vecPhrases["Minutes"];
    // if (iSeconds > 0)
    //     parts += std::to_string(iSeconds) + g_vecPhrases["Seconds"];

    return parts;
}

void CheckPunishmentsForce(int iSlot, uint64 xuid)
{
    bool bIP = true;
    auto playerNetInfo = engine->GetPlayerNetInfo(iSlot);
    std::string sIp;
    std::string sIp2;
    if (playerNetInfo == nullptr) bIP = false;
    else {
        sIp2 = std::string(playerNetInfo->GetAddress());
        sIp = sIp2.substr(0, sIp2.find(":"));
    }

    char szQuery[512];
    g_SMAPI->Format(szQuery, sizeof(szQuery),
        "SELECT p.*, a.steamid, a.name, a.id AS admin_steamid FROM %spunishments p LEFT JOIN %sadmins a ON p.admin_id = a.id WHERE (p.steamid = '%llu'%s) AND p.unpunish_admin_id IS NULL",
        g_szDatabasePrefix,
        g_szDatabasePrefix,
        xuid,
        g_bPunishIP && bIP ? (" OR p.ip = '" + sIp + "'").c_str() : ""
    );

    g_pConnection->Query(szQuery, [iSlot](ISQLQuery* query) {
        ISQLResult *result = query->GetResultSet();
        if (result->GetRowCount() > 0) {
            while (result->FetchRow()) {
                int serverID = result->GetInt(9);
                if (serverID != -1 && serverID != g_iServerID[SID_PUNISH]) continue;
                int iType = result->GetInt(10);
                int iCreated = result->GetInt(5);
                int iExpired = result->GetInt(6);
                const char* szReason = result->GetString(7);
                if(iExpired != 0 && iExpired < std::time(0)) continue;
                
                if(iType == RT_BAN) {
                    if(g_iBanDelay > 0) {
                        
                        g_pUtils->CreateTimer(g_iBanDelay, [iSlot]() {
                            engine->DisconnectClient(CPlayerSlot(iSlot), NETWORK_DISCONNECT_KICKBANADDED);
                            return -1.0f;
                        });
                    } else engine->DisconnectClient(CPlayerSlot(iSlot), NETWORK_DISCONNECT_KICKBANADDED);
                } else {
                    if(g_iPunishments[iSlot][iType] != 0 && g_iPunishments[iSlot][iType] < iExpired)
                        g_iPunishments[iSlot][iType] = iExpired;

                    g_szPunishReasons[iSlot][iType] = szReason;
                    
                    if(!result->IsNull(11)) {
                        const char* szAdminSteamID = result->GetString(11);
                        if(OnlyDigits(szAdminSteamID)) {
                            uint64 adminSteamID = std::stoll(szAdminSteamID);
                            g_iAdminPunish[iSlot][iType] = adminSteamID;
                        }
                    }
                }
                int iTime = iExpired > 0?iExpired-iCreated:0;
                const char* szAdminName = result->GetString(12);
                int iAdminID = result->GetInt(13);
                if(iAdminID == 1) iAdminID = -1;
                g_pAdminApi->OnPlayerPunishSend(iSlot, iType, iTime, szReason, iAdminID);
                std::string szMessage = "";
                switch (iType)
                {
                case RT_BAN:
                    switch (g_iNofityType)
                    {
                    case 1:
                        szMessage = g_vecPhrases["NewBanMessage"];
                        break;
                    case 2:
                        szMessage = g_vecPhrases["NewBanMessageAll"];
                        break;
                    }
                    break;
                case RT_MUTE:
                    switch (g_iNofityType)
                    {
                    case 1:
                        szMessage = g_vecPhrases["NewMuteMessage"];
                        break;
                    case 2:
                        szMessage = g_vecPhrases["NewMuteMessageAll"];
                        break;
                    }
                    break;
                case RT_GAG:
                    switch (g_iNofityType)
                    {
                    case 1:
                        szMessage = g_vecPhrases["NewGagMessage"];
                        break;
                    case 2:
                        szMessage = g_vecPhrases["NewGagMessageAll"];
                        break;
                    }
                    break;
                case RT_SILENCE:
                    switch (g_iNofityType)
                    {
                    case 1:
                        szMessage = g_vecPhrases["NewSilenceMessage"];
                        break;
                    case 2:
                        szMessage = g_vecPhrases["NewSilenceMessageAll"];
                        break;
                    }
                    break;
                }
                switch (g_iNofityType)
                {
                case 1:
                    g_pUtils->PrintToChat(iSlot, szMessage.c_str(), FormatSeconds(iTime).c_str(), szReason, iAdminID == -1?"Console":szAdminName);
                    if(iAdminID != -1)
                    {
                        switch (iType)
                        {
                        case RT_BAN:
                            g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewBanMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatSeconds(iTime).c_str(), szReason);
                            break;
                        case RT_MUTE:
                            g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewMuteMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatSeconds(iTime).c_str(), szReason);
                            break;
                        case RT_GAG:
                            g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewGagMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatSeconds(iTime).c_str(), szReason);
                            break;
                        case RT_SILENCE:
                            g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewSilenceMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatSeconds(iTime).c_str(), szReason);
                            break;
                        }
                    }
                    break;
                case 2:
                    g_pUtils->PrintToChatAll(szMessage.c_str(), iAdminID == -1?"Console":szAdminName, engine->GetClientConVarValue(iSlot, "name"), FormatSeconds(iTime).c_str(), szReason);
                    break;
                }
            }
        }
    });
}

void CheckPermissions(int iSlot, uint64 xuid)
{
    g_pAdmins[iSlot].iID = 0;
    g_pAdmins[iSlot].iImmunity = 0;
    g_pAdmins[iSlot].iExpireTime = -1;
    g_pAdmins[iSlot].vFlags.clear();
    g_pAdmins[iSlot].vPermissions.clear();

    char szQuery[512];
    g_SMAPI->Format(szQuery, sizeof(szQuery),
        "SELECT a.id, a.name, s.flags, s.immunity, s.expires, s.group_id, g.name AS group_name, g.flags AS group_flags, g.immunity AS group_immunity, s.server_id FROM %sadmins a LEFT JOIN %sadmins_servers s ON a.id = s.admin_id LEFT JOIN %sgroups g ON s.group_id = g.id WHERE a.steamid = '%llu'",
        g_szDatabasePrefix,
        g_szDatabasePrefix,
        g_szDatabasePrefix,
        xuid
    );
    g_pConnection->Query(szQuery, [iSlot](ISQLQuery* query) {
        ISQLResult *result = query->GetResultSet();
        if (result->GetRowCount() > 0) {
            Admin hAdmin;
            hAdmin.iExpireTime = -1;
            hAdmin.iImmunity = 0;
            while (result->FetchRow()) {
                hAdmin.iID = result->GetInt(0);
                hAdmin.szName = result->GetString(1);
                int iImmunity = result->GetInt(3);
                if(iImmunity > hAdmin.iImmunity) hAdmin.iImmunity = iImmunity;
                int iGroupImmunity = result->IsNull(8) ? 0 : result->GetInt(8);
                int iServerID = result->GetInt(9);
                if(g_iServerID[SID_ADMIN] != -1 && iServerID != g_iServerID[SID_ADMIN] && iServerID != -1) continue;
                if(iGroupImmunity > hAdmin.iImmunity) hAdmin.iImmunity = iGroupImmunity;
                int iExpires = result->GetInt(4);
                if((iExpires > hAdmin.iExpireTime || iExpires == 0) && hAdmin.iExpireTime != 0) hAdmin.iExpireTime = iExpires;
                const char* szFlags = result->GetString(2);
                for (size_t i = 0; i < strlen(szFlags); i++) {
                    std::vector<std::string> vPermissions = g_pAdminApi->GetPermissionsByFlag(std::string(1, szFlags[i]).c_str());
                    hAdmin.vPermissions.insert(hAdmin.vPermissions.end(), vPermissions.begin(), vPermissions.end());
                    hAdmin.vFlags.push_back(std::string(1, szFlags[i]));
                }
                if(!result->IsNull(5) && result->GetInt(5) != 0) {
                    int iGroupID = result->GetInt(5);
                    hAdmin.iGroup = iGroupID;
                    hAdmin.szGroupName = result->GetString(6);
                    const char* szGroupFlags = result->GetString(7);
                    for (size_t i = 0; i < strlen(szGroupFlags); i++) {
                        std::vector<std::string> vGroupPermissions = g_pAdminApi->GetPermissionsByFlag(std::string(1, szGroupFlags[i]).c_str());
                        hAdmin.vPermissions.insert(hAdmin.vPermissions.end(), vGroupPermissions.begin(), vGroupPermissions.end());
                        hAdmin.vFlags.push_back(std::string(1, szGroupFlags[i]));
                    }
                }
            }
            g_pAdmins[iSlot] = hAdmin;
            g_pAdminApi->OnAdminConnectedSend(iSlot);
        }
    });
}

bool AddPunishment(int iSlot, int iType, int iTime, std::string szReason, int iAdminID, bool bDB)
{
    if(g_pAdminApi->OnPlayerPunishPreSend(iSlot, iType, iTime, szReason.c_str(), iAdminID)) return false;
    uint64 SteamID64 = g_pPlayers->GetSteamID64(iSlot);
    if(SteamID64 == 0) return false;
    if(bDB)
    {
        auto playerNetInfo = engine->GetPlayerNetInfo(iSlot);
        bool bIP = false;
        std::string sIp2;
        std::string sIp;
        if(playerNetInfo != nullptr)
        {
            bIP = true;
            sIp2 = std::string(playerNetInfo->GetAddress());
            sIp = sIp2.substr(0, sIp2.find(":"));
        }
        char szQuery[512];
        g_SMAPI->Format(szQuery, sizeof(szQuery), 
            "INSERT INTO %spunishments (name, steamid, ip, admin_id, created, expires, reason, server_id, punish_type) VALUES ('%s', '%llu', '%s', %d, %d, %d, '%s', %d, %d)",
            g_szDatabasePrefix,
            g_pConnection->Escape(engine->GetClientConVarValue(iSlot, "name")).c_str(),
            SteamID64,
            bIP?sIp.c_str():"",
            iAdminID == -1 ? 1 : g_pAdmins[iAdminID].iID,
            std::time(0),
            iTime == 0 ? 0 : std::time(0) + iTime,
            g_pConnection->Escape(szReason.c_str()).c_str(),
            g_iServerID[SID_PUNISH],
            iType
        );
        g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
    }
    
    g_iPunishments[iSlot][iType] = iTime == 0 ? 0 : std::time(0) + iTime;
    g_szPunishReasons[iSlot][iType] = szReason;
    if(iAdminID != -1) g_iAdminPunish[iSlot][iType] = g_pPlayers->GetSteamID64(iAdminID);
    else g_iAdminPunish[iSlot][iType] = 0;
    g_pAdminApi->OnPlayerPunishSend(iSlot, iType, iTime, szReason.c_str(), iAdminID);
    if(iType == RT_BAN) {
        if(g_iBanDelay > 0) {
            g_pUtils->CreateTimer(g_iBanDelay, [iSlot]() {
                engine->DisconnectClient(CPlayerSlot(iSlot), NETWORK_DISCONNECT_KICKBANADDED);
                return -1.0f;
            });
        } else engine->DisconnectClient(CPlayerSlot(iSlot), NETWORK_DISCONNECT_KICKBANADDED);
    }
    return true;
}

void AddOfflinePunishment(const char* szSteamID64, const char* szName, int iType, int iTime, std::string szReason, int iAdminID)
{
    if(!szSteamID64 || strlen(szSteamID64) == 0) return;
    if(g_pAdminApi->OnOfflinePlayerPunishPreSend(szSteamID64, szName, iType, iTime, szReason.c_str(), iAdminID)) return;
    char szQuery[256];
    g_SMAPI->Format(szQuery, sizeof(szQuery), 
        "INSERT INTO %spunishments (name, steamid, admin_id, created, expires, reason, server_id, punish_type) VALUES ('%s', '%s', %d, %d, %d, '%s', %d, %d)",
        g_szDatabasePrefix,
        g_pConnection->Escape(szName).c_str(),
        szSteamID64,
        iAdminID == -1 ? 1 : g_pAdmins[iAdminID].iID,
        std::time(0),
        iTime == 0 ? 0 : std::time(0) + iTime,
        g_pConnection->Escape(szReason.c_str()).c_str(),
        g_iServerID[SID_PUNISH],
        iType
    );

    g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
    g_pAdminApi->OnOfflinePlayerPunishSend(szSteamID64, szName, iType, iTime, szReason.c_str(), iAdminID);
}

void RemovePunishment(int iSlot, int iType, int iAdminID)
{
    uint64 SteamID64 = g_pPlayers->GetSteamID64(iSlot);
    if(SteamID64 == 0) return;
    char szQuery[256];
    g_SMAPI->Format(szQuery, sizeof(szQuery), 
        "UPDATE %spunishments SET unpunish_admin_id = %d WHERE steamid = '%llu' AND punish_type = %d AND (expires > %d OR expires = 0) AND server_id = %d",
        g_szDatabasePrefix,
        iAdminID == -1 ? 1 : g_pAdmins[iAdminID].iID,
        SteamID64,
        iType,
        std::time(0),
        g_iServerID[SID_PUNISH]
    );
    g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
    g_iPunishments[iSlot][iType] = -1;
    g_szPunishReasons[iSlot][iType] = "";
    g_pAdminApi->OnPlayerUnpunishSend(iSlot, iType, iAdminID);
}

void RemoveOfflinePunishment(const char* szSteamID64, int iType, int iAdminID)
{
    bool bUnpunishAll = g_pAdminApi->HasPermission(iAdminID, "@admin/unpunish_all");
    char szQuery[256];
    char szAdminID[32];
    g_SMAPI->Format(szAdminID, sizeof(szAdminID), " AND admin_id = %d", iAdminID == -1 ? 1 : g_pAdmins[iAdminID].iID);
    g_SMAPI->Format(szQuery, sizeof(szQuery),
        "UPDATE %spunishments SET unpunish_admin_id = %d WHERE steamid = '%s' AND punish_type = %d AND (expires > %d OR expires = 0)%s AND server_id = %d",
        g_szDatabasePrefix,
        iAdminID == -1 ? 1 : g_pAdmins[iAdminID].iID,
        szSteamID64,
        iType,
        std::time(0),
        g_iUnpunishType == 0 && iAdminID != -1 && !bUnpunishAll? szAdminID : "",
        g_iServerID[SID_PUNISH]
    );

    g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
    g_pAdminApi->OnOfflinePlayerUnpunishSend(szSteamID64, iType, iAdminID);
}

void AddAdmin(const char* szName, const char* szSteamID64, const char* szFlags, int iImmunity, int iExpireTime, int iGroupID, const char* szComment, bool bDB)
{
    const char* szSteamID = strdup(szSteamID64);
    bool bFound = false;
    int iSlot = -1;
    if(OnlyDigits(szSteamID)) {
        for(int i = 0; i < 64; i++)
        {
            CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
            if(!pPlayer) continue;
            if(g_pPlayers->GetSteamID64(i) == std::stoull(szSteamID))
            {
                bFound = true;
                iSlot = i;
            }
        }
    }

    if(bFound && g_pAdmins[iSlot].iID != 0) return;

    if(bDB) 
    {
        char szQuery[512];
        g_SMAPI->Format(szQuery, sizeof(szQuery), 
            "SELECT id FROM %sadmins WHERE steamid = '%s'",
            g_szDatabasePrefix,
            szSteamID
        );
        g_pConnection->Query(szQuery, [iSlot, szName, szSteamID, szFlags, iImmunity, iExpireTime, iGroupID, szComment](ISQLQuery* query) {
            ISQLResult *result = query->GetResultSet();
            if (result->GetRowCount() > 0) {
                result->FetchRow();
                int iAdminID = result->GetInt(0);
                char szQuery[512];
                g_SMAPI->Format(szQuery, sizeof(szQuery), 
                    "INSERT IGNORE INTO %sadmins_servers (admin_id, flags, immunity, expires, group_id, server_id) VALUES (%d, '%s', %d, %d, %d, %d)",
                    g_szDatabasePrefix,
                    iAdminID,
                    szFlags,
                    iImmunity,
                    iExpireTime == 0 ? 0 : std::time(0) + iExpireTime,
                    iGroupID,
                    g_iServerID[SID_ADMIN]
                );
                g_pConnection->Query(szQuery, [iSlot, szSteamID](ISQLQuery* query) {
                    if(iSlot != -1 && OnlyDigits(szSteamID)) {
                        CheckPermissions(iSlot, std::stoull(szSteamID));
                    }
                });
            } else {
                char szQuery[512];
                g_SMAPI->Format(szQuery, sizeof(szQuery), 
                    "INSERT INTO %sadmins (name, steamid, comment) VALUES ('%s', '%s', '%s')",
                    g_szDatabasePrefix,
                    g_pConnection->Escape(szName).c_str(),
                    szSteamID,
                    szComment
                );
                g_pConnection->Query(szQuery, [iSlot, szSteamID, szFlags, iImmunity, iExpireTime, iGroupID](ISQLQuery* query) {
                    char szQuery[512];
                    g_SMAPI->Format(szQuery, sizeof(szQuery), 
                        "SELECT id FROM %sadmins WHERE steamid = '%s'",
                        g_szDatabasePrefix,
                        szSteamID
                    );
                    g_pConnection->Query(szQuery, [iSlot, szSteamID, szFlags, iImmunity, iExpireTime, iGroupID](ISQLQuery* query) {
                        ISQLResult *result = query->GetResultSet();
                        if (result->GetRowCount() > 0) {
                            result->FetchRow();
                            int iAdminID = result->GetInt(0);
                            char szQuery[512];
                            g_SMAPI->Format(szQuery, sizeof(szQuery), 
                                "INSERT IGNORE INTO %sadmins_servers (admin_id, flags, immunity, expires, group_id, server_id) VALUES (%d, '%s', %d, %d, %d, %d)",
                                g_szDatabasePrefix,
                                iAdminID,
                                szFlags,
                                iImmunity,
                                iExpireTime == 0 ? 0 : std::time(0) + iExpireTime,
                                iGroupID,
                                g_iServerID[SID_ADMIN]
                            );
                            g_pConnection->Query(szQuery, [iSlot, szSteamID](ISQLQuery* query) {
                                if(iSlot != -1 && OnlyDigits(szSteamID)) {
                                    CheckPermissions(iSlot, std::stoull(szSteamID));
                                }
                            });
                        }
                    });
                });
            }
        });
    }

    if(bFound && !bDB)
    {
        Admin hAdmin;
        hAdmin.iID = 1;
        hAdmin.szName = szName;
        hAdmin.iImmunity = iImmunity;
        hAdmin.iExpireTime = iExpireTime;
        for (size_t i = 0; i < strlen(szFlags); i++) {
            std::vector<std::string> vPermissions = g_pAdminApi->GetPermissionsByFlag(std::string(1, szFlags[i]).c_str());
            hAdmin.vPermissions.insert(hAdmin.vPermissions.end(), vPermissions.begin(), vPermissions.end());
            hAdmin.vFlags.push_back(std::string(1, szFlags[i]));
        }
        g_pAdmins[iSlot] = hAdmin;
        if(iGroupID != 0)
        {
            char szQuery[512];
            g_SMAPI->Format(szQuery, sizeof(szQuery), 
                "SELECT * FROM %sgroups WHERE id = %d",
                g_szDatabasePrefix,
                iGroupID
            );
            g_pConnection->Query(szQuery, [iSlot](ISQLQuery* query) {
                ISQLResult *result = query->GetResultSet();
                if (result->GetRowCount() > 0) {
                    g_pAdmins[iSlot].iGroup = result->GetInt(0);
                    g_pAdmins[iSlot].szGroupName = result->GetString(1);
                    result->FetchRow();
                    int iImmunity = result->GetInt(3);
                    if(iImmunity > g_pAdmins[iSlot].iImmunity) g_pAdmins[iSlot].iImmunity = iImmunity;
                    const char* szFlags = result->GetString(2);
                    for (size_t i = 0; i < strlen(szFlags); i++) {
                        std::vector<std::string> vPermissions = g_pAdminApi->GetPermissionsByFlag(std::string(1, szFlags[i]).c_str());
                        g_pAdmins[iSlot].vPermissions.insert(g_pAdmins[iSlot].vPermissions.end(), vPermissions.begin(), vPermissions.end());
                        g_pAdmins[iSlot].vFlags.push_back(std::string(1, szFlags[i]));
                    }
                }
            });
        }
        g_pAdminApi->OnAdminConnectedSend(iSlot);
    }
}

void RemoveAdmin(const char* szSteamID64, bool bDB)
{
    if(bDB)
    {
        const char* szSteamID = strdup(szSteamID64);
        char szQuery[512];
        char szServerID[32];
        g_SMAPI->Format(szServerID, sizeof(szServerID), " AND server_id = %d", g_iServerID[SID_ADMIN]);
        g_SMAPI->Format(szQuery, sizeof(szQuery), 
            "DELETE FROM %sadmins_servers WHERE admin_id = (SELECT id FROM %sadmins WHERE steamid = '%s')%s",
            g_szDatabasePrefix,
            g_szDatabasePrefix,
            szSteamID,
            g_iServerID[SID_ADMIN] == -1 ? "" : szServerID
        );
        g_pConnection->Query(szQuery, [](ISQLQuery* query) {});

        g_SMAPI->Format(szQuery, sizeof(szQuery), 
            "SELECT * FROM %sadmins_servers WHERE admin_id = (SELECT id FROM %sadmins WHERE steamid = '%s')",
            g_szDatabasePrefix,
            g_szDatabasePrefix,
            szSteamID
        );
        g_pConnection->Query(szQuery, [szSteamID](ISQLQuery* query) {
            ISQLResult *result = query->GetResultSet();
            if (result->GetRowCount() == 0) {
                char szQuery[512];
                g_SMAPI->Format(szQuery, sizeof(szQuery), 
                    "DELETE FROM %sadmins WHERE steamid = '%s'",
                    g_szDatabasePrefix,
                    szSteamID
                );
                g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
            }
        });
    }
    if(OnlyDigits(szSteamID64)) {
        for(int i = 0; i < 64; i++)
        {
            CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
            if(!pPlayer) continue;
            if(g_pPlayers->GetSteamID64(i) == std::stoull(szSteamID64))
            {
                g_pAdmins[i].iID = 0;
                g_pAdmins[i].iImmunity = 0;
                g_pAdmins[i].iExpireTime = -1;
                g_pAdmins[i].vFlags.clear();
                g_pAdmins[i].vPermissions.clear();
            }
        }
    }
}

void AddGroup(int iSlot, const char* szName, const char* szFlags, int iImmunity, bool bConsole)
{
    char szQuery[512];
    g_SMAPI->Format(szQuery, sizeof(szQuery), 
        "INSERT INTO %sgroups (name, flags, immunity) VALUES ('%s', '%s', %d)",
        g_szDatabasePrefix,
        szName,
        szFlags,
        iImmunity
    );
    g_pConnection->Query(szQuery, [bConsole, iSlot](ISQLQuery* query) {
        int iGroupID = query->GetInsertId();
        if(iSlot == -1) {
            META_CONPRINTF("[Admin System] Group added (ID: %i)\n", iGroupID);
        }
        else {
            if(bConsole) g_pUtils->PrintToConsole(iSlot, "[Admin System] Group added (ID: %i)\n", iGroupID);
            else g_pUtils->PrintToChat(iSlot, g_pAdminApi->GetTranslation("GroupAdded"), iGroupID);
        }
    });
}

bool OnlyDigits(const char* szString)
{
    for (size_t i = 0; i < strlen(szString); i++)
    {
        if (!isdigit(szString[i])) return false;
    }
    return true;
}

void RemoveGroup(const char* szIdentifier)
{
    char szQuery[512];
    if(OnlyDigits(szIdentifier))
    {
        g_SMAPI->Format(szQuery, sizeof(szQuery), 
            "DELETE FROM %sgroups WHERE id = %s",
            g_szDatabasePrefix,
            szIdentifier
        );
    } else {
        g_SMAPI->Format(szQuery, sizeof(szQuery), 
            "DELETE FROM %sgroups WHERE name = '%s'",
            g_szDatabasePrefix,
            szIdentifier
        );
    }
    g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
}

bool TryAddPunishment(int iSlot, int iType, int iTime, std::string szReason, int iAdminID, bool bDB)
{
    if(iSlot < 0 || iSlot > 64) return false;
    switch (g_iImmunityType)
    {
    case 0:
        return AddPunishment(iSlot, iType, iTime, szReason, iAdminID, bDB);
        break;
    case 1:
        if(g_pAdmins[iAdminID].iImmunity < g_pAdmins[iSlot].iImmunity) return false;
        return AddPunishment(iSlot, iType, iTime, szReason, iAdminID, bDB);
        break;
    case 2:
        if(g_pAdmins[iAdminID].iImmunity <= g_pAdmins[iSlot].iImmunity) return false;
        return AddPunishment(iSlot, iType, iTime, szReason, iAdminID, bDB);
        break;
    case 3:
        if(g_pAdmins[iSlot].iImmunity > 0) return false;
        return AddPunishment(iSlot, iType, iTime, szReason, iAdminID, bDB);
        break;
    }
    return true;
}

void TryAddOfflinePunishment(const char* szSteamID64, const char* szName, int iType, int iTime, std::string szReason, int iAdminID)
{
    if(iAdminID == -1) {
        AddOfflinePunishment(szSteamID64, szName, iType, iTime, szReason, iAdminID);
        return;
    }
    char szQuery[1024], szServerID[64];
    g_SMAPI->Format(szServerID, sizeof(szServerID), " AND (server_id = %d OR server_id = -1)", g_iServerID[SID_PUNISH]);
    g_SMAPI->Format(szQuery, sizeof(szQuery), 
        "SELECT a_s.immunity, g.immunity AS group_immunity FROM %sadmins a\
        LEFT JOIN %sadmins_servers a_s ON a.id = a_s.admin_id\
        LEFT JOIN %sgroups g ON a_s.group_id = g.id\
        WHERE a.steamid = '%s'%s", g_szDatabasePrefix, g_szDatabasePrefix, g_szDatabasePrefix, szSteamID64, g_iServerID[SID_PUNISH] == -1 ? "" : szServerID);
    g_pConnection->Query(szQuery, [szSteamID64, szName, iType, iTime, szReason, iAdminID](ISQLQuery* query) {
        ISQLResult *result = query->GetResultSet();
        if (result->GetRowCount() > 0) {
            result->FetchRow();
            int iImmunity = result->GetInt(0);
            int iGroupImmunity = result->IsNull(1) ? 0 : result->GetInt(1);
            if(iGroupImmunity > iImmunity) iImmunity = iGroupImmunity;
            switch (g_iImmunityType)
            {
            case 0:
                AddOfflinePunishment(szSteamID64, szName, iType, iTime, szReason, iAdminID);
                break;
            case 1:
                if(g_pAdmins[iAdminID].iImmunity < iImmunity) return;
                AddOfflinePunishment(szSteamID64, szName, iType, iTime, szReason, iAdminID);
                break;
            case 2:
                if(g_pAdmins[iAdminID].iImmunity <= iImmunity) return;
                AddOfflinePunishment(szSteamID64, szName, iType, iTime, szReason, iAdminID);
                break;
            case 3:
                if(iImmunity > 0) return;
                AddOfflinePunishment(szSteamID64, szName, iType, iTime, szReason, iAdminID);
                break;
            }
        } else {
            AddOfflinePunishment(szSteamID64, szName, iType, iTime, szReason, iAdminID);
        }
    });
}

void TryRemovePunishment(int iSlot, int iType, int iAdminID)
{
    if(iSlot < 0 || iSlot > 64) return;
    bool bUnpunishAll = false;
    uint64 iSteamID = 0;
    if(iAdminID != -1) bUnpunishAll = true;
    else {
        bUnpunishAll = g_pAdminApi->HasPermission(iAdminID, "@admin/unpunish_all");
        iSteamID = g_pPlayers->GetSteamID64(iAdminID);
    }
    if(g_iUnpunishType == 0 && g_iAdminPunish[iSlot][iType] != iSteamID && !bUnpunishAll) return;
    RemovePunishment(iSlot, iType, iAdminID);
}

void TryRemoveOfflinePunishment(const char* szSteamID64, int iType, int iAdminID)
{
    if(iAdminID == -1) {
        RemoveOfflinePunishment(szSteamID64, iType, iAdminID);
        return;
    }
    bool bUnpunishAll = g_pAdminApi->HasPermission(iAdminID, "@admin/unpunish_all");
    char szQuery[512], szAdminID[32];
    g_SMAPI->Format(szAdminID, sizeof(szAdminID), " AND admin_id = %d", iAdminID == -1 ? 1 : g_pAdmins[iAdminID].iID);
    g_SMAPI->Format(szQuery, sizeof(szQuery), 
        "SELECT * FROM %spunishments WHERE steamid = '%s' AND punish_type = %d AND (expires > %d OR expires = 0) AND server_id = %d%s", g_szDatabasePrefix, szSteamID64, iType, std::time(0), g_iServerID[SID_PUNISH], g_iUnpunishType == 0 && iAdminID != -1 && !bUnpunishAll? szAdminID : "");
    g_pConnection->Query(szQuery, [szSteamID64, iType, iAdminID](ISQLQuery* query) {
        ISQLResult *result = query->GetResultSet();
        if (result->GetRowCount() > 0) {
            result->FetchRow();
            RemoveOfflinePunishment(szSteamID64, iType, iAdminID);
        }
    });
}

void RemoveExpiresAdmins()
{
    char szQuery[512];
    g_SMAPI->Format(szQuery, sizeof(szQuery), 
        "SELECT a.id, a.steamid, a.name, s.flags, s.immunity, s.expires, s.group_id, g.name AS group_name, g.flags AS group_flags, g.immunity AS group_immunity, s.server_id FROM %sadmins a LEFT JOIN %sadmins_servers s ON a.id = s.admin_id LEFT JOIN %sgroups g ON s.group_id = g.id WHERE s.expires > 0 AND s.expires < %d",
        g_szDatabasePrefix,
        g_szDatabasePrefix,
        g_szDatabasePrefix,
        std::time(0)
    );
    g_pConnection->Query(szQuery, [](ISQLQuery* query) {
        ISQLResult *result = query->GetResultSet();
        if (result->GetRowCount() > 0) {
            while (result->FetchRow()) {
                int iAdminID = result->GetInt(0);
                int iServerID = result->GetInt(10);
                char szQuery[512];
                g_SMAPI->Format(szQuery, sizeof(szQuery), 
                    "DELETE FROM %sadmins_servers WHERE admin_id = %d AND server_id = %d",
                    g_szDatabasePrefix,
                    iAdminID,
                    iServerID
                );
                g_pConnection->Query(szQuery, [](ISQLQuery* query) {});
            }
        }
    });
}