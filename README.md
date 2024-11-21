# Install
1. Download and install last Release
2. Download and install [sql_mm](https://github.com/zer0k-z/sql_mm/releases)
3. Customize the configuration file(addons/configs/admin_system/core.ini)
4. Write the data from your database(addons/configs/databasese.cfg)<br>
4.1 If you have such a file, you should simply add the admin_system key to it<br>
4.2 If you do not have such a file, create one and specify the following data in it
```
"Databases"
{
    "admin_system"
    {
        "host"      ""
        "user"      ""
        "pass"      ""
        "database"  ""
        "port"      "3306"
    }
}
```

# Commands
- `mm_admin/css_admin/!admin` - Main menu

- `mm_ban/!ban <name/steamid/userid> <time> <reason>` - Ban command (@admin/ban)
- `mm_mute/!mute <name/steamid/userid> <time> <reason>` - Mute command (@admin/mute)
- `mm_gag/!gag <name/steamid/userid> <time> <reason>` - Gag command (@admin/gag)
- `mm_silence/!silence <name/steamid/userid> <time> <reason>` - Silence command (@admin/silence)

- `mm_unban/!unban <name/steamid/userid>` - UnBan command (@admin/unban)
- `mm_unmute/!unmute <name/steamid/userid>` - UnMute command (@admin/unmute)
- `mm_ungag/!ungag <name/steamid/userid>` - UnGag command (@admin/ungag)
- `mm_unsilence/!unsilence <name/steamid/userid>` - UnSilence command (@admin/unsilence)

- `mm_add_admin/!add_admin <name/steamid/userid> <name> <flags> <immunity> <time> <?group> <?comment>` - Add new Admin (@admin/add_admin)
- `mm_remove_admin/!remove_admin <name/steamid/userid>` - Remove Admin (@admin/remove_admin)

- `mm_add_group/!add_group <name> <flags> <immunity>` - Add new group (@admin/add_group)
- `mm_remove_group/!remove_group <id/name>` - Remove group (@admin/remove_group)

- `mm_as_reload_config` - Reload config (@admin/reload_config)
- `mm_as_reload_admin <steamid64>` - Reload admin (@admin/reload_admin)
- `mm_as_reload_punish <steamid64>` - Reload punishments (@admin/reload_punish)

# Info
- If you want to grant all rights, it will be enough to specify the @admin/root flag to the player
- Sorting category: punishments
- Sorting items: punish, unpunish, punish_offline, unpunish_offline