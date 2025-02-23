#include <stdio.h>
#include "admin_system.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include "database.h"
#include <sstream>

admin_system g_admin_system;
PLUGIN_EXPOSE(admin_system, g_admin_system);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IMySQLClient *g_pMysqlClient;
IMySQLConnection* g_pConnection;
IMenusApi* g_pMenus;
IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayers;

std::unordered_map<std::string, Category> mCategories;

// ADMIN SYSTEM API
AdminApi* g_pAdminApi = nullptr;
IAdminApi* g_pAdminCore = nullptr;

// TRANSLATIONS

std::map<std::string, std::string> g_vecPhrases;

// PLAYER DATA
int g_iPunishments[64][4];
uint64 g_iAdminPunish[64][4];
std::string g_szPunishReasons[64][4];
Admin g_pAdmins[64];

// SORTING
std::map<std::string, std::vector<std::string>> g_mSortItems;
std::vector<std::string> g_vSortCategories;

// CONFIG
int g_iServerID[2] = {0, 0}; // ServerID Admins, ServerID Punishments
const char* g_szDatabasePrefix = "as_";

std::map<std::string, Flag> g_mFlags;

int g_iBanDelay;

int g_iTime_Reason_Type;
//if time_reason_type = 0
std::vector<std::string> g_vReasons[4];
std::map<int, std::string> g_mTimes[4];
//if time_reason_type = 1
std::map<std::string, std::map<int, std::string>> g_mReasons[4];

int g_iNofityType = 0;
int g_iImmunityType = 0;
int g_iPunishOfflineCount = 0;
int g_iUnpunishType = 0;
int g_iUnpunishOfflineCount = 0;

bool g_bPunishIP = false;

bool g_bStaticNames = false;

int g_iMessageType;

std::vector<std::string> g_vecDefaultFlags;

std::map<uint64, OfflineUser> g_mOfflineUsers;

SH_DECL_HOOK6(IServerGameClients, ClientConnect, SH_NOATTRIB, 0, bool, CPlayerSlot, const char*, uint64, const char *, bool, CBufferString *);
SH_DECL_HOOK5_void(IServerGameClients, ClientDisconnect, SH_NOATTRIB, 0, CPlayerSlot, ENetworkDisconnectionReason, const char *, uint64, const char *);

bool containsOnlyDigits(const std::string& str) {
	return str.find_first_not_of("0123456789") == std::string::npos;
}

CON_COMMAND_EXTERN(mm_ban, PunishCommand, "Punish a player");
CON_COMMAND_EXTERN(mm_mute, PunishCommand, "Punish a player");
CON_COMMAND_EXTERN(mm_gag, PunishCommand, "Punish a player");
CON_COMMAND_EXTERN(mm_silence, PunishCommand, "Punish a player");
CON_COMMAND_EXTERN(mm_unban, UnPunishCommand, "Unpunish a player");
CON_COMMAND_EXTERN(mm_unmute, UnPunishCommand, "Unpunish a player");
CON_COMMAND_EXTERN(mm_ungag, UnPunishCommand, "Unpunish a player");
CON_COMMAND_EXTERN(mm_unsilence, UnPunishCommand, "Unpunish a player");
CON_COMMAND_EXTERN(mm_add_admin, AddAdminCommand, "Add an admin");
CON_COMMAND_EXTERN(mm_remove_admin, RemoveAdminCommand, "Remove an admin");
CON_COMMAND_EXTERN(mm_add_group, AddGroupCommand, "Add a group");
CON_COMMAND_EXTERN(mm_remove_group, RemoveGroupCommand, "Remove a group");

CON_COMMAND_EXTERN(mm_as_reload_config, OnReloadConfig, "Reload the admin system config");
CON_COMMAND_EXTERN(mm_as_reload_admin, OnReloadAdmin, "Reload the admin system admins");
CON_COMMAND_EXTERN(mm_as_reload_punish, OnReloadPunish, "Reload the admin system punishments");

void LoadConfig();
void LoadSorting();
void LoadTranslations();

void ResetPunishments(int iSlot)
{
	for(int i = 0; i < 4; i++)
	{
		g_iPunishments[iSlot][i] = -1;
		g_szPunishReasons[iSlot][i] = "";
		g_iAdminPunish[iSlot][i] = 0;
	}
	g_pAdmins[iSlot].iID = 0;
	g_pAdmins[iSlot].iImmunity = 0;
	g_pAdmins[iSlot].iExpireTime = -1;

}

void OnReloadPunish(const CCommandContext& context, const CCommand& args)
{
	int iAdmin = context.GetPlayerSlot().Get();
	bool bConsole = iAdmin == -1;
	if(!bConsole && !g_pAdminApi->HasPermission(iAdmin, "@admin/reload_punish"))
	{
		if(bConsole) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	if(args.ArgC() < 2 && OnlyDigits(args[1]))
	{
		if(bConsole) META_CONPRINTF("[Admin System] Usage: %s <steamid64>\n", args[0]);
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["UsageReloadPunish"].c_str(), args[0]);
		return;
	}
    bool bFound = false;
    int iSlot = -1;
    for(int i = 0; i < 64; i++)
    {
        CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
        if(!pPlayer) continue;
        if(g_pPlayers->GetSteamID64(i) == std::stoull(args[1]))
        {
            bFound = true;
            iSlot = i;
        }
    }
	if(bFound) {
		ResetPunishments(iSlot);
		g_pAdmins[iSlot].vFlags.clear();
		g_pAdmins[iSlot].vPermissions.clear();
		CheckPunishmentsForce(iSlot, std::stoull(args[1]));
	}
}

void OnReloadAdmin(const CCommandContext& context, const CCommand& args)
{
	int iAdmin = context.GetPlayerSlot().Get();
	bool bConsole = iAdmin == -1;
	if(!bConsole && !g_pAdminApi->HasPermission(iAdmin, "@admin/reload_admin"))
	{
		if(bConsole) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	if(args.ArgC() < 2 && OnlyDigits(args[1]))
	{
		if(bConsole) META_CONPRINTF("[Admin System] Usage: %s <steamid64>\n", args[0]);
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["UsageReloadAdmin"].c_str(), args[0]);
		return;
	}
    bool bFound = false;
    int iSlot = -1;
    for(int i = 0; i < 64; i++)
    {
        CCSPlayerController* pPlayer = CCSPlayerController::FromSlot(i);
        if(!pPlayer) continue;
        if(g_pPlayers->GetSteamID64(i) == std::stoull(args[1]))
        {
            bFound = true;
            iSlot = i;
        }
    }
	if(bFound) CheckPermissions(iSlot, std::stoull(args[1]));
}

