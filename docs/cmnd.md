# SARA Command Documentation

## Telegram Gateway Commands
These commands are available as slash commands (e.g., `/dock`) via the Telegram Gateway.

- **/dock**: Open Media Dock
- **/system**: Open System Dock
- **/status**: Runtime status
- **/help**: Show all commands
- **/media**: Open Media controls
- **/desktop**: Start Remote Desktop
- **/desktopshutdown**: Stop Remote Desktop
- **/terminal**: Start browser terminal
- **/terminal_admin**: Start admin browser terminal
- **/sararestart**: Restart SARA and Cloudflare completely
- **/sarashutdown**: Kill SARA completely (no restart)
- **/screenshot**: Take screenshot
- **/photo**: Webcam photo
- **/monitor**: Live system stats
- **/tasks**: Scheduled tasks
- **/rules**: Event automation rules
- **/files**: Manage files via File Browser
- **/filesshutdown**: Shutdown File Browser completely
- **/workspace**: Open SARA IDE workspace
- **/workspaceshutdown**: Shutdown SARA IDE workspace
- **/sp_help**: Spotify commands list


## Command Parsing & Execution (CommandMap)
SARA uses a `CommandMap` to route natural language text into specific actions. Commands are matched in the following order of precedence:
1. **Exact (`exact`)**: The user input must precisely match the defined phrase (case-insensitive, ignoring leading/trailing spaces and trailing "please").
2. **Prefix (`prefix`)**: The user input must start with the defined phrase. The remainder of the text is captured and passed as a parameter.
3. **Regex (`regex`)**: The user input is evaluated against a regular expression. Capture groups can be used to extract parameters.
4. **Contains (`contains`)**: The defined phrase simply needs to be present anywhere within the user input.

## Natural Language Commands by Category

### Apps
*Note: SARA supports opening hundreds of popular applications via exact phrase matching. Here are a few examples:*

- **Phrases**: "open spotify", "launch spotify", "start spotify", "spotify open", "run spotify", "open spotify app", "launch spotify app", "spotify launch"
  - **Type**: exact
  - **Description**: Triggers action: `open_app`
- **Phrases**: "open chrome", "launch chrome", "start chrome", "open google chrome", "chrome browser", "run chrome", "launch google chrome", "google chrome open", "start google chrome", "open chromium", "launch chromium"
  - **Type**: exact
  - **Description**: Triggers action: `open_app`
- **Phrases**: "open calculator", "launch calc", "start calculator", "open calc", "run calculator", "launch calculator", "open windows calculator", "start calc", "open calc app", "calculator app"
  - **Type**: exact
  - **Description**: Triggers action: `open_app`
- **Phrases**: "open cmd", "open command prompt", "launch terminal", "command prompt", "start cmd", "launch cmd", "open dos", "run cmd", "open command line", "launch command prompt", "start command prompt", "cmd open", "dos prompt", "open dos prompt"
  - **Type**: exact
  - **Description**: Triggers action: `open_app`

*(... and many more, including IDEs, browsers, media players, security tools, and system utilities).*


### Browser
- **Phrases**: "search google", "google search", "search the web", "look up", "search internet", "google that", "search for", "do a google search", "search on google", "web search", "google something", "look it up online", "search online"
  - **Type**: exact
  - **Description**: Triggers action: `search_plugin`
- **Phrases**: "search google for ", "search for ", "google ", "search the web for ", "google search ", "find on google ", "search ", "search online for ", "search web for ", "search about ", "search on ", "search regarding ", "lookup ", "look for ", "quick search for ", "sara search "
  - **Type**: prefix
  - **Description**: Triggers action: `search_plugin`
- **Phrases**: "deep search", "scrape search", "search and read"
  - **Type**: exact
  - **Description**: Triggers action: `search_plugin_scrape`
- **Phrases**: "deep search ", "search and read ", "scrape search ", "deep search for ", "search and summarize ", "search and read about ", "scrape and search ", "detailed search for ", "detailed info on ", "read and search ", "fetch and read ", "full search on ", "comprehensive search for ", "research on ", "research about ", "do research on ", "in depth search for ", "in depth info on ", "search with content for ", "get full details on ", "get complete info on "
  - **Type**: prefix
  - **Description**: Triggers action: `search_plugin_scrape`
- **Phrases**: "open youtube", "launch youtube", "youtube", "open youtube website", "go to youtube", "youtube com"
  - **Type**: exact
  - **Description**: Triggers action: `open_url`
- **Phrases**: "open github", "github", "open github website", "github com"
  - **Type**: exact
  - **Description**: Triggers action: `open_url`
- **Phrases**: "open stackoverflow", "stackoverflow", "stack overflow", "open stack overflow"
  - **Type**: exact
  - **Description**: Triggers action: `open_url`
- **Phrases**: "open reddit", "reddit", "open reddit website", "reddit com"
  - **Type**: exact
  - **Description**: Triggers action: `open_url`
- **Phrases**: "open gmail", "gmail", "gmail com", "open gmail website", "my email"
  - **Type**: exact
  - **Description**: Triggers action: `open_url`
- **Phrases**: "open amazon", "amazon", "amazon com", "open amazon website", "amazon shopping"
  - **Type**: exact
  - **Description**: Triggers action: `open_url`
- **Phrases**: "open google drive", "google drive", "drive google", "my drive"
  - **Type**: exact
  - **Description**: Triggers action: `open_url`
- **Phrases**: ".*(?:open|launch|go to|take me to|visit|browse to) (?:website |site |the website |url )?(.*)"
  - **Type**: regex
  - **Description**: Triggers action: `open_url`


### Files
- **Phrases**: "create folder", "create directory", "new folder", "make directory", "make a new folder", "create a folder", "new directory", "create new folder"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "clear temp files", "delete temp files", "clean temp", "temporary files", "delete temporary files", "clear temp folder", "clean temporary files", "remove temp files", "clear temporary data", "clean up temp"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "empty recycle bin", "clear recycle bin", "recycle bin", "empty bin", "clear bin", "empty trash", "clear trash", "empty the recycle bin"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "clean disk", "disk cleanup", "run disk cleanup", "cleanup disk", "free up disk space", "clean up disk"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "list files", "show files", "list directory", "dir listing", "show all files", "what files are here", "list current directory"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "open downloads folder", "open downloads", "go to downloads", "downloads directory"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "open documents folder", "open documents", "go to documents", "my documents"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "open desktop folder", "go to desktop", "open desktop", "desktop directory"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "take ownership", "takeown file", "take file ownership"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "check file integrity", "check disk", "scan disk", "run chkdsk", "check disk for errors"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "defrag drive", "defragment", "run defrag", "optimize drive", "defrag disk"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "compress folder", "zip this", "compress files", "create zip"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "extract zip", "unzip", "extract archive", "decompress files", "unzip files"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`


### Focus
- **Phrases**: "focus window ", "switch to ", "focus ", "bring to front ", "switch window ", "activate window ", "open that window ", "go to window ", "minimize others and focus "
  - **Type**: prefix
  - **Description**: Triggers action: `focus_app`
- **Phrases**: "close window ", "close app ", "close ", "kill window ", "terminate "
  - **Type**: prefix
  - **Description**: Triggers action: `close_app`
- **Phrases**: "switch window", "alt tab", "switch app", "cycle windows", "next window"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "close current window", "close active window", "close this window"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "show desktop", "minimize all", "hide all windows", "show my desktop"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "task view", "show task view", "show all windows", "expose"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "open action center", "notification panel", "show notifications"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "open search", "windows search", "search bar", "start search"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "open run dialog", "run command", "open run", "windows r"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "lock screen", "lock windows", "lock workstation shortcut"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "open emoji panel", "emoji picker", "show emojis", "emoji keyboard"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "open clipboard history", "clipboard manager", "show clipboard history"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "open snipping tool shortcut", "screen snip", "open snip tool"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "minimize window", "minimize", "minimize current window"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "maximize window", "maximize", "maximize current window"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "snap left", "snap window left", "window to left side"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "snap right", "snap window right", "window to right side"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "new virtual desktop", "add desktop", "create new desktop"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "switch virtual desktop", "switch desktop", "next desktop"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`
- **Phrases**: "close virtual desktop", "remove desktop", "delete desktop"
  - **Type**: exact
  - **Description**: Triggers action: `keyboard_shortcut`


### General
- **Phrases**: "pause", "resume", "play pause", "toggle play", "play/pause", "play or pause", "toggle music", "toggle playback", "unpause", "continue playing"
  - **Type**: exact
  - **Description**: Triggers action: `media_play_pause`
- **Phrases**: "next", "skip", "next track", "next song", "skip track", "skip song", "forward"
  - **Type**: exact
  - **Description**: Triggers action: `media_next`
- **Phrases**: "previous", "back", "previous track", "previous song", "go back", "backward"
  - **Type**: exact
  - **Description**: Triggers action: `media_prev`
- **Phrases**: "stop", "stop music", "stop playing", "stop the music", "stop playback"
  - **Type**: exact
  - **Description**: Triggers action: `media_stop`
- **Phrases**: "mute", "mute volume", "mute audio", "silence", "mute sound", "turn off sound", "silent mode", "no sound", "sound off", "disable audio"
  - **Type**: exact
  - **Description**: Triggers action: `volume_mute`
- **Phrases**: "unmute", "unmute volume", "unmute audio", "turn on sound", "enable audio"
  - **Type**: exact
  - **Description**: Triggers action: `volume_mute`


