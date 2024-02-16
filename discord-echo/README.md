# Discord Echo plugin
### Required:
- Installed [Concord](https://github.com/Cogmasters/concord)

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

### Build note:
- Maybe you need to add `-ldiscord -lcurl` to `src/plugins/Makefile`. Add that after `-lpcre`