void OnReloadConfig(const CCommandContext& context, const CCommand& args)
{
	int iAdmin = context.GetPlayerSlot().Get();
	bool bConsole = iAdmin == -1;
	if(!bConsole && !g_pAdminApi->HasPermission(iAdmin, "@admin/reload_config"))
	{
		if(bConsole) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	for (int i = 0; i < 4; i++)
	{
		g_mReasons[i].clear();
		g_vReasons[i].clear();
		g_mTimes[i].clear();
	}
	g_vecPhrases.clear();
	g_mFlags.clear();
	g_vSortCategories.clear();
	g_mSortItems.clear();
	g_vecDefaultFlags.clear();

	LoadConfig();
	LoadSorting();
	LoadTranslations();
	if(bConsole) META_CONPRINT("[Admin System] Config reloaded\n");
	else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["ConfigReloaded"].c_str());
}

void AddGroupCommand(const CCommandContext& context, const CCommand& args)
{
	int iAdmin = context.GetPlayerSlot().Get();
	bool bConsole = iAdmin == -1;
	if(!bConsole && !g_pAdminApi->HasPermission(iAdmin, "@admin/add_group"))
	{
		if(bConsole) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	if(args.ArgC() < 4)
	{
		if(bConsole) META_CONPRINTF("[Admin System] Usage: %s <name> <flags> <immunity>\n", args[0]);
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["UsageAddGroup"].c_str(), args[0]);
		return;
	}
	AddGroup(iAdmin, args[1], args[2], std::atoi(args[3]), true);
}

void RemoveGroupCommand(const CCommandContext& context, const CCommand& args)
{
	int iAdmin = context.GetPlayerSlot().Get();
	bool bConsole = iAdmin == -1;
	if(!bConsole && !g_pAdminApi->HasPermission(iAdmin, "@admin/remove_group"))
	{
		if(bConsole) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	if(args.ArgC() < 2)
	{
		if(bConsole) META_CONPRINTF("[Admin System] Usage: %s <id/name>\n", args[0]);
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["UsageRemoveGroup"].c_str(), args[0]);
		return;
	}
	RemoveGroup(args[1]);
	if(bConsole) META_CONPRINT("[Admin System] Group removed\n");
	else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["GroupRemoved"].c_str());
}

void AddNewAdmin(int iAdmin, const char* szFlag, const CCommand& args, bool bRemove, bool bConsole)
{
	if(iAdmin != -1 && !g_pAdminApi->HasPermission(iAdmin, szFlag))
	{
		if(bConsole) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	if(bRemove && ((bConsole && args.ArgC() < 2) || (!bConsole && args.ArgC() < 3)))
	{
		if(bConsole) META_CONPRINTF("[Admin System] Usage: %s <userid|steamid>\n", args[0]);
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["UsageRemoveAdmin"].c_str(), args[0]);
		return;
	}
	if(!bRemove && ((bConsole && args.ArgC() < 6) || (!bConsole && args.ArgC() < 7)))
	{
		if(bConsole) META_CONPRINTF("[Admin System] Usage: %s <userid|steamid> <name> <flags> <immunity> <time> <?group> <?comment>\n", args[0]);
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["UsageAddAdmin"].c_str(), args[0]);
		return;
	}
	bool bFound = false;
	char szSteamID[64];
	CCSPlayerController* pController;
	int iSlot = 0;
	for (int i = 0; i < 64; i++)
	{
		pController = CCSPlayerController::FromSlot(i);
		if (!pController) continue;
		uint m_steamID = pController->m_steamID();
		if(m_steamID == 0) continue;
		if(strcasestr(engine->GetClientConVarValue(i, "name"), args[1]) || (containsOnlyDigits(args[1]) && m_steamID == std::stoll(args[1])) || (containsOnlyDigits(args[1]) && std::stoll(args[1]) == i))
		{
			bFound = true;
			iSlot = i;
			g_SMAPI->Format(szSteamID, sizeof(szSteamID), "%llu", g_pPlayers->GetSteamID64(iSlot));
			break;
		}
	}
	if(bFound)
	{
		if(bRemove)
		{
			RemoveAdmin(szSteamID, true);
			if(bConsole) META_CONPRINT("[Admin System] Admin removed\n");
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["AdminRemoved"].c_str());
		}
		else
		{
			AddAdmin(strdup(args[2]), szSteamID, strdup(args[3]), std::atoi(args[4]), std::atoi(args[5]), std::atoi(args[6]), args.ArgC() < 7 ? "" : strdup(args[7]), true); 
			if(bConsole) META_CONPRINT("[Admin System] Admin added\n");
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["AdminAdded"].c_str());
		}
	}
	else if(containsOnlyDigits(args[1]) && std::string(args[1]).length() >= 17)
	{
		if(bRemove)
		{
			RemoveAdmin(args[1], true);
			if(bConsole) META_CONPRINT("[Admin System] Admin removed\n");
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["AdminRemoved"].c_str());
		}
		else
		{
			AddAdmin(strdup(args[2]), strdup(args[1]), strdup(args[3]), std::atoi(args[4]), std::atoi(args[5]), std::atoi(args[6]), args.ArgC() < 7 ? "" : strdup(args[7]), true);
			if(bConsole) META_CONPRINT("[Admin System] Admin added\n");
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["AdminAdded"].c_str());
		}
	}
	else
	{
		if(bConsole) META_CONPRINT("[Admin System] Player not found\n");
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["PlayerNotFound"].c_str());
	}
}