### Media
- **Phrases**: "play ", "please play ", "start playing ", "play song ", "play music ", "could you play ", "can you play ", "i want to hear ", "i want to listen to ", "play the song ", "play me ", "play some ", "play the "
  - **Type**: prefix
  - **Description**: Triggers action: `play_music`
- **Phrases**: ".*(pause|stop|hold on).*(song|music|playing|playback|track|it)?"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(next|skip|forward).*(song|track|one|music)?"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(previous|go back|last|back).*(song|track|one|music)?"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(play|resume|go back to).*(last|recent|previous).*(playlist).*"
  - **Type**: regex
  - **Description**: Triggers action: `play_last_playlist`
- **Phrases**: ".*(resume|continue|unpause|play again|start again).*(playing|music|song|playback|spotify|track)?"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(toggle).*(play|pause|music|song|playback).*", ".*(play/pause|play or pause).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(shuffle|random|randomize).*(on|all|songs|music|playlist)?.*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(shuffle|random).*(off|disable|stop|no).*", ".*(play in order).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(repeat|loop|continuous).*(on|all|playlist).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(repeat|loop).*(one|current|this|single|track|song).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(repeat|loop).*(off|disable|stop|no|cancel).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(like|love|heart|favorite|fav|save).*(this|song|track|music|library).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(unlike|unheart|remove|dislike).*(heart|favorite|liked|song|track|music).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(what|which|tell me).*(song|track|playing|music).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(add to|queue).*(queue|playlist|next).*"
  - **Type**: regex
  - **Description**: Triggers action: `media_command`
- **Phrases**: ".*(volume up|increase volume|turn up|louder|raise volume|boost volume).*"
  - **Type**: regex
  - **Description**: Triggers action: `volume_set`
- **Phrases**: ".*(volume down|decrease volume|turn down|quieter|lower volume|reduce volume).*"
  - **Type**: regex
  - **Description**: Triggers action: `volume_set`
- **Phrases**: ".*(max|full|maximum|100).*(volume|blast).*"
  - **Type**: regex
  - **Description**: Triggers action: `volume_set`
- **Phrases**: ".*(mute|silence|turn off sound|silent|no sound|disable audio).*"
  - **Type**: regex
  - **Description**: Triggers action: `volume_mute`
- **Phrases**: ".*(unmute|turn on sound|enable audio|sound on).*"
  - **Type**: regex
  - **Description**: Triggers action: `volume_mute`
- **Phrases**: ".*(set|change|adjust).*(volume|vol).*(to)? (\d+).*", ".*(volume|vol).*(to)? (\d+).*"
  - **Type**: regex
  - **Description**: Triggers action: `set_volume_level`
- **Phrases**: ".*(what|check).*(volume).*"
  - **Type**: regex
  - **Description**: Triggers action: `volume_get`


### Network
- **Phrases**: "my ip", "ip address", "show my ip", "network ip", "local ip", "what is my ip", "my public ip", "public ip", "external ip", "ipv4 address", "ipv6 address", "tell me my ip", "my ip address", "show ip"
  - **Type**: contains
  - **Description**: Triggers action: `get_ip_address`
- **Phrases**: "network status", "wifi status", "network info", "connection status", "internet status", "network connection", "am i connected", "internet connection", "wifi signal", "network strength", "connection info", "network diagnostics"
  - **Type**: contains
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "disk usage", "disk space", "storage", "how much space", "drive status", "free space", "disk free", "storage space", "remaining storage", "disk capacity", "hard drive space", "ssd space", "how much storage left", "disk utilization"
  - **Type**: contains
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "system uptime", "how long has pc been on", "uptime", "pc uptime", "how long has my pc been on", "system running time", "last boot time", "how long since restart", "uptime status", "running since"
  - **Type**: contains
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(test|check).*(internet|connection|speed).*", ".*(ping).*(google).*", ".*(is internet working).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(what is my|show my|tell me my)?.*(hostname|computer name|device name).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(what is my|show my|tell me my)?.*(mac address|physical address|network mac).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(flush|clear|reset).*(dns|dns cache).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(renew|release|refresh).*(ip|dhcp|ip address).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(list|available|show|scan).*(wifi|networks|wifi networks).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "active connections", "network connections", "open ports", "listening ports", "tcp connections", "active ports"
  - **Type**: contains
  - **Description**: Triggers action: `run_cmd`


### News
- **Phrases**: "^.*(top|latest|find).*news.*india.*$", "^.*news.*in india.*$", "^.*news.*of india.*$"
  - **Type**: regex
  - **Description**: Triggers action: `news_plugin`
- **Phrases**: "^.*(international|world|global).*news.*$"
  - **Type**: regex
  - **Description**: Triggers action: `news_plugin`
- **Phrases**: "^.*(tech|technology|it).*news.*$"
  - **Type**: regex
  - **Description**: Triggers action: `news_plugin`
- **Phrases**: "^.*(business|market|finance|economy).*news.*$"
  - **Type**: regex
  - **Description**: Triggers action: `news_plugin`
- **Phrases**: "^.*(sports|cricket|football).*news.*$"
  - **Type**: regex
  - **Description**: Triggers action: `news_plugin`
- **Phrases**: "^.*(entertainment|bollywood|hollywood|movies).*news.*$"
  - **Type**: regex
  - **Description**: Triggers action: `news_plugin`
- **Phrases**: "^.*(top|latest|today's|todays).*news.*$", "^.*news.*headlines.*$", "^.*what('s|s| is) the news.*$", "^.*(tell me|get|read|show me) the news.*$"
  - **Type**: regex
  - **Description**: Triggers action: `news_plugin`


### Notifications
- **Phrases**: "send notification ", "send notify ", "notify me ", "show notification ", "send alert ", "alert ", "notify ", "send a notification ", "show a notification ", "pop up notification ", "display notification ", "send a alert "
  - **Type**: prefix
  - **Description**: Triggers action: `notify_text`
- **Phrases**: "notify me", "send notification", "notification", "show notification", "send a notification", "popup notification", "desktop notification"
  - **Type**: exact
  - **Description**: Triggers action: `notify`
- **Phrases**: "get reminder", "set reminder", "remind me", "create reminder"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "remind me to ", "remind me that ", "set a reminder to ", "remind "
  - **Type**: prefix
  - **Description**: Triggers action: `notify_text`


### Power
- **Phrases**: ".*(lock|secure).*(pc|computer|system|workstation|screen|desktop|windows).*"
  - **Type**: regex
  - **Description**: Triggers action: `lock_pc`
- **Phrases**: ".*(sleep|suspend|standby|hibernate).*(pc|computer|system|laptop).*", ".*(put).*(pc|computer|system).*(sleep|hibernate).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(abort|cancel|stop|dont|nevermind).*(shutdown|restart|power off|reboot).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(shutdown|turn off|power off|power down|shut down|switch off).*(pc|computer|system|laptop).*(now|immediately|fast|quick).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(shutdown|turn off|power off|power down|shut down|switch off).*(pc|computer|system|laptop|it).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(restart|reboot).*(pc|computer|system|laptop).*(now|immediately|fast|quick|force).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(restart|reboot).*(pc|computer|system|laptop|it).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(sign out|log off|log out|sign off|signout|logoff|switch user).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(safe mode).*(restart|boot).*", ".*(restart|boot).*(safe mode).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(turn on|enable|start).*(wifi|wi-fi|wireless).*", ".*(wifi|wi-fi).*(on).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(turn off|disable|stop).*(wifi|wi-fi|wireless).*", ".*(wifi|wi-fi).*(off).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(turn on|enable|start).*(bluetooth|bt).*", ".*(bluetooth|bt).*(on).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(turn off|disable|stop).*(bluetooth|bt).*", ".*(bluetooth|bt).*(off).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`


### Processes
- **Phrases**: ".*(list|show|what).*(processes|running|active|apps|open).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(kill|end|stop|terminate|force close) (process|app|program|task )?(.*)"
  - **Type**: regex
  - **Description**: Triggers action: `task_kill`
- **Phrases**: ".*(process count|how many processes|number of processes).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(top|highest|most).*(process|memory|cpu).*", ".*(high).*(cpu|memory).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(services|service).*(status|running|list|windows).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: "start service ", "stop service ", "restart service ", "enable service ", "disable service "
  - **Type**: prefix
  - **Description**: Triggers action: `run_cmd`


### Search
- **Phrases**: "search for ", "search about ", "search on ", "search regarding ", "search google for ", "search google ", "google for ", "google ", "look up on google ", "search the web for ", "search the internet for ", "search online for ", "search web for ", "look up ", "look for ", "lookup ", "look into ", "quick search for ", "quick look up ", "sara search ", "sara find ", "sara look up "
  - **Type**: prefix
  - **Description**: Triggers action: `search_plugin`
- **Phrases**: "deep search for ", "deep search ", "search and summarize ", "search and read about ", "search and read ", "scrape and search ", "detailed search for ", "detailed info on ", "read and search ", "fetch and read ", "full search on ", "comprehensive search for ", "research on ", "research about ", "do research on ", "in depth search for ", "in depth info on ", "search with content for ", "get full details on ", "get complete info on "
  - **Type**: prefix
  - **Description**: Triggers action: `search_plugin_scrape`


