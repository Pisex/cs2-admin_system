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

- `mm_ban/!ban <steamid/userid> <time> <reason>` - Ban command (@admin/ban)
- `mm_mute/!mute <steamid/userid> <time> <reason>` - Mute command (@admin/mute)
- `mm_gag/!gag <steamid/userid> <time> <reason>` - Gag command (@admin/gag)
- `mm_silence/!silence <steamid/userid> <time> <reason>` - Silence command (@admin/silence)

- `mm_unban/!unban <steamid/userid>` - UnBan command (@admin/unban)
- `mm_unmute/!unmute <steamid/userid>` - UnMute command (@admin/unmute)
- `mm_ungag/!ungag <steamid/userid>` - UnGag command (@admin/ungag)
- `mm_unsilence/!unsilence <steamid/userid>` - UnSilence command (@admin/unsilence)

- `mm_add_admin/!add_admin <steamid/userid> <name> <flags> <immunity> <time> <?group> <?comment>` - Add new Admin (@admin/add)
- `mm_remove_admin/!remove_admin <steamid/userid>` - Remove Admin (@admin/remove)

# Info
- If you want to grant all rights, it will be enough to specify the @admin/root flag to the player