void AddAdminCommand(const CCommandContext& context, const CCommand& args)
{
	int iAdmin = context.GetPlayerSlot().Get();
	if(iAdmin != -1 && !g_pAdminApi->HasPermission(iAdmin, "@admin/add"))
	{
		if(iAdmin == -1) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	AddNewAdmin(iAdmin, "@admin/add", args, false, true);
}

void RemoveAdminCommand(const CCommandContext& context, const CCommand& args)
{
	int iAdmin = context.GetPlayerSlot().Get();
	if(iAdmin != -1 && !g_pAdminApi->HasPermission(iAdmin, "@admin/remove"))
	{
		if(iAdmin == -1) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	AddNewAdmin(iAdmin, "@admin/remove", args, true, true);
}

void TotalCommand(int iAdmin, int iType, const char* szFlag, const CCommand& args, bool bConsole, bool UnPunish)
{
	if(iAdmin != -1 && !g_pAdminApi->HasPermission(iAdmin, szFlag))
	{
		if(bConsole) {
			if(iAdmin == -1) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
			else g_pUtils->PrintToConsole(iAdmin, "[Admin System] You don't have permission to use this command\n");
		}
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	if ((UnPunish && args.ArgC() < 2+!bConsole) || (!UnPunish && args.ArgC() < 4+!bConsole)) 
	{
		if(bConsole)
		{
			if(UnPunish) {
				if(iAdmin == -1) META_CONPRINTF("[Admin System] Usage: %s <userid|steamid>\n", args[0]);
				else g_pUtils->PrintToConsole(iAdmin, "[Admin System] Usage: %s <userid|steamid>\n", args[0]);
			}
			else {
				if(iAdmin == -1) META_CONPRINTF("[Admin System] Usage: %s <userid|steamid> <time> <reason>\n", args[0]);
				else g_pUtils->PrintToConsole(iAdmin, "[Admin System] Usage: %s <userid|steamid> <time> <reason>\n", args[0]);
			}
		}
		else
		{
			if(UnPunish) g_pUtils->PrintToChat(iAdmin, g_vecPhrases["UsageUnPunish"].c_str(), args[0]);
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["UsagePunish"].c_str(), args[0]);
		}
		return;
	}
	std::string szReason = args.ArgS();
    std::string arg1 = args[1];
    std::string arg2 = args[2];
	bool bFound = false;
	int iSlot = 0;
	for (int i = 0; i < 64; i++)
	{
		if(g_pPlayers->IsFakeClient(i)) continue;
		uint64 m_steamID = g_pPlayers->GetSteamID64(i);
		if(m_steamID == 0) continue;
		if(containsOnlyDigits(arg1)) {
			if(arg1.length() <= 2 && atoi(arg1.c_str()) == i)
			{
				bFound = true;
				iSlot = i;
				break;
			} else if(m_steamID == std::stoull(arg1)) {
				bFound = true;
				iSlot = i;
				break;
			}
		} else if(strcasestr(engine->GetClientConVarValue(i, "name"), args[1])) {
			bFound = true;
			iSlot = i;
			break;
		}
	}
	int iTime = std::atoi(args[2]);
    szReason.erase(0, arg1.length() + arg2.length() + 2);
	if(!bConsole && szReason.length() > 0)
		szReason.pop_back();
	if(bFound)
	{
		if(UnPunish)
		{
			TryRemovePunishment(iSlot, iType, iAdmin);
			if(bConsole) {
				if(iAdmin == -1) META_CONPRINT("[Admin System] Player unpunished\n");
				else g_pUtils->PrintToConsole(iAdmin, "[Admin System] Player unpunished\n");
			}
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["PlayerUnPunished"].c_str());
		}
		else
		{
			TryAddPunishment(iSlot, iType, iTime, szReason, iAdmin, true);
			if(bConsole) {
				if(iAdmin == -1) META_CONPRINT("[Admin System] Player punished\n");
				else g_pUtils->PrintToConsole(iAdmin, "[Admin System] Player punished\n");
			}
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["PlayerPunished"].c_str());
		}
	}
	else if(containsOnlyDigits(arg1) && arg1.length() >= 17)
	{
		if(UnPunish)
		{
			TryRemoveOfflinePunishment(strdup(args[1]), iType, iAdmin);
			if(bConsole) {
				if(iAdmin == -1) META_CONPRINT("[Admin System] Player unpunished\n");
				else g_pUtils->PrintToConsole(iAdmin, "[Admin System] Player unpunished\n");
			}
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["PlayerUnPunished"].c_str());
		}
		else
		{
			TryAddOfflinePunishment(strdup(args[1]), "undefined", iType, iTime, szReason, iAdmin);
			if(bConsole) {
				if(iAdmin == -1) META_CONPRINT("[Admin System] Player punished\n");
				else g_pUtils->PrintToConsole(iAdmin, "[Admin System] Player punished\n");
			}
			else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["PlayerPunished"].c_str());
		}
	}
	else
	{
		if(bConsole) {
			if(iAdmin == -1) META_CONPRINT("[Admin System] Player not found\n");
			else g_pUtils->PrintToConsole(iAdmin, "[Admin System] Player not found\n");
		}
		else g_pUtils->PrintToChat(iAdmin, g_vecPhrases["PlayerNotFound"].c_str());
	}
}

void UnPunishCommand(const CCommandContext& context, const CCommand& args)
{
	int iType = 0;
	const char* szFlag = nullptr;
	const char* szCommand = args[0];
	if (strcmp(szCommand, "mm_unban") == 0) {
		iType = RT_BAN;
		szFlag = "@admin/unban";
	}
	else if (strcmp(szCommand, "mm_unmute") == 0) {
		iType = RT_MUTE;
		szFlag = "@admin/unmute";
	}
	else if (strcmp(szCommand, "mm_ungag") == 0) {
		iType = RT_GAG;
		szFlag = "@admin/ungag";
	}
	else if (strcmp(szCommand, "mm_unsilence") == 0) {
		iType = RT_SILENCE;
		szFlag = "@admin/unsilence";
	}
	int iAdmin = context.GetPlayerSlot().Get();
	if(iAdmin != -1 && !g_pAdminApi->HasPermission(iAdmin, szFlag))
	{
		if(iAdmin == -1) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	TotalCommand(iAdmin, iType, szFlag, args, true, true);
}

void PunishCommand(const CCommandContext& context, const CCommand& args)
{
	int iType = 0;
	const char* szFlag = nullptr;
	const char* szCommand = args[0];
	if (strcmp(szCommand, "mm_ban") == 0) {
		iType = RT_BAN;
		szFlag = "@admin/ban";
	}
	else if (strcmp(szCommand, "mm_mute") == 0) {
		iType = RT_MUTE;
		szFlag = "@admin/mute";
	}
	else if (strcmp(szCommand, "mm_gag") == 0) {
		iType = RT_GAG;
		szFlag = "@admin/gag";
	}
	else if (strcmp(szCommand, "mm_silence") == 0) {
		iType = RT_SILENCE;
		szFlag = "@admin/silence";
	}
	int iAdmin = context.GetPlayerSlot().Get();
	if(iAdmin != -1 && !g_pAdminApi->HasPermission(iAdmin, szFlag))
	{
		if(iAdmin == -1) META_CONPRINT("[Admin System] You don't have permission to use this command\n");
		else g_pUtils->PrintToConsole(iAdmin, g_vecPhrases["NoPermission"].c_str());
		return;
	}
	TotalCommand(iAdmin, iType, szFlag, args, true, false);
}

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();

	if(g_pConnection) RemoveExpiresAdmins();
}

std::vector<std::string> GetPermissionsByFlag(const char* szFlag)
{
	return g_pAdminApi->GetPermissionsByFlag(szFlag);
}

std::string FormatTime(int iTimestamp)
{
    if (iTimestamp == 0) return g_vecPhrases["Never"];
    int iSeconds = iTimestamp - std::time(nullptr);
    if (iSeconds < 0) return g_vecPhrases["Expired"];
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
    if (iSeconds > 0)
        parts += std::to_string(iSeconds) + g_vecPhrases["Seconds"];

    return parts;
}