### System
- **Phrases**: ".*(ram|memory).*(usage|status|much|utilization|consumption).*", ".*(how is|tell me).*(ram|memory).*"
  - **Type**: regex
  - **Description**: Triggers action: `get_system_stats`
- **Phrases**: ".*(cpu|processor).*(usage|status|load|performance|utilization).*", ".*(how is|tell me).*(cpu|processor).*"
  - **Type**: regex
  - **Description**: Triggers action: `get_system_stats`
- **Phrases**: ".*(battery|power).*(status|level|percentage|health|life|remaining|charge).*", ".*(how much|tell me).*(battery|power).*"
  - **Type**: regex
  - **Description**: Triggers action: `get_system_stats`
- **Phrases**: ".*(system|pc|computer|hardware).*(status|info|overview|specs|summary|specifications).*", ".*(tell me about).*(system|pc|computer|hardware).*"
  - **Type**: regex
  - **Description**: Triggers action: `get_system_stats`
- **Phrases**: ".*(take|capture|grab|do|print)?.*(screenshot|desktop).*", ".*(take|capture|grab|do|print).*(screen).*"
  - **Type**: regex
  - **Description**: Triggers action: `take_screenshot`
- **Phrases**: ".*(take|capture|click|grab).*(photo|picture|camera|webcam).*"
  - **Type**: regex
  - **Description**: Triggers action: `capture_camera`
- **Phrases**: ".*(set|change|adjust).*(bright).*(to)? (\d+).*", ".*(bright).*(to)? (\d+).*"
  - **Type**: regex
  - **Description**: Triggers action: `set_brightness_level`
- **Phrases**: ".*(clean|clear|free|optimize|boost).*(pc|ram|memory).*"
  - **Type**: regex
  - **Description**: Triggers action: `clean_memory`
- **Phrases**: ".*(turn off|disable|sleep).*(screen|display|monitor).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`


### Typing
- **Phrases**: "type this ", "type text ", "send keys ", "type ", "type out ", "keyboard type ", "input text ", "enter text "
  - **Type**: prefix
  - **Description**: Triggers action: `type_text`
- **Phrases**: "copy to clipboard ", "copy this ", "copy text ", "clipboard copy ", "copy ", "copy this text ", "add to clipboard ", "save to clipboard "
  - **Type**: prefix
  - **Description**: Triggers action: `clipboard_copy`
- **Phrases**: "read clipboard", "what is on clipboard", "clipboard content", "get clipboard", "show clipboard", "clipboard data", "what's in my clipboard", "clipboard text", "read my clipboard", "check clipboard"
  - **Type**: exact
  - **Description**: Triggers action: `clipboard_read`
- **Phrases**: "clear clipboard", "empty clipboard", "erase clipboard", "clipboard clear"
  - **Type**: exact
  - **Description**: Triggers action: `run_cmd`


### Utilities
- **Phrases**: ".*(save|take|capture|grab)?.*(screenshot).*", ".*(save|take|capture|grab).*(screen).*"
  - **Type**: regex
  - **Description**: Triggers action: `take_screenshot`
- **Phrases**: ".*(open|show).*(recent).*(files|documents|used).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(show|open|what).*(startup).*(programs|apps|items).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(show|open|what).*(environment variables|env vars|path variable|system environment).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(system restore|restore point).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(check|what|show).*(windows|os) (version).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(check|show|run).*(directx|dxdiag).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(run|open|start).*(command|cmd) (as admin|administrator).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(what|show|tell me).*(time is it|current time|time).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(what|show|tell me).*(date|today's date).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(what|show|tell me).*(day is it|day of week|today's day).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(installed|list).*(programs|apps|software|applications).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(clear|delete).*(command history|history).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(what is my|show|tell me).*(username|user|who am i|logged in user).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(list|show|all).*(users|user accounts).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(show|get).*(wifi password|saved wifi|wi fi password).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(show|get|tell me|what is|what's|my).*(wifi|wi fi) password( for )?(.*)"
  - **Type**: regex
  - **Description**: Triggers action: `get_wifi_password`
- **Phrases**: ".*(generate|create|random).*(password|secure password).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(generate|create|new|random).*(uuid|guid).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(who).*(created|made|built).*(you|sara).*", ".*(what are you|what is sara|about yourself).*"
  - **Type**: regex
  - **Description**: Triggers action: `run_cmd`
- **Phrases**: ".*(help|commands|what can you do|capabilities).*"
  - **Type**: regex
  - **Description**: Triggers action: `show_help`


## Native System Commands (cmd_help.json)

### 🖥️ System Information
- **`systeminfo`**: Complete system configuration details
- **`systeminfo | find "System Boot Time"`**: System uptime
- **`systeminfo | find "Total Physical Memory"`**: Total RAM
- **`msinfo32`**: System Information GUI
- **`ver`**: Windows version
- **`winver`**: Windows version GUI
- **`wmic os get caption,version,buildnumber`**: OS details
- **`wmic os get lastbootuptime`**: Last boot time
- **`wmic os get TotalVisibleMemorySize,FreePhysicalMemory`**: RAM total/free
- **`wmic computersystem get model,name,manufacturer`**: PC model
- **`wmic bios get serialnumber`**: BIOS serial number
- **`wmic cpu get name,maxclockspeed,numberofcores`**: CPU info
- **`wmic baseboard get product,manufacturer,serialnumber`**: Motherboard info
- **`set`**: Environment variables
- **`driverquery`**: Installed drivers list
- **`driverquery /v`**: Drivers with details
- **`bcdedit /enum`**: Boot configuration
- **`chcp`**: Active console code page
- **`mode`**: Device status (ports, console)
- **`reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion`**: Windows version from registry


### ⚙️ Process Management
- **`tasklist`**: List all running processes
- **`tasklist /v`**: Processes with details
- **`tasklist /m`**: Processes with loaded DLLs
- **`tasklist /fo csv`**: Processes as CSV
- **`tasklist /fi "status eq running"`**: Only running processes
- **`tasklist /fi "memusage gt 50000"`**: Processes using >50MB RAM
- **`tasklist /fi "cpugt 10"`**: Processes using >10% CPU
- **`tasklist | sort /+65`**: Processes sorted by memory
- **`taskkill /PID 1234 /F`**: Kill process by PID (force)
- **`taskkill /IM notepad.exe /F`**: Kill process by name (force)
- **`taskkill /FI "PID ge 5000" /F`**: Kill all PIDs >= 5000
- **`wmic process where name='notepad.exe' delete`**: Kill process via WMIC
- **`wmic process get name,processid,workingsetsize`**: Process list with memory
- **`start "" notepad.exe`**: Launch a program
- **`start /min "" cmd`**: Launch minimized
- **`start /max "" notepad`**: Launch maximized
- **`start /wait "" cmd /c dir`**: Launch and wait
- **`start /b "" cmd /c ping localhost`**: Launch in background


