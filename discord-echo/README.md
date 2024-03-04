# Discord Echo plugin
## About:
![](https://i.imgur.com/fLamjTb.mp4)

- Sync chat messages between Discord and In-game channel chat
- Change Discord name  by In-game name.
- Can be change Discord avatar if you have ROchargen

### Required:
- Installed [Concord](https://github.com/Cogmasters/concord)
- curl
- pthread

### File struct:
```
.
├── conf
│   └── import
│       └── discord.conf
└── src
    └── plugins
        └── hercules-echo.py
```
Note: `hercules-echo.py` dont need anymore

### Chat flow
1. Start server
- Map server start
- Load discord-echo plugin
- Connect Discord bot. 
    - Using `pthread` to run async Discord bot
    - This bot listen message from Discord channel
2. From Discord chat
- Discord user send chat message
- Discord Bot catch that message
- Send to in-game channel as Discord username. Add (`<>` to username): Example `<vietlubu>`
3. From In-game channel chat
- Plugin trigger after send message
- Execute webhook with custom name

### Build/run note:
- Maybe you need to add `-ldiscord -lcurl` to `src/plugins/Makefile`. Add that after `-lpcre`
- Create Discord bot, get token and put to `conf/import/discord.conf`
- Create Discord channel webhook and put webhook_id, webhook token to `conf/import/discord.conf`
- Add `discord-echo` plugin to `conf/plugins.conf`