bool IsClientMutedOrSilenced(int iSlot)
{
    int muteTime = g_iPunishments[iSlot][RT_MUTE];
    if (muteTime != -1) {
        if (muteTime == 0 || muteTime > std::time(nullptr)) return true;
    }
    int silenceTime = g_iPunishments[iSlot][RT_SILENCE];
    if (silenceTime != -1) {
        if (silenceTime == 0 || silenceTime > std::time(nullptr)) return true;
    }
    return false;
}

bool IsClientGaggedOrSilenced(int iSlot)
{
	int gagTime = g_iPunishments[iSlot][RT_GAG];
	if (gagTime != -1) {
		if (gagTime == 0 || gagTime > std::time(nullptr)) return true;
	}
	int silenceTime = g_iPunishments[iSlot][RT_SILENCE];
	if (silenceTime != -1) {
		if (silenceTime == 0 || silenceTime > std::time(nullptr)) return true;
	}
	return false;
}

bool OnHearingClient(int iSlot)
{
	bool bMuted = IsClientMutedOrSilenced(iSlot);
	if(bMuted)
	{
		int iType = g_iPunishments[iSlot][RT_MUTE] != -1 ? RT_MUTE : RT_SILENCE;
		g_pUtils->PrintToAlert(iSlot, g_vecPhrases["MuteActiveCenter"].c_str(), g_szPunishReasons[iSlot][iType].c_str(), FormatTime(g_iPunishments[iSlot][iType]).c_str());
	}
	return !bMuted;
}

bool admin_system::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetServerFactory, g_pSource2GameClients, IServerGameClients, SOURCE2GAMECLIENTS_INTERFACE_VERSION);
	
	SH_ADD_HOOK_MEMFUNC(IServerGameClients, ClientConnect, g_pSource2GameClients, this, &admin_system::OnClientConnect, false );
	SH_ADD_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &admin_system::OnClientDisconnect), true);

	g_SMAPI->AddListener( this, this );
	
	g_pAdminApi = new AdminApi();
	g_pAdminCore = g_pAdminApi;
	
	ConVar_Register(FCVAR_RELEASE | FCVAR_CLIENT_CAN_EXECUTE | FCVAR_SERVER_CAN_EXECUTE | FCVAR_GAMEDLL);

	return true;
}

void* admin_system::OnMetamodQuery(const char* iface, int* ret)
{
	if (!strcmp(iface, Admin_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pAdminCore;
	}

	*ret = META_IFACE_FAILED;
	return nullptr;
}

bool admin_system::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(IServerGameClients, ClientConnect, g_pSource2GameClients, this, &admin_system::OnClientConnect, false);
	SH_REMOVE_HOOK(IServerGameClients, ClientDisconnect, g_pSource2GameClients, SH_MEMBER(this, &admin_system::OnClientDisconnect), true);
	ConVar_Unregister();
	
	return true;
}

bool admin_system::OnClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason)
{
	int iSlot = slot.Get();
	if(xuid > 0)
	{
		ResetPunishments(iSlot);
		CheckPunishments(iSlot, xuid);
	}
	RETURN_META_VALUE(MRES_IGNORED, true);
}

void LoadSorting()
{
	KeyValues* pKVConfig = new KeyValues("Config");
    if (!pKVConfig->LoadFromFile(g_pFullFileSystem, "addons/configs/admin_system/sorting.ini")) {
        g_pUtils->ErrorLog("[%s] Failed to load config addons/configs/admin_system/sorting.ini", g_PLAPI->GetLogTag());
        return;
    }

	FOR_EACH_SUBKEY(pKVConfig, pKVCategory)
	{
		std::string szCategory = pKVCategory->GetName();
		g_vSortCategories.push_back(szCategory);
		FOR_EACH_VALUE(pKVCategory, pKVItems)
		{
			g_mSortItems[szCategory].push_back(pKVItems->GetString(nullptr));
		}
	}
}