### 💾 Disk & Storage
- **`wmic logicaldisk get size,freespace,caption,volumename`**: All drives with free space
- **`wmic logicaldisk where caption='C:' get size,freespace`**: C: drive space
- **`fsutil volume diskfree C:`**: C: free space (bytes)
- **`dir C:\`**: List C: root directory
- **`dir /s *.txt`**: Recursively find .txt files
- **`dir /s /b *.jpg > images.txt`**: List all JPGs to file
- **`dir /a`**: List all files including hidden
- **`tree C:\folder`**: Show folder tree structure
- **`chkdsk C:`**: Check disk for errors (read-only)
- **`chkdsk C: /f`**: Check disk and fix errors
- **`chkdsk C: /r`**: Check disk, find bad sectors
- **`sfc /scannow`**: System File Checker — repair system files
- **`dism /online /cleanup-image /scanhealth`**: DISM component store scan
- **`dism /online /cleanup-image /restorehealth`**: DISM component store repair
- **`defrag C: /u /v`**: Analyze drive fragmentation
- **`defrag C: /o`**: Optimize drive
- **`diskpart`**: Disk Partition Manager (interactive)
- **`diskpart /s script.txt`**: Run diskpart script
- **`wmic diskdrive get model,size,serialnumber`**: Physical disk info
- **`wmic volume get driveletter,freespace,capacity`**: Volume space (full)
- **`vssadmin list shadows`**: List Volume Shadow Copies
- **`vssadmin list volumes`**: List shadow-capable volumes
- **`mountvol`**: List mount points
- **`label C: NEWLABEL`**: Change volume label
- **`vol C:`**: Show volume label and serial
- **`compact /? or compact C:\folder`**: File compression status
- **`takeown /f C:\file /r /d y`**: Take ownership of file
- **`icacls C:\file /grant Users:F`**: Grant full permissions
- **`icacls C:\file /reset`**: Reset permissions
- **`attrib +h +s +r file.txt`**: Make file hidden + system + readonly
- **`attrib -h -s -r file.txt`**: Remove hidden + system + readonly
- **`del /f /s /q C:\temp\*`**: Force delete all in temp
- **`rmdir /s /q folder`**: Remove directory tree (force)
- **`move source dest`**: Move/rename file
- **`copy source dest`**: Copy file
- **`xcopy source dest /E /I /H`**: Copy directories (incl. hidden)
- **`robocopy source dest /E /COPYALL /R:0`**: Robust file copy with all attributes
- **`robocopy source dest /MIR`**: Mirror directory (backup)
- **`robocopy C:\Users\user\Desktop D:\backup\Desktop /E /R:3 /W:5`**: Backup Desktop


### 🌐 Network
- **`ipconfig`**: Basic IP configuration
- **`ipconfig /all`**: Full network configuration
- **`ipconfig /flushdns`**: Flush DNS resolver cache
- **`ipconfig /displaydns`**: Show DNS resolver cache
- **`ipconfig /renew`**: Renew DHCP lease
- **`ipconfig /release`**: Release DHCP lease
- **`ipconfig /registerdns`**: Register DNS
- **`ping 8.8.8.8`**: Test connectivity to Google DNS
- **`ping -n 20 google.com`**: Send 20 pings
- **`ping -t google.com`**: Continuous ping (Ctrl+C to stop)
- **`tracert google.com`**: Trace route to destination
- **`tracert -d google.com`**: Trace route (no DNS resolution)
- **`pathping google.com`**: Latency and packet loss per hop
- **`netstat -an`**: All active connections (TCP/UDP)
- **`netstat -b`**: Connections with owning process
- **`netstat -o`**: Connections with PID
- **`netstat -s`**: Per-protocol statistics
- **`netstat -r`**: Routing table
- **`netstat -ano | find ":80"`**: Find connections on port 80
- **`nslookup google.com`**: DNS lookup
- **`nslookup -type=mx gmail.com`**: MX record lookup
- **`nslookup -type=ns google.com`**: NS record lookup
- **`nslookup 8.8.8.8`**: Reverse DNS lookup
- **`getmac`**: MAC address
- **`getmac /v`**: MAC address with connection name
- **`netsh wlan show profiles`**: List saved WiFi networks
- **`netsh wlan show profile name="SSID" key=clear`**: Show WiFi password
- **`netsh wlan show interfaces`**: Current WiFi interface status
- **`netsh wlan connect name="SSID"`**: Connect to WiFi
- **`netsh wlan disconnect`**: Disconnect from WiFi
- **`netsh wlan show drivers`**: WiFi driver info
- **`netsh interface ip show config`**: IP config per interface
- **`netsh interface ip set address "Ethernet" static 192.168.1.100 255.255.255.0 192.168.1.1`**: Set static IP
- **`netsh advfirewall show allprofiles`**: Firewall status
- **`netsh advfirewall set allprofiles state off`**: Disable firewall
- **`netsh advfirewall set allprofiles state on`**: Enable firewall
- **`route print`**: IPv4 routing table
- **`arp -a`**: ARP cache table
- **`arp -d`**: Clear ARP cache
- **`telnet google.com 80`**: Test TCP connectivity (needs telnet)
- **`ssh user@host`**: SSH remote connection
- **`curl http://example.com`**: HTTP request (Windows 10+)
- **`curl -O https://example.com/file.zip`**: Download file via curl
- **`bitsadmin /transfer job /download /priority high https://example.com/file.zip C:\file.zip`**: Background download
- **`net use X: \\server\share`**: Map network drive
- **`net use X: /delete`**: Remove mapped drive
- **`net share`**: List shared folders
- **`net share MyShare=C:\Folder /grant:Everyone,full`**: Create a share
- **`net view`**: List network computers
- **`wmic nic get name,speed,macaddress`**: Network adapter details
- **`wmic netuse get *`**: Mapped drives details


### 🔋 Power Management
- **`powercfg /batteryreport`**: Generate battery health report
- **`powercfg /energy`**: Energy efficiency audit (60s)
- **`powercfg /sleepstudy`**: Sleep study report
- **`powercfg /list`**: List power schemes
- **`powercfg /setactive GUID`**: Set active power scheme
- **`powercfg /query`**: Current power scheme settings
- **`powercfg /hibernate on`**: Enable hibernation
- **`powercfg /hibernate off`**: Disable hibernation
- **`powercfg /change standby-timeout-ac 30`**: Set standby timeout (AC, mins)
- **`powercfg /change hibernate-timeout-ac 60`**: Set hibernate timeout
- **`powercfg /lastwake`**: What woke the PC last
- **`powercfg /devicequery wake_armed`**: Devices that can wake PC
- **`powercfg /waketimers`**: Active wake timers
- **`powercfg /requests`**: Active power requests
- **`shutdown /s /t 0`**: Shutdown immediately
- **`shutdown /s /t 60 /c "System will restart"`**: Shutdown with 60s delay and message
- **`shutdown /r /t 0`**: Restart immediately
- **`shutdown /r /o /t 0`**: Restart to Advanced Boot Options
- **`shutdown /l`**: Log off current user
- **`shutdown /h`**: Hibernate
- **`shutdown /a`**: Abort pending shutdown
- **`shutdown /fw /t 0`**: Restart to firmware (UEFI)
- **`shutdown /sg /t 0`**: Shutdown and restart apps on next boot
- **`wmic path Win32_Battery get EstimatedChargeRemaining,Status`**: Battery charge %


### 👤 User & Session Management
- **`whoami`**: Current user
- **`whoami /user`**: Current user SID
- **`whoami /groups`**: User group memberships
- **`whoami /priv`**: User privileges
- **`net user`**: List all user accounts
- **`net user username`**: User account details
- **`net user username *`**: Set/change password
- **`net user username /add`**: Create new user
- **`net user username /delete`**: Delete user
- **`net user username /active:yes`**: Enable user account
- **`net user username /active:no`**: Disable user account
- **`net localgroup`**: List local groups
- **`net localgroup Administrators`**: List admins
- **`net localgroup Administrators username /add`**: Add user to Administrators
- **`net localgroup Administrators username /delete`**: Remove from Administrators
- **`net accounts`**: Password policy settings
- **`quser`**: Currently logged-in users
- **`query session`**: All sessions on this PC
- **`query user`**: User sessions
- **`tscon sessionid /dest:console`**: Switch to console session
- **`logoff sessionid`**: Log off a session
- **`wmic useraccount get name,sid,status,disabled`**: All local user accounts


### 🔧 Service Management
- **`sc query`**: List running services
- **`sc query type= service state= all`**: List all services
- **`sc queryex Spooler`**: Service details (Spooler)
- **`sc start Spooler`**: Start a service
- **`sc stop Spooler`**: Stop a service
- **`sc pause Spooler`**: Pause a service
- **`sc continue Spooler`**: Resume a paused service
- **`sc config Spooler start= auto`**: Set service to auto-start
- **`sc config Spooler start= disabled`**: Disable a service
- **`sc config Spooler start= demand`**: Set service to manual start
- **`sc description Spooler "New description"`**: Set service description
- **`sc failure Spooler reset= 86400 actions= restart/5000`**: Configure failure recovery
- **`sc delete Spooler`**: Delete a service (careful!)
- **`sc queryex type= service | find "not running"`**: Stopped services
- **`net start`**: List running services (simple)
- **`net start Spooler`**: Start a service (net)
- **`net stop Spooler`**: Stop a service (net)
- **`wmic service get name,displayname,startmode,state`**: Services via WMIC


### 🔄 Windows Update & Recovery
- **`wmic qfe list brief /format:table`**: List installed updates
- **`wmic qfe get hotfixid,installedon,description`**: Update details
- **`systeminfo | find "Hotfix(s)"`**: Installed hotfixes
- **`wuaulaui`**: Check for updates (Control Panel)
- **`UsoClient scannow`**: Trigger Windows Update scan
- **`UsoClient startinstall`**: Install pending updates
- **`UsoClient refreshsettings`**: Refresh update settings
- **`wuauclt /detectnow /updatenow`**: Force update detection (legacy)
- **`rstrui`**: System Restore GUI
- **`rstrui /restorepoint <name>`**: System restore via console
- **`systemreset -factoryreset`**: Reset this PC (factory reset)
- **`systemreset -cleanpc`**: Fresh start (keeps personal files)
- **`reagentc /info`**: Windows Recovery Environment status
- **`reagentc /enable`**: Enable WinRE
- **`reagentc /boottore`**: Boot to WinRE on next restart
- **`bcdedit /set {default} recoveryenabled no`**: Disable auto repair
- **`bcdedit /set {default} recoveryenabled yes`**: Enable auto repair


### ⏰ Date, Time & Scheduling
- **`date`**: Display/set date
- **`date 01-15-2025`**: Set date (MM-DD-YYYY)
- **`time`**: Display/set time
- **`time 14:30:00`**: Set time
- **`tzutil /g`**: Current time zone
- **`tzutil /l`**: List all time zones
- **`tzutil /s "India Standard Time"`**: Set time zone
- **`wmic os get localtime`**: Current system time (UTC)
- **`wmic path Win32_LocalTime get *`**: Detailed local time
- **`schtasks`**: List scheduled tasks
- **`schtasks /query /v`**: Tasks with details
- **`schtasks /query /fo csv`**: Tasks as CSV
- **`schtasks /create /sc daily /tn "MyTask" /tr "notepad.exe" /st 09:00`**: Create daily task
- **`schtasks /create /sc onlogon /tn "StartupTask" /tr "notepad.exe" /delay 0000:30`**: Task on user logon
- **`schtasks /create /sc minute /mo 5 /tn "MyTask" /tr "notepad.exe"`**: Task every 5 minutes
- **`schtasks /delete /tn "MyTask" /f`**: Delete a scheduled task
- **`schtasks /change /tn "MyTask" /disable`**: Disable a task
- **`schtasks /change /tn "MyTask" /enable`**: Enable a task
- **`schtasks /run /tn "MyTask"`**: Run a task immediately
- **`schtasks /end /tn "MyTask"`**: Stop a running task
- **`schtasks /export /tn "MyTask" /xml task.xml`**: Export task as XML
- **`schtasks /create /xml task.xml /tn "MyTask"`**: Import task from XML
- **`timeout /t 10`**: Pause for 10 seconds
- **`timeout /t 10 /nobreak`**: Pause (no key skip)
- **`sleep 10`**: Sleep for 10 seconds (PowerShell)


