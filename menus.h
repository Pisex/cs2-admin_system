#ifndef MENUS_H
#define MENUS_H

#include <ISmmPlugin.h>
#include "include/menus.h"
#include "include/admin.h"

struct Item
{
	const char* szIdentity;
	const char* szName;
	const char* szCategory;
	std::vector<std::string> vecFlags;
	OnItemDisplayCallback hCallbackDisplay;
	OnItemSelectCallback hCallbackSelect;
};

struct Category
{
	const char* szIdentity;
	const char* szName;
	std::unordered_map<std::string, Item> mItems;
	OnCategoryDisplayCallback hCallback;
};

struct OfflineUser
{
	uint64 iSteamID64;
	std::string szName;
	bool bPunished[4];
	uint64 iAdminID[4];
};

void OnUnPunishSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem);
void OnPunishSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem);
void OnPunishOfflineSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem);
void OnUnPunishOfflineSelect(int iSlot, const char* szCategory, const char* szIdentity, const char* szItem);
bool OnAdminMenu(int iSlot, const char* szContent);
void ShowItemMenu(int iSlot, const char* szCategory, const char* szIdentity);
void ShowCategoryMenu(int iSlot, const char* szCategory);
void ShowLastCategoryMenu(int iSlot);
void ShowPunishMenu(int iSlot, int iType);
void ShowUnPunishMenu(int iSlot, int iType);

#endif