void LoadConfig()
{
	KeyValues* pKVConfig = new KeyValues("Config");
    if (!pKVConfig->LoadFromFile(g_pFullFileSystem, "addons/configs/admin_system/core.ini")) {
        g_pUtils->ErrorLog("[%s] Failed to load config addons/configs/admin_system/core.ini", g_PLAPI->GetLogTag());
        return;
    }
	g_szDatabasePrefix = pKVConfig->GetString("database_prefix", "as_");

	g_iServerID[SID_ADMIN] = pKVConfig->GetInt("admin_server_id", -1);
	g_iServerID[SID_PUNISH] = pKVConfig->GetInt("punish_server_id", -1);

	g_bPunishIP = pKVConfig->GetBool("punish_ip", false);

	g_iTime_Reason_Type = pKVConfig->GetInt("time_reason_type", 0);

	g_iBanDelay = pKVConfig->GetInt("ban_delay", 0);

	g_iNofityType = pKVConfig->GetInt("notify_type", 0);

	g_iUnpunishType = pKVConfig->GetInt("unpunish_type", 0);

	g_iImmunityType = pKVConfig->GetInt("immunity_type", 0);

	g_iPunishOfflineCount = pKVConfig->GetInt("punish_offline_count", 0);

	g_iUnpunishOfflineCount = pKVConfig->GetInt("unpunish_offline_count", 0);

	g_bStaticNames = pKVConfig->GetBool("static_name", false);

	g_iMessageType = pKVConfig->GetInt("message_type");

	KeyValues* pKVDefaultPermissions = pKVConfig->FindKey("default_permissions", false);
	if(pKVDefaultPermissions)
	{
		FOR_EACH_VALUE(pKVDefaultPermissions, pKVFlag)
		{
			if(pKVFlag->GetInt(nullptr, 0))
			{
				g_vecDefaultFlags.push_back(pKVFlag->GetName());
			}
		}
	}

	KeyValues* pKVFlags = pKVConfig->FindKey("flags", false);
	if(pKVFlags)
	{
		FOR_EACH_SUBKEY(pKVFlags, pKVFlag)
		{
			Flag flag;
			flag.szName = pKVFlag->GetString("name");
			KeyValues* pKVAccesses = pKVFlag->FindKey("access", false);
			FOR_EACH_VALUE(pKVAccesses, pKVAccess)
			{
				bool bAccess = pKVAccess->GetInt(nullptr, 0);
				if(bAccess) flag.vPermissions.push_back(pKVAccess->GetName());
			}
			g_mFlags[pKVFlag->GetName()] = flag;
		}
	}

	if(g_iTime_Reason_Type)
	{
		KeyValues* pKVReasonsTimes = pKVConfig->FindKey("times_reasons", false);
		if(pKVReasonsTimes)
		{
			KeyValues* pKVBan = pKVReasonsTimes->FindKey("bans", false);
			if(pKVBan)
			{
				FOR_EACH_SUBKEY(pKVBan, pKVTime)
				{
					const char* szReason = pKVTime->GetName();
					FOR_EACH_VALUE(pKVTime, pKVTimeValue)
					{
						g_mReasons[RT_BAN][szReason][std::atoi(pKVTimeValue->GetName())] = pKVTimeValue->GetString(nullptr);
					}
				}
			}

			KeyValues* pKVMute = pKVReasonsTimes->FindKey("mutes", false);
			if(pKVMute)
			{
				FOR_EACH_SUBKEY(pKVMute, pKVTime)
				{
					const char* szReason = pKVTime->GetName();
					FOR_EACH_VALUE(pKVTime, pKVTimeValue)
					{
						g_mReasons[RT_MUTE][szReason][std::atoi(pKVTimeValue->GetName())] = pKVTimeValue->GetString(nullptr);
					}
				}
			}

			KeyValues* pKVGag = pKVReasonsTimes->FindKey("gags", false);
			if(pKVGag)
			{
				FOR_EACH_SUBKEY(pKVGag, pKVTime)
				{
					const char* szReason = pKVTime->GetName();
					FOR_EACH_VALUE(pKVTime, pKVTimeValue)
					{
						g_mReasons[RT_GAG][szReason][std::atoi(pKVTimeValue->GetName())] = pKVTimeValue->GetString(nullptr);
					}
				}
			}

			KeyValues* pKVSilence = pKVReasonsTimes->FindKey("silences", false);
			if(pKVSilence)
			{
				FOR_EACH_SUBKEY(pKVSilence, pKVTime)
				{
					const char* szReason = pKVTime->GetName();
					FOR_EACH_VALUE(pKVTime, pKVTimeValue)
					{
						g_mReasons[RT_SILENCE][szReason][std::atoi(pKVTimeValue->GetName())] = pKVTimeValue->GetString(nullptr);
					}
				}
			}
		}
	}
	else
	{
		KeyValues* pKVReasons = pKVConfig->FindKey("reasons", false);
		if(pKVReasons)
		{
			KeyValues* pKVBan = pKVReasons->FindKey("bans", false);
			if(pKVBan)
			{
				FOR_EACH_VALUE(pKVBan, pKVReason)
				{
					bool bToogle = pKVReason->GetInt(nullptr, 0);
					if(bToogle) g_vReasons[RT_BAN].push_back(pKVReason->GetName());
				}
			}
			KeyValues* pKVMute = pKVReasons->FindKey("mutes", false);
			if(pKVMute)
			{
				FOR_EACH_VALUE(pKVMute, pKVReason)
				{
					bool bToogle = pKVReason->GetInt(nullptr, 0);
					if(bToogle) g_vReasons[RT_MUTE].push_back(pKVReason->GetName());
				}
			}
			KeyValues* pKVGag = pKVReasons->FindKey("gags", false);
			if(pKVGag)
			{
				FOR_EACH_VALUE(pKVGag, pKVReason)
				{
					bool bToogle = pKVReason->GetInt(nullptr, 0);
					if(bToogle) g_vReasons[RT_GAG].push_back(pKVReason->GetName());
				}
			}
			KeyValues* pKVSilence = pKVReasons->FindKey("silences", false);
			if(pKVSilence)
			{
				FOR_EACH_VALUE(pKVSilence, pKVReason)
				{
					bool bToogle = pKVReason->GetInt(nullptr, 0);
					if(bToogle) g_vReasons[RT_SILENCE].push_back(pKVReason->GetName());
				}
			}
		}
		KeyValues* pKVTimes = pKVConfig->FindKey("times", false);
		if(pKVTimes)
		{
			KeyValues* pKVBan = pKVTimes->FindKey("bans", false);
			if(pKVBan)
			{
				FOR_EACH_VALUE(pKVBan, pKVTime)
				{
					g_mTimes[RT_BAN][std::atoi(pKVTime->GetName())] = pKVTime->GetString(nullptr);
				}
			}
			KeyValues* pKVMute = pKVTimes->FindKey("mutes", false);
			if(pKVMute)
			{
				FOR_EACH_VALUE(pKVMute, pKVTime)
				{
					g_mTimes[RT_MUTE][std::atoi(pKVTime->GetName())] = pKVTime->GetString(nullptr);
				}
			}
			KeyValues* pKVGag = pKVTimes->FindKey("gags", false);
			if(pKVGag)
			{
				FOR_EACH_VALUE(pKVGag, pKVTime)
				{
					g_mTimes[RT_GAG][std::atoi(pKVTime->GetName())] = pKVTime->GetString(nullptr);
				}
			}
			KeyValues* pKVSilence = pKVTimes->FindKey("silences", false);
			if(pKVSilence)
			{
				FOR_EACH_VALUE(pKVSilence, pKVTime)
				{
					g_mTimes[RT_SILENCE][std::atoi(pKVTime->GetName())] = pKVTime->GetString(nullptr);
				}
			}
		}
	}
}

void LoadTranslations()
{
	KeyValues* kvPhrases = new KeyValues("Phrases");
	const char *pszPath = "addons/translations/admin_system.phrases.txt";

	if (!kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
	{
		Warning("Failed to load %s\n", pszPath);
		return;
	}
	const char* pszLanguage = g_pUtils->GetLanguage();
	for (KeyValues *pKey = kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
		g_vecPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(pszLanguage));
}

bool OnChatPre(int iSlot, const char* szContent, bool bTeam)
{
	if(IsClientGaggedOrSilenced(iSlot))
	{
		if(strstr(szContent, "!") || strstr(szContent, "/") || !strcmp(szContent, "\"\"")) return false;
		int iType = g_iPunishments[iSlot][RT_GAG] != -1 ? RT_GAG : RT_SILENCE;
		g_pUtils->PrintToChat(iSlot, g_vecPhrases["MuteActiveChat"].c_str(), g_szPunishReasons[iSlot][iType].c_str(), FormatTime(g_iPunishments[iSlot][iType]).c_str());
		return false;
	}
	return true;
}

void OnClientAuthorized(int iSlot, uint64 xuid)
{
	if(g_mOfflineUsers.find(xuid) != g_mOfflineUsers.end())
		g_mOfflineUsers.erase(xuid);
	CheckPunishments(iSlot, xuid);
	CheckPermissions(iSlot, xuid);
}

void admin_system::OnClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID)
{
	if(xuid <= 0) return;
	int iSlot = slot.Get();
	OfflineUser user;
	user.iSteamID64 = xuid;
	user.szName = pszName;
	for(int i = 0; i < 4; i++)
	{
		if(g_iPunishments[iSlot][i] != -1) 
			user.bPunished[i] = true;
		else
			user.bPunished[i] = false;
	}
	for(int i = 0; i < 4; i++)
	{
		user.iAdminID[i] = g_iAdminPunish[iSlot][i];
	}
	g_mOfflineUsers[xuid] = user;
	if(g_mOfflineUsers.size() > g_iPunishOfflineCount)
	{
		auto it = g_mOfflineUsers.begin();
		g_mOfflineUsers.erase(it);
	}
	ResetPunishments(iSlot);
	g_pAdmins[iSlot].vFlags.clear();
	g_pAdmins[iSlot].vPermissions.clear();
}