### 📁 File Operations
- **`type file.txt`**: Display file contents
- **`type file.txt | more`**: Display file page by page
- **`more file.txt`**: Display file with pagination
- **`find "text" file.txt`**: Search text in file
- **`find /i "text" file.txt`**: Case-insensitive search
- **`find /v "text" file.txt`**: Lines NOT containing text
- **`findstr "error" *.log`**: Regex search in files
- **`findstr /s /i "TODO" *.cpp`**: Recursive regex search
- **`findstr /m "error" *.log`**: Files containing match
- **`sort file.txt`**: Sort file alphabetically
- **`sort /r file.txt`**: Sort in reverse
- **`sort /+10 file.txt`**: Sort by column 10
- **`fc file1.txt file2.txt`**: Compare files
- **`comp file1.txt file2.txt`**: Compare files (binary)
- **`diff file1.txt file2.txt`**: Diff (PowerShell)
- **`print file.txt`**: Print file
- **`echo text > file.txt`**: Write text to file (overwrite)
- **`echo text >> file.txt`**: Append text to file
- **`type nul > file.txt`**: Create empty file
- **`ren old.txt new.txt`**: Rename file
- **`ren *.txt *.bak`**: Batch rename extension
- **`move *.txt C:\folder`**: Move all .txt files
- **`copy *.txt C:\folder`**: Copy all .txt files
- **`xcopy dir1 dir2 /E /I /Y`**: Copy directory tree (no prompts)
- **`xcopy dir1 dir2 /T /E`**: Copy directory structure only
- **`replace new.txt old.txt`**: Replace file in destination
- **`expand archive.cab target`**: Extract CAB archive
- **`expand -R archive.cab target\`**: Extract CAB recursively
- **`icacls file.txt /grant Everyone:F`**: Grant full access
- **`icacls file.txt /deny Everyone:R`**: Deny read access
- **`icacls file.txt /remove Everyone`**: Remove user/group ACL
- **`takeown /f C:\Windows\system.ini`**: Take ownership of file
- **`cipher /e C:\folder`**: Encrypt folder (EFS)
- **`cipher /d C:\folder`**: Decrypt folder
- **`cipher /w:C:\drive`**: Wipe free space
- **`compact /c file.txt`**: Compress file (NTFS)
- **`compact /u file.txt`**: Decompress file
- **`fsutil file createnew test.bin 1048576`**: Create 1MB test file
- **`fsutil behavior query disablecompression`**: Check compression status
- **`where notepad`**: Locate executable in PATH
- **`where /r C:\ *.exe`**: Find all EXEs recursively


### 📋 Registry Operations
- **`reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion`**: List registry keys
- **`reg query HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion /v ProgramFilesDir`**: Read specific value
- **`reg add HKCU\Software\MyApp /v Version /t REG_SZ /d "1.0" /f`**: Add/create registry value
- **`reg delete HKCU\Software\MyApp /v Version /f`**: Delete registry value
- **`reg delete HKCU\Software\MyApp /f`**: Delete entire registry key
- **`reg copy HKLM\Software\MyApp HKCU\Software\MyApp /s /f`**: Copy registry key
- **`reg export HKLM\Software\MyApp backup.reg`**: Export registry key
- **`reg import backup.reg`**: Import registry file
- **`reg save HKLM\Software\MyApp hive.hiv`**: Save hive to file
- **`reg restore HKLM\Software\MyApp hive.hiv`**: Restore hive from file
- **`reg compare HKLM\Software\MyApp HKCU\Software\MyApp`**: Compare two registry keys
- **`regini script.ini`**: Apply registry permissions script


### 📜 Event Logs
- **`eventvwr`**: Event Viewer GUI
- **`wevtutil el`**: List all event logs
- **`wevtutil gl System`**: Event log configuration
- **`wevtutil qe System /c:10 /rd:true /f:text`**: Last 10 System events
- **`wevtutil qe Application /c:20 /rd:true /f:text`**: Last 20 Application events
- **`wevtutil qe Security /c:5 /rd:true /f:text`**: Last 5 Security events
- **`wevtutil epl System backup.evtx`**: Export System log
- **`wevtutil cl System`**: Clear System event log
- **`wevtutil set-log System /enabled:false`**: Disable System log
- **`wevtutil set-log System /enabled:true`**: Enable System log
- **`wevtutil sl System /ms:209715200`**: Set log max size (200MB)
- **`Get-WinEvent -LogName System -MaxEvents 10 | Format-List`**: System events via PowerShell


### 🔒 Security & Certificates
- **`certutil -store My`**: List personal certificates
- **`certutil -store Root`**: List trusted root CAs
- **`certutil -store CA`**: List intermediate CAs
- **`certutil -addstore Root cert.cer`**: Install root certificate
- **`certutil -delstore Root serial`**: Delete certificate
- **`certutil -exportPFX My serial cert.pfx`**: Export cert with private key
- **`certutil -urlcache *`**: Clear certificate cache
- **`certutil -hashfile file.txt SHA256`**: Compute file hash
- **`certutil -hashfile file.txt MD5`**: Compute MD5 hash
- **`gpresult /r`**: Group Policy results
- **`gpresult /h report.html`**: Export GP report as HTML
- **`gpupdate /force`**: Force Group Policy update
- **`gpupdate /target:computer`**: Update computer policies
- **`secedit /export /cfg secpolicy.inf`**: Export security policy
- **`secedit /configure /db secpol.sdb /cfg secpolicy.inf`**: Apply security policy
- **`cipher /w:C:`**: Wipe free space (security)
- **`cipher /e C:\folder`**: Enable EFS encryption
- **`manage-bde -status`**: BitLocker status
- **`manage-bde -on C:`**: Enable BitLocker
- **`manage-bde -off C:`**: Decrypt BitLocker drive
- **`manage-bde -protectors -get C:`**: BitLocker protectors
- **`manage-bde -changepassword C:`**: Change BitLocker password
- **`net accounts /minpwlen 8`**: Set minimum password length
- **`net accounts /maxpwage 30`**: Set max password age (days)
- **`net accounts /lockoutthreshold 5`**: Account lockout threshold
- **`wmic path Win32_ComputerSystemProduct get UUID`**: System UUID (tied to hardware)
- **`reg query "HKLM\SYSTEM\CurrentControlSet\Control\SecureBoot\State"`**: Check SecureBoot status


### 💻 Hardware & Performance
- **`wmic cpu get name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed`**: Full CPU details
- **`wmic cpu get loadpercentage`**: CPU usage %
- **`wmic memorychip get capacity,speed,manufacturer,partnumber`**: RAM module details
- **`wmic diskdrive get model,size,interfaceType`**: Disk drive model/size
- **`wmic path Win32_VideoController get name,adapterram,driverdate`**: GPU info
- **`wmic path Win32_SoundDevice get name,manufacturer`**: Audio device
- **`wmic path Win32_Keyboard get name,description`**: Keyboard info
- **`wmic path Win32_PointingDevice get name,manufacturer`**: Mouse info
- **`wmic path Win32_USBControllerDevice get *`**: USB devices
- **`wmic path Win32_NetworkAdapter where NetEnabled=True get Name,Speed,AdapterType`**: Active network adapters
- **`wmic csproduct get name,identifyingnumber,uuid`**: System model & UUID
- **`wmic path Win32_ComputerSystem get TotalPhysicalMemory,UserName,Domain`**: RAM total & logged user
- **`wmic path Win32_LogicalMemoryConfiguration get *`**: Memory configuration
- **`wmic path Win32_Printer get name,driverName,portName`**: Installed printers
- **`wmic path Win32_Battery get estimatedChargeRemaining,expectedLife`**: Battery charge & life
- **`wmic path Win32_Fan get *`**: Fan speeds (if supported)
- **`wmic path Win32_TemperatureProbe get *`**: Temperature sensors
- **`dxdiag`**: DirectX Diagnostic GUI
- **`dxdiag /t dxdiag.txt`**: Export DxDiag report
- **`msdt.exe /id PerformanceDiagnostic`**: Performance troubleshooter
- **`perfmon /res`**: Resource Monitor GUI
- **`perfmon /rel`**: Reliability Monitor
- **`perfmon /report`**: System Diagnostic Report
- **`perfmon /sys`**: Performance Monitor standalone
- **`typeperf -q`**: List available performance counters
- **`typeperf "\Processor(_Total)\% Processor Time" -sc 10`**: Sample CPU 10 times
- **`lodctr /R`**: Rebuild performance counters
- **`winsat cpuformal`**: CPU performance assessment
- **`winsat diskformal`**: Disk performance assessment
- **`winsat memformal`**: Memory performance assessment
- **`winsat mediaformal`**: Media (GPU) assessment
- **`winsat dwmformal`**: Desktop Window Manager assessment
- **`winsat graphicsformal`**: Graphics performance assessment
- **`winsat prepop`**: Pre-populate WinSAT data


### 📦 Virtualization & Containers
- **`wmic path Win32_ComputerSystem get HypervisorPresent`**: Check virtualization support
- **`systeminfo | find "Hyper-V"`**: Hyper-V requirements status
- **`bcdedit /set hypervisorlaunchtype auto`**: Enable Hyper-V
- **`bcdedit /set hypervisorlaunchtype off`**: Disable Hyper-V
- **`dism /online /enable-feature /featurename:Microsoft-Hyper-V-All /all /quiet`**: Install Hyper-V
- **`dism /online /disable-feature /featurename:Microsoft-Hyper-V-All /quiet`**: Remove Hyper-V
- **`dism /online /enable-feature /featurename:VirtualMachinePlatform /all /quiet`**: Enable WSL / VM Platform
- **`dism /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /quiet`**: Enable WSL
- **`wsl --list --verbose`**: List WSL instances
- **`wsl --install`**: Install WSL default distro
- **`wsl --shutdown`**: Shutdown all WSL instances
- **`wsl --set-version Ubuntu 2`**: Set WSL version
- **`wsl --export Ubuntu backup.tar`**: Export WSL distro
- **`wsl --import MyDistro install-dir backup.tar`**: Import WSL distro
- **`wsl -d Ubuntu -u root`**: Run WSL as root


### 🎨 Fonts & Display
- **`wmic path Win32_DesktopMonitor get name,screenheight,screenwidth`**: Monitor resolution
- **`wmic path Win32_DesktopMonitor get *`**: Full monitor details
- **`wmic path Win32_VideoController get CurrentHorizontalResolution,CurrentVerticalResolution,RefreshRate`**: Current resolution and refresh rate
- **`wmic path Win32_VideoController get MaxRefreshRate,MinRefreshRate`**: Supported refresh rates
- **`dpiScaling.exe /help`**: DPI scaling settings
- **`fontview fontfile.ttf`**: Preview a font
- **`presentationsettings`**: Presentation settings GUI


### 🔑 Product Activation & Licensing
- **`slmgr /dli`**: Windows license basic info
- **`slmgr /dlv`**: Detailed license info
- **`slmgr /xpr`**: License expiration date
- **`slmgr /ipk XXXXX-XXXXX-XXXXX-XXXXX-XXXXX`**: Install product key
- **`slmgr /skms kms.server.com`**: Set KMS server
- **`slmgr /ato`**: Activate Windows online
- **`slmgr /rearm`**: Reset activation timers
- **`wmic path SoftwareLicensingProduct where "LicenseStatus=0" get *`**: Unlicensed products


### 🔍 Network Diagnostics & Troubleshooting
- **`netsh trace start persistent=yes capture=yes tracefile=network.etl`**: Start network packet capture
- **`netsh trace stop`**: Stop packet capture
- **`netsh wlan show wlanreport`**: WiFi connectivity report
- **`netsh wlan show hostednetwork`**: Hosted network status
- **`netsh wlan set hostednetwork mode=allow ssid=MyWiFi key=password`**: Create hosted WiFi
- **`netsh wlan start hostednetwork`**: Start hosted WiFi
- **`netsh wlan stop hostednetwork`**: Stop hosted WiFi
- **`netsh advfirewall firewall add rule name="Open 8080" dir=in action=allow protocol=TCP localport=8080`**: Open firewall port
- **`netsh advfirewall firewall delete rule name="Open 8080"`**: Close firewall port
- **`netsh interface portproxy add v4tov4 listenaddress=0.0.0.0 listenport=8080 connectaddress=192.168.1.100 connectport=80`**: Port forwarding (portproxy)
- **`netsh interface portproxy delete v4tov4 listenaddress=0.0.0.0 listenport=8080`**: Remove portproxy
- **`netsh interface portproxy show all`**: List portproxy rules
- **`netsh http show urlacl`**: Show HTTP URL reservations
- **`netsh http add urlacl url=http://+:8080/ user=Everyone`**: Reserve URL for all users
- **`netsh http delete urlacl url=http://+:8080/`**: Delete URL reservation
- **`netsh winhttp show proxy`**: Show WinHTTP proxy
- **`netsh winhttp set proxy proxy-server=myproxy:8080`**: Set system proxy
- **`netsh winhttp reset proxy`**: Reset proxy settings
- **`netsh mbn show interfaces`**: Mobile broadband status
- **`nbtstat -a 192.168.1.1`**: NetBIOS name table
- **`nbtstat -c`**: NetBIOS cache
- **`nbtstat -R`**: Purge NetBIOS cache
- **`nbtstat -S`**: NetBIOS sessions table
- **`fport`**: Open ports to process mapping
- **`netstat -ano | findstr LISTENING`**: Listening ports only
- **`netstat -ano | findstr ESTABLISHED`**: Active connections only
- **`wmic netlogin get *`**: Network login profile


