#ifndef DATABASE_H
#define DATABASE_H

#include <vector>
#include <string>

typedef unsigned long long uint64;

#define SID_ADMIN 0
#define SID_PUNISH 1

void CreateConnection();
void CheckPunishments(int iSlot, uint64 xuid);
void CheckPermissions(int iSlot, uint64 xuid);
void AddPunishment(int iSlot, int iType, int iTime, std::string szReason, int iAdminID, bool bDB);
void AddOfflinePunishment(const char* szSteamID64, const char* szName, int iType, int iTime, std::string szReason, int iAdminID);
void RemovePunishment(int iSlot, int iType, int iAdminID);
void RemoveOfflinePunishment(const char* szSteamID64, int iType, int iAdminID);
void AddAdmin(const char* szName, const char* szSteamID64, const char* szFlags, int iImmunity, int iTime, int iGroupID, const char* szComment, bool bDB);
void RemoveAdmin(const char* szSteamID64, bool bDB);
#endif