void admin_system::AllPluginsLoaded()
{
	char error[64];
	int ret;
	g_pUtils = (IUtilsApi *)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Utils system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pMenus = (IMenusApi *)g_SMAPI->MetaFactory(Menus_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Menus system plugin", g_PLAPI->GetLogTag());
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pPlayers = (IPlayersApi *)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_pUtils->ErrorLog("[%s] Missing Players system plugin", g_PLAPI->GetLogTag());
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	ISQLInterface* g_SqlInterface = (ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, &ret, nullptr);
	if (ret == META_IFACE_FAILED) {
		g_pUtils->ErrorLog("[%s] Missing MYSQL plugin", g_PLAPI->GetLogTag());
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pMysqlClient = g_SqlInterface->GetMySQLClient();

	LoadConfig();
	LoadSorting();
	LoadTranslations();
	CreateConnection();

	g_pAdminCore->RegisterCategory("punishments", 	"Category_Punishments", 		nullptr);
	g_pAdminCore->RegisterItem("punish", 			"Item_PunishPlayer", 			"punishments", "@admin/ban|@admin/mute|@admin/gag|@admin/silence", 			nullptr, OnPunishSelect);
	g_pAdminCore->RegisterItem("unpunish", 			"Item_UnPunishPlayer", 		"punishments", "@admin/unban|@admin/unmute|@admin/ungag|@admin/unsilence", 	nullptr, OnUnPunishSelect);
	g_pAdminCore->RegisterItem("punish_offline", 	"Item_PunishOfflinePlayer", 	"punishments", "@admin/ban|@admin/mute|@admin/gag|@admin/silence", 			nullptr, OnPunishOfflineSelect);
	g_pAdminCore->RegisterItem("unpunish_offline", 	"Item_UnPunishOfflinePlayer", "punishments", "@admin/unban|@admin/unmute|@admin/ungag|@admin/unsilence", 	nullptr, OnUnPunishOfflineSelect);

	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pUtils->HookIsHearingClient(g_PLID, OnHearingClient);
	g_pUtils->AddChatListenerPre(g_PLID, OnChatPre);
	g_pPlayers->HookOnClientAuthorized(g_PLID, OnClientAuthorized);

	if(g_bStaticNames)
	{
		g_pUtils->RegCommand(g_PLID, {"jointeam"}, {}, [](int iSlot, const char* szContent){
			if(g_pAdmins[iSlot].iID > 0)
			{
				g_pPlayers->SetPlayerName(iSlot, g_pAdmins[iSlot].szName.c_str());
			}
			return false;
		});
		g_pUtils->HookEvent(g_PLID, "round_start", [](const char* szName, IGameEvent* pEvent, bool bDontBroadcast) {
			for (int i = 0; i < 64; i++)
			{
				if(g_pAdmins[i].iID > 0)
				{
					g_pPlayers->SetPlayerName(i, g_pAdmins[i].szName.c_str());
				}
			}
		});
	}

	g_pUtils->CreateTimer(1.0f, [](){
		for (int i = 0; i < 64; i++)
		{
			if(g_pPlayers->IsFakeClient(i)) continue;
			for (int j = 1; j < 4; j++)
			{
				if (g_iPunishments[i][j] == -1) continue;
				if (g_iPunishments[i][j] == 0) continue;
				if (g_iPunishments[i][j] < std::time(nullptr))
				{
					g_iPunishments[i][j] = -1;
					g_szPunishReasons[i][j] = "";
					g_pUtils->PrintToChat(i, g_vecPhrases["MuteExpired"].c_str());
					g_pAdminApi->OnPlayerUnpunishSend(i, j, -1);
				}
			}

			if (g_pAdmins[i].iID > 0 && g_pAdmins[i].iExpireTime != 0 && g_pAdmins[i].iExpireTime < std::time(nullptr))
			{
				g_pAdmins[i].iID = 0;
				g_pAdmins[i].iImmunity = 0;
				g_pAdmins[i].iExpireTime = -1;
				g_pAdmins[i].vFlags.clear();
				g_pAdmins[i].vPermissions.clear();
				g_pUtils->PrintToChat(i, g_vecPhrases["AdminExpired"].c_str());
			}
		}
		return 1.0f;
	});

	g_pUtils->RegCommand(g_PLID, {"mm_admin", "css_admin"}, {"!admin"}, OnAdminMenu);

	g_pUtils->RegCommand(g_PLID, {}, {"!ban", "!mute", "!gag", "!silence", "!unban", "!unmute", "!ungag", "!unsilence"}, [](int iSlot, const char* szContent){
		CCommand arg;
		arg.Tokenize(szContent);
		int iType = 0;
		const char* szFlag = nullptr;
		const char* szCommand = arg[0];
		if (strcmp(szCommand, "!ban") == 0) {
			iType = RT_BAN;
			szFlag = "@admin/ban";
		}
		else if (strcmp(szCommand, "!mute") == 0) {
			iType = RT_MUTE;
			szFlag = "@admin/mute";
		}
		else if (strcmp(szCommand, "!gag") == 0) {
			iType = RT_GAG;
			szFlag = "@admin/gag";
		}
		else if (strcmp(szCommand, "!silence") == 0) {
			iType = RT_SILENCE;
			szFlag = "@admin/silence";
		}
		else if (strcmp(szCommand, "!unban") == 0) {
			iType = RT_BAN;
			szFlag = "@admin/unban";
		}
		else if (strcmp(szCommand, "!unmute") == 0) {
			iType = RT_MUTE;
			szFlag = "@admin/unmute";
		}
		else if (strcmp(szCommand, "!ungag") == 0) {
			iType = RT_GAG;
			szFlag = "@admin/ungag";
		}
		else if (strcmp(szCommand, "!unsilence") == 0) {
			iType = RT_SILENCE;
			szFlag = "@admin/unsilence";
		}
		if(!g_pAdminApi->HasPermission(iSlot, szFlag))
		{
			g_pUtils->PrintToChat(iSlot, g_vecPhrases["NoPermission"].c_str());
			return true;
		}
		TotalCommand(iSlot, iType, szFlag, arg, false, arg[0][1] == 'u');		
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!add_admin", "!remove_admin"}, [](int iSlot, const char* szContent){
		CCommand arg;
		arg.Tokenize(szContent);
		const char* szFlag = nullptr;
		if(strcmp(arg[0], "!add_admin") == 0)
		{
			if(!g_pAdminApi->HasPermission(iSlot, "@admin/add_admin"))
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["NoPermission"].c_str());
				return true;
			}
			if(arg.ArgC() < 7)
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["UsageAddAdmin"].c_str(), arg[0]);
				return true;
			}
			AddNewAdmin(iSlot, "@admin/add", arg, false, false);
		}
		else
		{
			if(!g_pAdminApi->HasPermission(iSlot, "@admin/remove_admin"))
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["NoPermission"].c_str());
				return true;
			}
			if(arg.ArgC() < 3)
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["UsageRemoveAdmin"].c_str(), arg[0]);
				return true;
			}
			AddNewAdmin(iSlot, "@admin/remove", arg, true, false);
		}
		return true;
	});

	g_pUtils->RegCommand(g_PLID, {}, {"!add_group", "!remove_group"}, [](int iSlot, const char* szContent){
		CCommand arg;
		arg.Tokenize(szContent);
		const char* szFlag = nullptr;
		if(strcmp(arg[0], "!add_group") == 0)
		{
			if(!g_pAdminApi->HasPermission(iSlot, "@admin/add_group"))
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["NoPermission"].c_str());
				return true;
			} 
			if(arg.ArgC() < 5)
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["UsageAddGroup"].c_str(), arg[0]);
				return true;
			}
			AddGroup(iSlot, arg[1], arg[2], atoi(arg[3]), false);
		}
		else
		{
			if(!g_pAdminApi->HasPermission(iSlot, "@admin/remove_group"))
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["NoPermission"].c_str());
				return true;
			}
			if(arg.ArgC() < 3)
			{
				g_pUtils->PrintToChat(iSlot, g_vecPhrases["UsageRemoveGroup"].c_str(), arg[0]);
				return true;
			}
			RemoveGroup(arg[1]);
			g_pUtils->PrintToChat(iSlot, g_vecPhrases["GroupRemoved"].c_str());
		}
		return true;
	});
}