### 🖨️ Printing
- **`wmic path Win32_Printer get name,status,default,network`**: All printers
- **`wmic path Win32_Printer where default=true get name`**: Default printer
- **`wmic printer where name='PrinterName' set default=true`**: Set default printer
- **`rundll32 printui.dll,PrintUIEntry /y /n "PrinterName"`**: Set default printer via rundll32
- **`rundll32 printui.dll,PrintUIEntry /in /n "PrinterName"`**: Install network printer
- **`rundll32 printui.dll,PrintUIEntry /dn /n "PrinterName"`**: Remove printer
- **`print /D:"PrinterName" file.txt`**: Print file to specific printer
- **`net stop Spooler && net start Spooler`**: Restart print spooler
- **`del /q /f %systemroot%\system32\spool\printers\*`**: Clear print queue


### 🖥️ Remote Desktop
- **`mstsc`**: Remote Desktop Connection GUI
- **`mstsc /v:computer`**: RDP to computer
- **`mstsc /v:192.168.1.100:3389`**: RDP with port
- **`mstsc /admin`**: RDP console session
- **`mstsc /f`**: RDP full screen
- **`mstsc /multimon`**: RDP multi-monitor
- **`qwinsta`**: Query RDP sessions
- **`rwinsta sessionid`**: Reset RDP session
- **`change logon /enable`**: Allow RDP logons
- **`change logon /disable`**: Disable new RDP logons
- **`reg query "HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server" /v fDenyTSConnections`**: Check if RDP enabled
- **`reg add "HKLM\SYSTEM\CurrentControlSet\Control\Terminal Server" /v fDenyTSConnections /t REG_DWORD /d 0 /f`**: Enable RDP


### 🧩 Windows Features & Components
- **`dism /online /get-features /format:table`**: List all Windows features
- **`dism /online /enable-feature /featurename:NetFx3 /all /source:D:\sources\sxs /quiet`**: Enable .NET 3.5
- **`dism /online /disable-feature /featurename:Internet-Explorer-Optional-amd64 /quiet`**: Disable IE
- **`dism /online /get-features /format:table | find "Disabled"`**: Disabled features only
- **`dism /online /get-features /format:table | find "Enabled"`**: Enabled features only
- **`dism /online /get-currentedition`**: Current Windows edition
- **`dism /online /get-targeteditions`**: Upgradable editions
- **`dism /online /set-edition ServerDatacenter /productkey:XXXXX /accepteula`**: Upgrade Windows edition
- **`optionalfeatures`**: Windows Features GUI


### ⚡ Environment & System Variables
- **`set`**: List all environment variables
- **`set PATH`**: Show PATH variable
- **`set MYVAR=value`**: Set temporary variable
- **`setx MYVAR value`**: Set user environment variable
- **`setx MYVAR value /M`**: Set system environment variable
- **`setx PATH "%PATH%;C:\MyApp" /M`**: Append to system PATH
- **`reg query "HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\Environment"`**: System env vars in registry
- **`reg query "HKCU\Environment"`**: User env vars in registry
- **`echo %USERNAME%`**: Current username
- **`echo %COMPUTERNAME%`**: Computer name
- **`echo %OS%`**: Operating system
- **`echo %PROCESSOR_ARCHITECTURE%`**: Processor architecture
- **`echo %NUMBER_OF_PROCESSORS%`**: Number of processors
- **`echo %USERPROFILE%`**: User profile path
- **`echo %APPDATA%`**: AppData Roaming path
- **`echo %LOCALAPPDATA%`**: AppData Local path
- **`echo %TEMP%`**: Temp directory path
- **`echo %SYSTEMROOT%`**: Windows directory
- **`echo %PUBLIC%`**: Public folder path
- **`echo %ALLUSERSPROFILE%`**: All users profile path


