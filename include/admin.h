#pragma once

#include <functional>
#include <string>
#include <map>
#include <vector>

class IMySQLConnection;

// Punishment types
#define RT_BAN 0
#define RT_MUTE 1
#define RT_GAG 2
#define RT_SILENCE 3

typedef std::function<void(int iSlot, const char* szIdentity, std::string& szCategory)> OnCategoryDisplayCallback;
typedef std::function<void(int iSlot, const char* szCategory, const char* szIdentity, std::string& szItem)> OnItemDisplayCallback;
typedef std::function<void(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem)> OnItemSelectCallback;
typedef std::function<void(int iSlot)> OnAdminConnectedCallback;
typedef std::function<void(int iSlot, int iType, int iTime, const char* szReason, int iAdminID)> OnPlayerPunishCallback;
typedef std::function<void(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID)> OnOfflinePlayerPunishCallback;
typedef std::function<void(int iSlot, int iType, int iAdminID)> OnPlayerUnpunishCallback;
typedef std::function<void(const char* szSteamID64, int iType, int iAdminID)> OnOfflinePlayerUnpunishCallback;
typedef std::function<void()> OnCoreLoadedCallback;
typedef std::function<void(const char* szCategory)> OnCategoryRegisterCallback;
typedef std::function<void(int iSlot, const char* szAction, const char* szParam)> OnActionCallback;

#define Admin_INTERFACE "IAdminApi"

class IAdminApi
{
public:
    virtual float GetPluginVersion() = 0;
    virtual const char* GetTranslation(const char* szKey) = 0;
    virtual IMySQLConnection* GetMySQLConnection() = 0;
    virtual void RegisterCategory(const char* szIdentity, const char* szName, OnCategoryDisplayCallback callback) = 0;
    virtual void RegisterItem(const char* szIdentity, const char* szName, const char* szCategory, const char* szFlag, OnItemDisplayCallback callbackDisplay, OnItemSelectCallback callbackSelect) = 0;
    virtual bool HasPermission(int iSlot, const char* szPermission) = 0;
    virtual bool HasFlag(int iSlot, const char* szFlag) = 0;
    virtual bool IsAdmin(int iSlot) = 0;
    virtual void OnAdminConnected(SourceMM::PluginId id, OnAdminConnectedCallback callback) = 0;
    virtual const char* GetFlagName(const char* szFlag) = 0;
    virtual std::vector<std::string> GetPermissionsByFlag(const char* szFlag) = 0;
    virtual std::vector<std::string> GetAdminFlags(int iSlot) = 0;
    virtual std::vector<std::string> GetAdminPermissions(int iSlot) = 0;
    virtual int GetAdminImmunity(int iSlot) = 0;
    virtual int GetAdminExpireTime(int iSlot) = 0;
    virtual bool IsPlayerPunished(int iSlot, int iType) = 0;
    virtual int GetPlayerPunishmentExpired(int iSlot, int iType) = 0;
    virtual const char* GetPlayerPunishmentReason(int iSlot, int iType) = 0;
    virtual void AddPlayerPunishment(int iSlot, int iType, int iTime, const char* szReason, int iAdminID = -1, bool bNotify = true, bool bDB = true) = 0;
    virtual void OnPlayerPunish(SourceMM::PluginId id, OnPlayerPunishCallback callback) = 0;
    virtual void OnOfflinePlayerPunish(SourceMM::PluginId id, OnOfflinePlayerPunishCallback callback) = 0;
    virtual void AddOfflinePlayerPunishment(const char* szSteamID64, const char* szName, int iType, int iTime, const char* szReason, int iAdminID = -1) = 0;
    virtual void RemovePlayerPunishment(int iSlot, int iType, int iAdminID = -1, bool bNotify = true) = 0;
    virtual void RemoveOfflinePlayerPunishment(const char* szSteamID64, int iType, int iAdminID = -1) = 0;
    virtual void OnPlayerUnpunish(SourceMM::PluginId id, OnPlayerUnpunishCallback callback) = 0;
    virtual void OnOfflinePlayerUnpunish(SourceMM::PluginId id, OnOfflinePlayerUnpunishCallback callback) = 0;
    virtual void ShowAdminMenu(int iSlot) = 0;
    virtual void ShowAdminCategoryMenu(int iSlot, const char* szCategory) = 0;
    virtual void ShowAdminLastCategoryMenu(int iSlot) = 0;
    virtual void ShowAdminItemMenu(int iSlot, const char* szCategory, const char* szIdentity) = 0;
    virtual void AddPlayerAdmin(const char* szName, const char* szSteamID64, const char* szFlags, int iImmunity = 0, int iExpireTime = 0, int iGroupID = 0, const char* szComment = "", bool bDB = true) = 0;
    virtual void RemovePlayerAdmin(const char* szSteamID64, bool bDB = true) = 0;
    virtual void AddPlayerLocalFlag(int iSlot, const char* szFlag) = 0;
    virtual void RemovePlayerLocalFlag(int iSlot, const char* szFlag) = 0;
    virtual void AddPlayerLocalPermission(int iSlot, const char* szPermission) = 0;
    virtual void RemovePlayerLocalPermission(int iSlot, const char* szPermission) = 0;
    virtual void AddPlayerLocalImmunity(int iSlot, int iImmunity) = 0;
    virtual void RemovePlayerLocalImmunity(int iSlot) = 0;
    virtual void OnCoreLoaded(SourceMM::PluginId id, OnCoreLoadedCallback callback) = 0;
    virtual bool IsCoreLoaded() = 0;
    virtual void OnCategoryRegister(SourceMM::PluginId id, OnCategoryRegisterCallback callback) = 0;
    virtual void SendAction(int iSlot, const char* szAction, const char* szParam) = 0;
    virtual void OnAction(SourceMM::PluginId id, OnActionCallback callback) = 0;
    virtual int GetMessageType() = 0;
};