bool HasAccessInItem(int iSlot, const char* szCategory, const char* szItem)
{
	if(g_pAdmins[iSlot].vPermissions.empty()) return false;
	for (const auto& item : mCategories[szCategory].mItems)
	{
		const std::string& key = item.first;
		const Item& _item = item.second;
		if(key == szItem)
		{
			for (const auto& flag : _item.vecFlags) {
				if (std::find(g_pAdmins[iSlot].vPermissions.begin(), g_pAdmins[iSlot].vPermissions.end(), flag) != g_pAdmins[iSlot].vPermissions.end() || 
					std::find(g_pAdmins[iSlot].vPermissions.begin(), g_pAdmins[iSlot].vPermissions.end(), "@admin/root") != g_pAdmins[iSlot].vPermissions.end()) {
					return true;
				}
			}
		}
	}
	return false;
}

bool HasAccessInCategory(int iSlot, const char* szCategory)
{
	if(g_pAdmins[iSlot].vPermissions.empty()) return false;
	for (const auto& item : mCategories[szCategory].mItems)
	{
		const std::string& key = item.first;
		const Item& _item = item.second;
		for (const auto& flag : _item.vecFlags) {
			if (std::find(g_pAdmins[iSlot].vPermissions.begin(), g_pAdmins[iSlot].vPermissions.end(), flag) != g_pAdmins[iSlot].vPermissions.end() || 
				std::find(g_pAdmins[iSlot].vPermissions.begin(), g_pAdmins[iSlot].vPermissions.end(), "@admin/root") != g_pAdmins[iSlot].vPermissions.end()) {
				return true;
			}
		}
	}
	return false;
}

bool AdminApi::HasPermission(int iSlot, const char* szPermission)
{
    if (std::find(g_pAdmins[iSlot].vPermissions.begin(), g_pAdmins[iSlot].vPermissions.end(), szPermission) != g_pAdmins[iSlot].vPermissions.end() ||
		std::find(g_pAdmins[iSlot].vPermissions.begin(), g_pAdmins[iSlot].vPermissions.end(), "@admin/root") != g_pAdmins[iSlot].vPermissions.end() ||
		std::find(g_vecDefaultFlags.begin(), g_vecDefaultFlags.end(), szPermission) != g_vecDefaultFlags.end())
        return true;

    return false;
}

bool AdminApi::HasFlag(int iSlot, const char* szFlag)
{
	if (g_pAdmins[iSlot].vFlags.empty()) return false;
	if (std::find(g_pAdmins[iSlot].vFlags.begin(), g_pAdmins[iSlot].vFlags.end(), szFlag) != g_pAdmins[iSlot].vFlags.end()) {
		return true;
	}

	return false;
}

bool AdminApi::IsAdmin(int iSlot)
{
	return !g_pAdmins[iSlot].vFlags.empty();
}

std::vector<std::string> AdminApi::GetPermissionsByFlag(const char* szFlag)
{
	std::vector<std::string> vPermissions;
	if(g_mFlags.find(szFlag) != g_mFlags.end())
	{
		vPermissions = g_mFlags[szFlag].vPermissions;
	}
	return vPermissions;
}

std::vector<std::string> AdminApi::GetAdminFlags(int iSlot)
{
	return g_pAdmins[iSlot].vFlags;
}

std::vector<std::string> AdminApi::GetAdminPermissions(int iSlot)
{
	return g_pAdmins[iSlot].vPermissions;
}

int AdminApi::GetAdminImmunity(int iSlot)
{
	return g_pAdmins[iSlot].iImmunity;
}

bool AdminApi::IsPlayerPunished(int iSlot, int iType)
{
	return g_iPunishments[iSlot][iType] != -1;
}

int AdminApi::GetPlayerPunishmentExpired(int iSlot, int iType)
{
	return g_iPunishments[iSlot][iType];
}

const char* AdminApi::GetPlayerPunishmentReason(int iSlot, int iType)
{
	return g_szPunishReasons[iSlot][iType].c_str();
}