### 📜 Batch & Scripting
- **`for %i in (*.txt) do echo %i`**: Loop through files
- **`for /r C:\ %i in (*.log) do @echo %i`**: Recursive file loop
- **`for /f %i in (file.txt) do echo %i`**: Read file line by line
- **`for /l %i in (1,1,10) do echo %i`**: Numeric loop 1-10
- **`for /f "tokens=*" %i in ('dir /b') do echo %i`**: Capture command output
- **`if exist file.txt echo Found`**: Check if file exists
- **`if not exist folder\ md folder`**: Create folder if not exists
- **`if %errorlevel% equ 0 echo Success`**: Check last error code
- **`if "%var%"=="value" echo Match`**: String comparison
- **`choice /c YN /n /m "Continue?"`**: Interactive choice prompt
- **`set /p input=Enter something: `**: Prompt for user input
- **`set /a result=5+3*2`**: Arithmetic expression
- **`color 0A`**: Set console color (black/green)
- **`title MyScript`**: Change console window title
- **`cls`**: Clear screen
- **`pause`**: Press any key to continue...
- **`cmd /k`**: Open new cmd (stays open)
- **`cmd /c command`**: Run command and exit
- **`start cmd /k "cd C:\ && dir"`**: New cmd, run command, stay open
- **`exit /b 1`**: Exit batch with error code 1
- **`assoc .txt=txtfile`**: Associate file extension
- **`assoc .html=htmlfile`**: Associate .html
- **`ftype txtfile="%SystemRoot%\system32\NOTEPAD.EXE" "%1"`**: Set file type handler
- **`ftype htmlfile="C:\Program Files\Google\Chrome\Application\chrome.exe" "%1"`**: Set default browser for html
- **`powercfg /list > power.txt && notepad power.txt`**: Chain commands in sequence
- **`dir || echo Failed`**: Run only if previous fails
- **`dir && echo Success`**: Run only if previous succeeds
- **`dir 2>&1`**: Redirect stderr to stdout
- **`dir > output.txt 2>&1`**: Redirect both outputs to file


### 🖥️️ Console & Terminal
- **`color 0A`**: Set console color (black/green)
- **`color /?`**: Console color help
- **`title MyScript`**: Change console window title
- **`mode con: cols=120 lines=50`**: Set console buffer size
- **`cls`**: Clear screen
- **`pause`**: Press any key to continue...
- **`prompt $P$G`**: Set prompt (default)
- **`prompt $T $P$G`**: Set prompt with time
- **`doskey /history`**: Show command history
- **`doskey ls=dir /w $*`**: Create 'ls' alias for dir /w
- **`doskey ..=cd ..`**: Create '..' alias for cd ..
- **`cmd /k`**: Open new cmd (stays open)
- **`cmd /c command`**: Run command and exit
- **`exit /b 1`**: Exit batch with error code 1
- **`exit`**: Close command prompt
- **`start cmd /k "cd C:\ && dir"`**: New cmd, run command, stay open


### 🛡️️ Windows Defender & Security
- **`powershell Get-MpComputerStatus`**: Defender status overview
- **`powershell Start-MpScan -ScanType QuickScan`**: Quick scan with Defender
- **`powershell Start-MpScan -ScanType FullScan`**: Full system scan
- **`powershell Update-MpSignature`**: Update Defender definitions
- **`powershell Add-MpPreference -ExclusionPath C:\folder`**: Add exclusion path
- **`powershell Add-MpPreference -ExclusionExtension .exe`**: Add exclusion extension
- **`powershell Get-MpPreference | select ExclusionPath`**: List all exclusions
- **`powershell Set-MpPreference -DisableRealtimeMonitoring $true`**: Disable real-time protection
- **`powershell Set-MpPreference -DisableRealtimeMonitoring $false`**: Enable real-time protection
- **`powershell Set-MpPreference -MAPSReporting 2`**: Enable cloud protection
- **`powershell Get-MpThreat`**: List detected threats
- **`start windowsdefender:`**: Open Windows Security app
- **`wmic /namespace:\\root\securitycenter2 path antivirusproduct get displayname`**: Installed AV product
- **`reg query "HKLM\SOFTWARE\Microsoft\Windows Defender" /v DisableAntiSpyware`**: Check if Defender is disabled


### 🔌 Devices & Drivers
- **`devmgmt.msc`**: Device Manager GUI
- **`driverquery`**: List all drivers
- **`driverquery /v`**: Drivers with details
- **`driverquery /si`**: Driver signing status
- **`pnputil /enum-drivers`**: List all driver packages
- **`pnputil /enum-devices`**: List all devices
- **`pnputil /enum-devices /connected`**: Only connected devices
- **`pnputil /enum-devices /problem`**: Devices with problems
- **`pnputil /export-driver oem0.inf C:\backup`**: Export driver package
- **`pnputil /add-driver driver.inf /install`**: Install driver
- **`pnputil /delete-driver oem0.inf /uninstall`**: Uninstall driver
- **`pnputil /scan-devices`**: Scan for hardware changes
- **`pnputil /disable-device "DeviceInstanceID"`**: Disable device
- **`pnputil /enable-device "DeviceInstanceID"`**: Enable device
- **`wmic path Win32_PnPEntity get name,deviceid,status`**: All PnP devices
- **`wmic path Win32_PnPEntity where configmanagererrorcode != 0 get name`**: Problem devices
- **`wmic path Win32_SystemDriver get name,displayname,state,status`**: System drivers


### 🛒 Windows Store & Apps
- **`winget list`**: List installed packages
- **`winget install notepadplusplus`**: Install Notepad++
- **`winget install 7zip.7zip`**: Install 7-Zip
- **`winget install Git.Git`**: Install Git
- **`winget install Google.Chrome`**: Install Chrome
- **`winget install Mozilla.Firefox`**: Install Firefox
- **`winget install VideoLAN.VLC`**: Install VLC
- **`winget install Microsoft.VisualStudioCode`**: Install VS Code
- **`winget install Python.Python.3.12`**: Install Python
- **`winget uninstall package-name`**: Uninstall package
- **`winget upgrade --all`**: Upgrade all packages
- **`winget search firefox`**: Search for packages
- **`winget show 7zip.7zip`**: Package details
- **`winget export -o packages.json`**: Export installed list
- **`winget import -i packages.json`**: Import/install from list
- **`powershell Get-AppxPackage | select Name,PackageFullName`**: List Store apps
- **`powershell Get-AppxPackage -AllUsers | select Name,PackageFullName`**: Store apps for all users
- **`powershell Remove-AppxPackage PackageFullName`**: Remove Store app for user


### 💿 System Protection & Backup
- **`rstrui`**: System Restore GUI
- **`powershell Enable-ComputerRestore -Drive C:\`**: Enable System Restore on C:
- **`powershell Disable-ComputerRestore -Drive C:\`**: Disable System Restore
- **`powershell Checkpoint-Computer -Description "BeforeInstall" -RestorePointType MODIFY_SETTINGS`**: Create restore point
- **`powershell Get-ComputerRestorePoint`**: List restore points
- **`vssadmin list shadows /for=C:`**: List VSS snapshots on C:
- **`vssadmin create shadow /for=C:`**: Create VSS snapshot
- **`vssadmin delete shadows /for=C: /all`**: Delete all snapshots
- **`vssadmin list shadowstorage`**: VSS storage usage
- **`vssadmin resize shadowstorage /for=C: /on=C: /maxsize=10%`**: Limit VSS to 10% of volume
- **`robocopy C:\source D:\backup /MIR /R:2 /W:3 /LOG:backup.log`**: Mirror backup with log
- **`wbadmin start backup -backupTarget:D: -include:C: -allCritical -quiet`**: Windows Server Backup
- **`wbadmin get versions`**: List backup versions
- **`wbadmin start recovery -version:01/01/2025-00:00 -itemType:File -items:C:\file.txt -recoveryTarget:C:\restored`**: Restore file from backup
- **`dism /capture-image /imagefile:backup.wim /capturedir:C:\ /name:"Windows Backup"`**: Create WIM backup
- **`dism /apply-image /imagefile:backup.wim /index:1 /applydir:C:\`**: Apply WIM backup


### 🔮 PowerShell Commands
- **`powershell Get-Process | Sort-Object CPU -Descending | Select -First 10`**: Top 10 CPU processes
- **`powershell Get-Process | Sort-Object WorkingSet -Descending | Select -First 10`**: Top 10 memory processes
- **`powershell Get-Service | Where Status -eq Running`**: Running services
- **`powershell Get-Service | Where Status -eq Stopped`**: Stopped services
- **`powershell Get-ChildItem -Recurse -Filter *.txt | Select FullName`**: Recursive file search
- **`powershell Get-EventLog System -Newest 20 | FT`**: Last 20 System events
- **`powershell Get-WmiObject Win32_LogicalDisk | Select DeviceID,Size,FreeSpace`**: All drive space
- **`powershell Get-WmiObject Win32_Processor | Select Name,NumberOfCores,LoadPercentage`**: CPU info
- **`powershell Get-NetIPAddress | Sort InterfaceIndex | FT InterfaceAlias,IPAddress,AddressFamily`**: All IP addresses
- **`powershell Test-NetConnection google.com -Port 443`**: Test TCP connectivity
- **`powershell Get-HotFix | Sort InstalledOn -Descending | Select -First 10`**: Last 10 installed updates
- **`powershell Get-ComputerInfo | Select WindowsVersion,WindowsBuildLabEx`**: Windows version info
- **`powershell Set-TimeZone -Id "India Standard Time"`**: Set timezone
- **`powershell Get-Volume | Select DriveLetter,FileSystem,DriveType,SizeRemaining`**: Volume space info
- **`powershell Invoke-WebRequest -Uri https://example.com -Method GET`**: HTTP GET request
- **`powershell Get-ChildItem Env: | Format-Table -AutoSize`**: All environment variables
- **`powershell Stop-Process -Name notepad -Force`**: Stop process by name
- **`powershell Format-Volume -DriveLetter D -FileSystem NTFS -NewFileSystemLabel "Data"`**: Format volume
- **`powershell Repair-Volume -DriveLetter C -Scan`**: Scan drive for errors


