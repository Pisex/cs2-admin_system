#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include "igameevents.h"
#include "vector.h"
#include <deque>
#include <functional>
#include <utlstring.h>
#include <keyvalues.h>
#include "CCSPlayerController.h"
#include "include/menus.h"
#include "include/mysql_mm.h"
#include "include/admin.h"
#include "menus.h"

struct Flag
{
	const char* szName;
	std::vector<std::string> vPermissions;
};

struct Admin
{
    int iID;
    int iImmunity;
    int iExpireTime;
    std::vector<std::string> vFlags;
    std::vector<std::string> vPermissions;
	std::string szName;
	int iGroup;
	std::string szGroupName;
};

extern std::unordered_map<std::string, Category> mCategories;

class AdminApi : public IAdminApi
{
public:
	std::vector<std::string> ParseString(std::string szString, const char* szDelimiter) {
		std::vector<std::string> vecResult;
		size_t pos = 0;
		std::string token;
		while ((pos = szString.find(szDelimiter)) != std::string::npos) {
			token = szString.substr(0, pos);
			vecResult.push_back(token);
			szString.erase(0, pos + strlen(szDelimiter));
		}
		vecResult.push_back(szString);
		return vecResult;
	}
	void RegisterCategory(const char* szIdentity, const char* szName, OnCategoryDisplayCallback callback) override {
		for (auto& it : mOnCategoryRegister) it.second(szIdentity);
		mCategories[szIdentity].szIdentity = szIdentity;
		mCategories[szIdentity].szName = szName;
		mCategories[szIdentity].hCallback = callback;
	}
	void RegisterItem(const char* szIdentity, const char* szName, const char* szCategory, const char* szFlags, OnItemDisplayCallback callbackDisplay, OnItemSelectCallback callbackSelect) override {
		Item hItem;
		hItem.szIdentity = szIdentity;
		hItem.szName = szName;
		hItem.szCategory = szCategory;
		hItem.vecFlags = ParseString(szFlags, "|");
		hItem.hCallbackDisplay = callbackDisplay;
		hItem.hCallbackSelect = callbackSelect;
		mCategories[szCategory].mItems[szIdentity] = hItem;
	}
	bool HasPermission(int iSlot, const char* szPermission);
	bool HasFlag(int iSlot, const char* szFlag);
	bool IsAdmin(int iSlot);
	void OnAdminConnected(SourceMM::PluginId id, OnAdminConnectedCallback callback) override {
		mOnAdminConnected[id] = callback;
	}
	void OnAdminConnectedSend(int iSlot) {
		for (auto& it : mOnAdminConnected) {
			it.second(iSlot);
		}
	}
	std::vector<std::string> GetPermissionsByFlag(const char* szFlag);
	std::vector<std::string> GetAdminFlags(int iSlot);
	std::vector<std::string> GetAdminPermissions(int iSlot);
	int GetAdminImmunity(int iSlot);
	bool IsPlayerPunished(int iSlot, int iType);
	int GetPlayerPunishmentExpired(int iSlot, int iType);
	const char* GetPlayerPunishmentReason(int iSlot, int iType);
	void AddPlayerPunishment(int iSlot, int iType, int iTime, const char* szReason, int iAdminID, bool bNotify, bool bDB);
	void OnPlayerPunish(SourceMM::PluginId id, OnPlayerPunishCallback callback) override {
		mOnPlayerPunish[id] = callback;
	}
	void OnPlayerPunishPre(SourceMM::PluginId id, OnPlayerPunishCallbackPre callback) {
		mOnPlayerPunishPre[id] = callback;
	}
	void OnPlayerPunishSend(int iSlot, int iType, int iTime, const char* szReason, int iAdminID) {
		for (auto& it : mOnPlayerPunish) {
			it.second(iSlot, iType, iTime, szReason, iAdminID);
		}
	}
	bool OnPlayerPunishPreSend(int iSlot, int iType, int iTime, const char* szReason, int iAdminID) {
		for (auto& it : mOnPlayerPunishPre) {
			if (it.second(iSlot, iType, iTime, szReason, iAdminID)) return true;
		}
		return false;
	}
	void AddOfflinePlayerPunishment(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID);
	void OnOfflinePlayerPunish(SourceMM::PluginId id, OnOfflinePlayerPunishCallback callback) override {
		mOnOfflinePlayerPunish[id] = callback;
	}
	void OnOfflinePlayerPunishPre(SourceMM::PluginId id, OnOfflinePlayerPunishCallbackPre callback) {
		mOnOfflinePlayerPunishPre[id] = callback;
	}
	void OnOfflinePlayerPunishSend(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID) {
		for (auto& it : mOnOfflinePlayerPunish) {
			it.second(szSteamID64, szName, iType, iTime, szReason, iAdminID);
		}
	}
	bool OnOfflinePlayerPunishPreSend(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID) {
		for (auto& it : mOnOfflinePlayerPunishPre) {
			if (it.second(szSteamID64, szName, iType, iTime, szReason, iAdminID)) return true;
		}
		return false;
	}
	void RemovePlayerPunishment(int iSlot, int iType, int iAdminID, bool bNotify);
	void RemoveOfflinePlayerPunishment(const char* szSteamID64, int iType, int iAdminID);
	void OnPlayerUnpunish(SourceMM::PluginId id, OnPlayerUnpunishCallback callback) {
		mOnPlayerUnpunish[id] = callback;
	}
	void OnPlayerUnpunishSend(int iSlot, int iType, int iAdminID) {
		for (auto& it : mOnPlayerUnpunish) {
			it.second(iSlot, iType, iAdminID);
		}
	}
	void OnOfflinePlayerUnpunish(SourceMM::PluginId id, OnOfflinePlayerUnpunishCallback callback) {
		mOnOfflinePlayerUnpunish[id] = callback;
	}
	void OnOfflinePlayerUnpunishSend(const char* szSteamID64, int iType, int iAdminID) {
		for (auto& it : mOnOfflinePlayerUnpunish) {
			it.second(szSteamID64, iType, iAdminID);
		}
	}
	const char* GetAdminName(int iSlot);
    int GetAdminGroupID(int iSlot);
    const char* GetAdminGroupName(int iSlot);
    int GetImmunityType();
	const char* GetFlagName(const char* szFlag);
	int GetAdminExpireTime(int iSlot);
	void ShowAdminMenu(int iSlot);
	void ShowAdminCategoryMenu(int iSlot, const char* szCategory);
	void ShowAdminLastCategoryMenu(int iSlot);
	void ShowAdminItemMenu(int iSlot, const char* szCategory, const char* szIdentity);
	void AddPlayerAdmin(const char* szName, const char* szSteamID64, const char* szFlags, int iImmunity, int iExpireTime, int iGroupID, const char* szComment, bool bDB);
	void RemovePlayerAdmin(const char* szSteamID64, bool bDB);
	void AddPlayerLocalFlag(int iSlot, const char* szFlag);
	void RemovePlayerLocalFlag(int iSlot, const char* szFlag);
	void AddPlayerLocalPermission(int iSlot, const char* szPermission);
	void RemovePlayerLocalPermission(int iSlot, const char* szPermission);
	void AddPlayerLocalImmunity(int iSlot, int iImmunity);
	void RemovePlayerLocalImmunity(int iSlot);
	IMySQLConnection* GetMySQLConnection();
	float GetPluginVersion();
    const char* GetTranslation(const char* szKey);
	void OnCoreLoaded(SourceMM::PluginId id, OnCoreLoadedCallback callback)
	{
		mOnCoreLoaded[id] = callback;
	}
	bool IsCoreLoaded()
	{
		return m_bCoreLoaded;
	}
	void OnCoreLoadedSend()
	{
		m_bCoreLoaded = true;
		for (auto& it : mOnCoreLoaded)
		{
			it.second();
		}
	}
	void OnCategoryRegister(SourceMM::PluginId id, OnCategoryRegisterCallback callback)
	{
		mOnCategoryRegister[id] = callback;
	}
	void SendAction(int iSlot, const char* szAction, const char* szParam)
	{
		for (auto& it : mOnAction)
		{
			it.second(iSlot, szAction, szParam);
		}
	}
	void OnAction(SourceMM::PluginId id, OnActionCallback callback)
	{
		mOnAction[id] = callback;
	}
	int GetMessageType();
private:
	std::map<int, OnAdminConnectedCallback> mOnAdminConnected;
	std::map<int, OnPlayerPunishCallback> mOnPlayerPunish;
	std::map<int, OnOfflinePlayerPunishCallback> mOnOfflinePlayerPunish;
	std::map<int, OnPlayerPunishCallbackPre> mOnPlayerPunishPre;
	std::map<int, OnOfflinePlayerPunishCallbackPre> mOnOfflinePlayerPunishPre;
	std::map<int, OnPlayerUnpunishCallback> mOnPlayerUnpunish;
	std::map<int, OnOfflinePlayerUnpunishCallback> mOnOfflinePlayerUnpunish;
	std::map<int, OnCoreLoadedCallback> mOnCoreLoaded;
	std::map<int, OnCategoryRegisterCallback> mOnCategoryRegister;
	std::map<int, OnActionCallback> mOnAction;

	bool m_bCoreLoaded = false;
};

class admin_system final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void* OnMetamodQuery(const char* iface, int* ret);
private:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();
private:
	bool OnClientConnect(CPlayerSlot slot, const char *pszName, uint64 xuid, const char *pszNetworkID, bool unk1, CBufferString *pRejectReason);
	void OnClientDisconnect(CPlayerSlot slot, ENetworkDisconnectionReason reason, const char *pszName, uint64 xuid, const char *pszNetworkID);
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