void SendPunishmentNotification(int iSlot, int iType, int iTime, const char* szReason, int iAdminID)
{
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
		g_pUtils->PrintToChat(iSlot, szMessage.c_str(), FormatTime(iTime).c_str(), szReason, iAdminID == -1?"Console":engine->GetClientConVarValue(iAdminID, "name"));
		if(iAdminID != -1)
		{
			switch (iType)
			{
			case RT_BAN:
				g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewBanMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatTime(iTime).c_str(), szReason);
				break;
			case RT_MUTE:
				g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewMuteMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatTime(iTime).c_str(), szReason);
				break;
			case RT_GAG:
				g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewGagMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatTime(iTime).c_str(), szReason);
				break;
			case RT_SILENCE:
				g_pUtils->PrintToChat(iAdminID, g_vecPhrases["NewSilenceMessageAdmin"].c_str(), engine->GetClientConVarValue(iSlot, "name"), FormatTime(iTime).c_str(), szReason);
				break;
			}
		}
		break;
	case 2:
		g_pUtils->PrintToChatAll(szMessage.c_str(), iAdminID == -1?"Console":engine->GetClientConVarValue(iAdminID, "name"), engine->GetClientConVarValue(iSlot, "name"), FormatTime(iTime).c_str(), szReason);
		break;
	}
}

void AdminApi::AddPlayerPunishment(int iSlot, int iType, int iTime, const char* szReason, int iAdminID, bool bNotify, bool bDB)
{
	int iTimeExpire = iTime == 0 ? 0 : std::time(nullptr) + iTime;
	if(bNotify) SendPunishmentNotification(iSlot, iType, iTimeExpire, szReason, iAdminID);
	AddPunishment(iSlot, iType, iTime, szReason, iAdminID, bDB);
}

void AdminApi::AddOfflinePlayerPunishment(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID)
{
	AddOfflinePunishment(szSteamID64, szName, iType, iTime, szReason, iAdminID);
}

void AdminApi::RemovePlayerPunishment(int iSlot, int iType, int iAdminID, bool bNotify)
{
	if(bNotify && g_iNofityType == 2)
	{
		std::string szMessage = "";
		switch (iType)
		{
		case RT_BAN:
			szMessage = g_vecPhrases["UnbanMessageAll"];
			break;
		case RT_MUTE:
			szMessage = g_vecPhrases["UnmuteMessageAll"];
			break;
		case RT_GAG:
			szMessage = g_vecPhrases["UngagMessageAll"];
			break;
		case RT_SILENCE:
			szMessage = g_vecPhrases["UnsilenceMessageAll"];
			break;
		}
		g_pUtils->PrintToChatAll(szMessage.c_str(), iAdminID == -1?"Console":engine->GetClientConVarValue(iAdminID, "name"), engine->GetClientConVarValue(iSlot, "name"));
	}
	RemovePunishment(iSlot, iType, iAdminID);
}

void AdminApi::RemoveOfflinePlayerPunishment(const char* szSteamID64, int iType, int iAdminID)
{
	RemoveOfflinePunishment(szSteamID64, iType, iAdminID);
}

const char* AdminApi::GetFlagName(const char* szFlag)
{
	if(g_mFlags.find(szFlag) != g_mFlags.end())
	{
		return g_mFlags[szFlag].szName;
	}
	return "";
}

int AdminApi::GetAdminExpireTime(int iSlot)
{
	return g_pAdmins[iSlot].iExpireTime; 
}

void AdminApi::ShowAdminMenu(int iSlot)
{
	OnAdminMenu(iSlot, nullptr);
}

void AdminApi::ShowAdminCategoryMenu(int iSlot, const char* szCategory)
{
	ShowCategoryMenu(iSlot, szCategory);
}

void AdminApi::ShowAdminLastCategoryMenu(int iSlot)
{
	ShowLastCategoryMenu(iSlot);
}

void AdminApi::ShowAdminItemMenu(int iSlot, const char* szCategory, const char* szIdentity)
{
	ShowItemMenu(iSlot, szCategory, szIdentity);
}

void AdminApi::AddPlayerAdmin(const char* szName, const char* szSteamID64, const char* szFlags, int iImmunity, int iExpireTime, int iGroupID, const char* szComment, bool bDB)
{
	AddAdmin(szName, szSteamID64, szFlags, iImmunity, iExpireTime, iGroupID, szComment, bDB);
}

void AdminApi::RemovePlayerAdmin(const char* szSteamID64, bool bDB)
{
	RemoveAdmin(szSteamID64, bDB);
}

void AdminApi::AddPlayerLocalFlag(int iSlot, const char* szFlag)
{
	g_pAdmins[iSlot].vFlags.push_back(szFlag);
}

void AdminApi::RemovePlayerLocalFlag(int iSlot, const char* szFlag)
{
	g_pAdmins[iSlot].vFlags.erase(std::remove(g_pAdmins[iSlot].vFlags.begin(), g_pAdmins[iSlot].vFlags.end(), szFlag), g_pAdmins[iSlot].vFlags.end());
}

void AdminApi::AddPlayerLocalPermission(int iSlot, const char* szPermission)
{
	g_pAdmins[iSlot].vPermissions.push_back(szPermission);
}

void AdminApi::RemovePlayerLocalPermission(int iSlot, const char* szPermission)
{
	g_pAdmins[iSlot].vPermissions.erase(std::remove(g_pAdmins[iSlot].vPermissions.begin(), g_pAdmins[iSlot].vPermissions.end(), szPermission), g_pAdmins[iSlot].vPermissions.end());
}

void AdminApi::AddPlayerLocalImmunity(int iSlot, int iImmunity)
{
	g_pAdmins[iSlot].iImmunity = iImmunity;
}

void AdminApi::RemovePlayerLocalImmunity(int iSlot)
{
	g_pAdmins[iSlot].iImmunity = 0;
}

IMySQLConnection* AdminApi::GetMySQLConnection()
{
	return g_pConnection;
}

float AdminApi::GetPluginVersion()
{
	return std::atof(g_PLAPI->GetVersion());
}

const char* AdminApi::GetTranslation(const char* szKey)
{
	if(g_vecPhrases.find(szKey) != g_vecPhrases.end())
	{
		return g_vecPhrases[szKey].c_str();
	}
	return szKey;
}

int AdminApi::GetMessageType()
{
	return g_iMessageType;
}

///////////////////////////////////////
const char* admin_system::GetLicense()
{
	return "GPL";
}

const char* admin_system::GetVersion()
{
	return "1.0.6";
}

const char* admin_system::GetDate()
{
	return __DATE__;
}

const char *admin_system::GetLogTag()
{
	return "[AS]";
}

const char* admin_system::GetAuthor()
{
	return "Pisex";
}

const char* admin_system::GetDescription()
{
	return "Admin System by PISEX";
}

const char* admin_system::GetName()
{
	return "[AS] Core";
}

const char* admin_system::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