### 🔧 Troubleshooting & Repair
- **`sfc /scannow`**: System File Checker repair
- **`sfc /verifyonly`**: Verify system files only
- **`dism /online /cleanup-image /scanhealth`**: DISM health scan
- **`dism /online /cleanup-image /restorehealth`**: DISM repair
- **`dism /online /cleanup-image /startcomponentcleanup /resetbase`**: DISM reset base (clean WinSxS)
- **`chkdsk C: /f /r`**: Check and repair disk
- **`chkdsk C: /scan`**: Online scan (no reboot)
- **`chkdsk C: /spotfix`**: Disk spot fix (no reboot)
- **`msdt.exe -id SystemMaintenanceDiagnostic`**: System Maintenance troubleshooter
- **`msdt.exe -id PerformanceDiagnostic`**: Performance troubleshooter
- **`msdt.exe -id PowerDiagnostic`**: Power troubleshooter
- **`msdt.exe -id NetworkDiagnosticsWeb`**: Network troubleshooter
- **`msdt.exe -id AudioPlaybackDiagnostic`**: Audio troubleshooter
- **`msdt.exe -id PrinterDiagnostic`**: Printer troubleshooter
- **`msdt.exe -id WindowsUpdateDiagnostic`**: Windows Update troubleshooter
- **`msdt.exe -id BluescreenDiagnostic`**: Blue Screen troubleshooter
- **`mdsched.exe`**: Windows Memory Diagnostic
- **`resmon`**: Resource Monitor
- **`wsreset`**: Reset Windows Store cache
- **`netsh winsock reset`**: Reset Winsock catalog
- **`netsh int ip reset`**: Reset TCP/IP stack
- **`netsh winhttp reset proxy`**: Reset WinHTTP proxy
- **`ipconfig /flushdns`**: Flush DNS
- **`regsvr32 /s "DllName"`**: Register COM DLL
- **`powercfg /energy`**: Energy efficiency report


### 🕳 WSL Management
- **`wsl --list --verbose`**: List WSL instances
- **`wsl --list --running`**: Running WSL instances
- **`wsl --install`**: Install WSL default distro
- **`wsl --install -d Ubuntu`**: Install Ubuntu
- **`wsl --install -d Debian`**: Install Debian
- **`wsl --install -d kali-linux`**: Install Kali Linux
- **`wsl --status`**: WSL overall status
- **`wsl --update`**: Update WSL kernel
- **`wsl --shutdown`**: Shutdown all WSL instances
- **`wsl --terminate Ubuntu`**: Terminate specific distro
- **`wsl --set-version Ubuntu 2`**: Set WSL version
- **`wsl --set-default-version 2`**: Default WSL version
- **`wsl --set-default Ubuntu`**: Set default distro
- **`wsl --export Ubuntu backup.tar`**: Export WSL distro
- **`wsl --import MyDistro install-dir backup.tar`**: Import WSL distro
- **`wsl -d Ubuntu -u root`**: Run WSL as root
- **`wsl -d Ubuntu -- cd / && ls`**: Run command in WSL
- **`wsl --mount \\.\PHYSICALDRIVE0`**: Mount physical disk in WSL2
- **`wsl --unmount \\.\PHYSICALDRIVE0`**: Unmount WSL disk
- **`wsl --cd C:\folder`**: Start WSL in Windows folder


### 🔊 Audio & Sound
- **`wmic path Win32_SoundDevice get name,manufacturer,status`**: Audio device info
- **`wmic path Win32_SoundDevice get productname,driverversion`**: Audio driver details
- **`mmsys.cpl`**: Sound settings GUI
- **`mmsys.cpl,,1`**: Recording devices
- **`mmsys.cpl,,2`**: Sound playback devices
- **`mmsys.cpl,,3`**: Communications tab
- **`sndvol`**: Volume Mixer GUI
- **`sndvol /f`**: Volume Mixer (full view)
- **`sndrec32`**: Sound Recorder
- **`psr.exe`**: Problem Steps Recorder (audio notes)
- **`dxdiag /t dxdiag.txt`**: DxDiag test (includes audio)
- **`msdt.exe -id AudioPlaybackDiagnostic`**: Audio playback troubleshooter
- **`msdt.exe -id AudioRecordingDiagnostic`**: Audio recording troubleshooter
- **`reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\MMDevices\Audio\Render"`**: Audio render devices (registry)
- **`net start Audiosrv`**: Start Windows Audio service
- **`net stop Audiosrv && net start Audiosrv`**: Restart Windows Audio service


### 🌐 Internet & Browser
- **`control /name Microsoft.InternetOptions`**: Internet Options GUI
- **`inetcpl.cpl`**: Internet Properties GUI
- **`inetcpl.cpl,,4`**: Internet Properties Security tab
- **`rundll32.exe url.dll,FileProtocolHandler http://google.com`**: Open URL in default browser
- **`reg query "HKCU\Software\Microsoft\Internet Explorer\Main"`**: IE settings
- **`reg query "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings"`**: Proxy settings
- **`reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyEnable /t REG_DWORD /d 1 /f`**: Enable proxy
- **`reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Internet Settings" /v ProxyServer /f`**: Clear proxy server
- **`certutil -urlcache http://example.com/crl.crl`**: Fetch URL with certutil
- **`curl -I https://google.com`**: HTTP headers check
- **`bitsadmin /list`**: List BITS transfers
- **`start ms-settings:network-proxy`**: Open proxy settings (Win 10+)
- **`wsreset`**: Reset Microsoft Store cache
- **`ipconfig /flushdns`**: Flush DNS cache (browsers)


### ♿ Accessibility
- **`control /name Microsoft.EaseOfAccessCenter`**: Ease of Access Center GUI
- **`utilman`**: Utility Manager (accessibility)
- **`narrator`**: Start Narrator (screen reader)
- **`magnify`**: Start Magnifier
- **`osk`**: On-Screen Keyboard
- **`sapi.cpl`**: Speech Properties (TTS)
- **`control accessibility`**: Accessibility control panel
- **`reg query "HKCU\SOFTWARE\Microsoft\Windows NT\CurrentVersion\Accessibility"`**: Accessibility registry config
- **`reg add "HKCU\Control Panel\Accessibility\HighContrast" /v HighContrastOn /t REG_SZ /d 1 /f`**: Enable High Contrast


### 📡 Networking (Advanced)
- **`netsh trace start capture=yes tracefile=net.etl`**: Start network capture
- **`netsh trace stop`**: Stop network capture
- **`netsh trace convert input=net.etl output=net.cab`**: Convert trace to CAB
- **`netsh http show urlacl`**: Show URL ACLs
- **`netsh http add urlacl url=http://+:8080/ user=Everyone`**: Reserve URL
- **`netsh http delete urlacl url=http://+:8080/`**: Delete URL reservation
- **`netsh http show servicestate`**: HTTP service state
- **`netsh http show iplisten`**: Show IP listen list
- **`netsh winhttp show proxy`**: Show WinHTTP proxy
- **`netsh winhttp set proxy proxy-server=myproxy:8080`**: Set WinHTTP proxy
- **`netsh winhttp reset proxy`**: Reset WinHTTP proxy
- **`netsh advfirewall firewall show rule name=all verbose`**: All firewall rules
- **`netsh advfirewall firewall set rule group="remote desktop" new enable=Yes`**: Allow RDP in firewall
- **`nbtstat -c`**: NetBIOS cache
- **`nbtstat -R`**: Purge NetBIOS cache
- **`wmic path Win32_NetworkAdapterConfiguration where IPEnabled=True get *`**: Full adapter config
- **`wmic path Win32_NetworkConnection get *`**: Network connections


### ⚙️ Management & Admin Tools
- **`compmgmt.msc`**: Computer Management GUI
- **`compmgmt.msc -a`**: Computer Management (admin)
- **`services.msc`**: Services GUI
- **`taskschd.msc`**: Task Scheduler GUI
- **`eventvwr.msc`**: Event Viewer GUI
- **`perfmon.msc`**: Performance Monitor GUI
- **`gpedit.msc`**: Group Policy Editor (Pro/Enterprise)
- **`secpol.msc`**: Local Security Policy
- **`lusrmgr.msc`**: Local Users and Groups
- **`devmgmt.msc`**: Device Manager GUI
- **`diskmgmt.msc`**: Disk Management GUI
- **`dfrgui`**: Drive Optimizer (defrag GUI)
- **`certlm.msc`**: Local Machine Certificate Manager
- **`certmgr.msc`**: Current User Certificate Manager
- **`fsmgmt.msc`**: Shared Folders GUI
- **`tpm.msc`**: TPM Management GUI
- **`printmanagement.msc`**: Print Management GUI
- **`rsop.msc`**: Resultant Set of Policy
- **`azman.msc`**: Authorization Manager
- **`wf.msc`**: Windows Firewall with Advanced Security GUI
- **`control /name Microsoft.AdministrativeTools`**: Administrative Tools folder


## Q&A Examples
- **Q: How to find a file?**
  **A:** Use the `find_files` command by typing something like "find files mydoc" or "search for file mydoc" (which uses prefix/regex matching).
- **Q: How do I play a song?**
  **A:** Say "play <song name>" or "spotify play <song>". The prefix matching captures the song name and triggers the media playback action.
- **Q: How to shut down the PC?**
  **A:** Use an exact command like "shutdown pc" or "turn off computer".
- **Q: How to open a specific website?**
  **A:** Say "open site google.com" or "browse to <url>". The browser command parser handles prefix matching to launch the URL.