# SARA Q&A Examples

This document provides a massive list of commands supported by SARA, sorted by priority and usefulness.

## Priority 1: System Control & Power Commands
Q: How to lock the PC?
A: Use the lock_pc command by typing something like "lock my pc" (which uses regex matching).

Q: How to put the PC to sleep?
A: Use the run_cmd command by typing something like "put pc to sleep" (which uses regex matching).

Q: How to abort a shutdown or restart?
A: Use the run_cmd command by typing something like "cancel shutdown" (which uses regex matching).

Q: How to shutdown the PC immediately?
A: Use the run_cmd command by typing something like "shutdown pc now" (which uses regex matching).

Q: How to shutdown the PC with a delay?
A: Use the run_cmd command by typing something like "turn off laptop" (which uses regex matching).

Q: How to restart the PC immediately?
A: Use the run_cmd command by typing something like "restart pc now" (which uses regex matching).

Q: How to restart the PC with a delay?
A: Use the run_cmd command by typing something like "reboot computer" (which uses regex matching).

Q: How to log off the current user?
A: Use the run_cmd command by typing something like "sign out" (which uses regex matching).

Q: How to restart in safe mode?
A: Use the run_cmd command by typing something like "safe mode restart" (which uses regex matching).

Q: How to turn on Wi-Fi?
A: Use the run_cmd command by typing something like "turn on wifi" (which uses regex matching).

Q: How to turn off Wi-Fi?
A: Use the run_cmd command by typing something like "turn off wi-fi" (which uses regex matching).

Q: How to turn on Bluetooth?
A: Use the run_cmd command by typing something like "turn on bluetooth" (which uses regex matching).

Q: How to turn off Bluetooth?
A: Use the run_cmd command by typing something like "turn off bluetooth" (which uses regex matching).

Q: How to check battery status?
A: Use the get_system_stats command by typing something like "battery status" (which uses regex matching).

Q: How to check CPU usage?
A: Use the get_system_stats command by typing something like "cpu usage" (which uses regex matching).

Q: How to check RAM usage?
A: Use the get_system_stats command by typing something like "ram usage" (which uses regex matching).

Q: How to view overall system info?
A: Use the get_system_stats command by typing something like "system specs" (which uses regex matching).

Q: How to clean system memory?
A: Use the clean_memory command by typing something like "clean pc memory" (which uses regex matching).

Q: How to adjust screen brightness?
A: Use the set_brightness_level command by typing something like "set brightness to 50" (which uses regex matching).

Q: How to turn off the display?
A: Use the run_cmd command by typing something like "turn off screen" (which uses regex matching).


## Priority 2: File Management
Q: How to create a new folder?
A: Use the run_cmd command by typing something like "create folder" (which uses exact matching).

Q: How to clear temporary files?
A: Use the run_cmd command by typing something like "clear temp files" (which uses exact matching).

Q: How to empty the recycle bin?
A: Use the run_cmd command by typing something like "empty recycle bin" (which uses exact matching).

Q: How to clean the disk?
A: Use the run_cmd command by typing something like "disk cleanup" (which uses exact matching).

Q: How to list files in a directory?
A: Use the run_cmd command by typing something like "list files" (which uses exact matching).

Q: How to open the downloads folder?
A: Use the run_cmd command by typing something like "open downloads folder" (which uses exact matching).

Q: How to open the documents folder?
A: Use the run_cmd command by typing something like "open documents" (which uses exact matching).

Q: How to open the desktop folder?
A: Use the run_cmd command by typing something like "go to desktop" (which uses exact matching).

Q: How to take ownership of a file?
A: Use the run_cmd command by typing something like "take ownership" (which uses exact matching).

Q: How to check file integrity?
A: Use the run_cmd command by typing something like "check disk" (which uses exact matching).

Q: How to defrag the hard drive?
A: Use the run_cmd command by typing something like "defrag drive" (which uses exact matching).

Q: How to compress a folder into a ZIP?
A: Use the run_cmd command by typing something like "compress folder" (which uses exact matching).

Q: How to extract a ZIP archive?
A: Use the run_cmd command by typing something like "extract zip" (which uses exact matching).


## Priority 3: Media & Playback Controls
Q: How to play a specific song?
A: Use the play_music command by typing something like "play song " (which uses prefix matching).

Q: How to pause the music?
A: Use the media_command command by typing something like "pause song" (which uses regex matching).

Q: How to skip to the next song?
A: Use the media_command command by typing something like "next track" (which uses regex matching).

Q: How to go back to the previous song?
A: Use the media_command command by typing something like "previous song" (which uses regex matching).

Q: How to resume a playlist?
A: Use the play_last_playlist command by typing something like "play previous playlist" (which uses regex matching).

Q: How to resume music playback?
A: Use the media_command command by typing something like "continue playing" (which uses regex matching).

Q: How to toggle play or pause?
A: Use the media_command command by typing something like "toggle play" (which uses regex matching).

Q: How to turn on shuffle?
A: Use the media_command command by typing something like "shuffle on" (which uses regex matching).

Q: How to turn off shuffle?
A: Use the media_command command by typing something like "shuffle off" (which uses regex matching).

Q: How to repeat the entire playlist?
A: Use the media_command command by typing something like "repeat playlist" (which uses regex matching).

Q: How to repeat the current song?
A: Use the media_command command by typing something like "repeat this song" (which uses regex matching).

Q: How to disable repeat?
A: Use the media_command command by typing something like "repeat off" (which uses regex matching).

Q: How to like the current song?
A: Use the media_command command by typing something like "like this song" (which uses regex matching).

Q: How to unlike a song?
A: Use the media_command command by typing something like "unlike song" (which uses regex matching).

Q: How to identify what song is currently playing?
A: Use the media_command command by typing something like "what song is playing" (which uses regex matching).

Q: How to add a song to the queue?
A: Use the media_command command by typing something like "add to queue" (which uses regex matching).

Q: How to increase the volume?
A: Use the volume_set command by typing something like "volume up" (which uses regex matching).

Q: How to decrease the volume?
A: Use the volume_set command by typing something like "volume down" (which uses regex matching).

Q: How to maximize the volume?
A: Use the volume_set command by typing something like "max volume" (which uses regex matching).

Q: How to mute the sound?
A: Use the volume_mute command by typing something like "mute sound" (which uses regex matching).

Q: How to unmute the sound?
A: Use the volume_mute command by typing something like "unmute audio" (which uses regex matching).

Q: How to set a specific volume level?
A: Use the set_volume_level command by typing something like "set volume to 50" (which uses regex matching).

Q: How to check the current volume?
A: Use the volume_get command by typing something like "check volume" (which uses regex matching).


## Priority 4: Search & Browser
Q: How to search Google?
A: Use the search_plugin command by typing something like "search google" (which uses exact matching).

Q: How to search for a specific term?
A: Use the search_plugin command by typing something like "search for " (which uses prefix matching).

Q: How to perform a deep web search?
A: Use the search_plugin_scrape command by typing something like "deep search" (which uses exact matching).

Q: How to deep search for a specific term?
A: Use the search_plugin_scrape command by typing something like "deep search for " (which uses prefix matching).

Q: How to open YouTube?
A: Use the open_url command by typing something like "open youtube" (which uses exact matching).

Q: How to open GitHub?
A: Use the open_url command by typing something like "open github" (which uses exact matching).

Q: How to open Stack Overflow?
A: Use the open_url command by typing something like "open stackoverflow" (which uses exact matching).

Q: How to open Reddit?
A: Use the open_url command by typing something like "open reddit" (which uses exact matching).

Q: How to open Gmail?
A: Use the open_url command by typing something like "open gmail" (which uses exact matching).

Q: How to open Amazon?
A: Use the open_url command by typing something like "open amazon" (which uses exact matching).

Q: How to open Google Drive?
A: Use the open_url command by typing something like "open google drive" (which uses exact matching).

Q: How to open any specific website?
A: Use the open_url command by typing something like "go to website http..." (which uses regex matching).


## Priority 5: Window Management & Focus
Q: How to focus a specific app window?
A: Use the focus_app command by typing something like "focus window " (which uses prefix matching).

Q: How to close a specific app window?
A: Use the close_app command by typing something like "close app " (which uses prefix matching).

Q: How to switch windows (Alt+Tab)?
A: Use the keyboard_shortcut command by typing something like "switch window" (which uses exact matching).

Q: How to close the current active window?
A: Use the keyboard_shortcut command by typing something like "close current window" (which uses exact matching).

Q: How to show the desktop?
A: Use the keyboard_shortcut command by typing something like "show desktop" (which uses exact matching).

Q: How to open Task View?
A: Use the keyboard_shortcut command by typing something like "task view" (which uses exact matching).

Q: How to open the Action Center?
A: Use the keyboard_shortcut command by typing something like "open action center" (which uses exact matching).

Q: How to open Windows Search?
A: Use the keyboard_shortcut command by typing something like "open search" (which uses exact matching).

Q: How to open the Run dialog?
A: Use the keyboard_shortcut command by typing something like "open run dialog" (which uses exact matching).

Q: How to lock the Windows screen?
A: Use the keyboard_shortcut command by typing something like "lock screen" (which uses exact matching).

Q: How to open the Emoji panel?
A: Use the keyboard_shortcut command by typing something like "open emoji panel" (which uses exact matching).

Q: How to open Clipboard History?
A: Use the keyboard_shortcut command by typing something like "open clipboard history" (which uses exact matching).

Q: How to open the Snipping Tool?
A: Use the keyboard_shortcut command by typing something like "open snipping tool shortcut" (which uses exact matching).

Q: How to minimize the current window?
A: Use the keyboard_shortcut command by typing something like "minimize window" (which uses exact matching).

Q: How to maximize the current window?
A: Use the keyboard_shortcut command by typing something like "maximize window" (which uses exact matching).

Q: How to snap a window to the left?
A: Use the keyboard_shortcut command by typing something like "snap left" (which uses exact matching).

Q: How to snap a window to the right?
A: Use the keyboard_shortcut command by typing something like "snap right" (which uses exact matching).

Q: How to create a new virtual desktop?
A: Use the keyboard_shortcut command by typing something like "new virtual desktop" (which uses exact matching).

Q: How to switch to the next virtual desktop?
A: Use the keyboard_shortcut command by typing something like "switch virtual desktop" (which uses exact matching).

Q: How to close the current virtual desktop?
A: Use the keyboard_shortcut command by typing something like "close virtual desktop" (which uses exact matching).


## Priority 6: Typing & Clipboard
Q: How to type out specific text?
A: Use the type_text command by typing something like "type this " (which uses prefix matching).

Q: How to copy text to the clipboard?
A: Use the clipboard_copy command by typing something like "copy to clipboard " (which uses prefix matching).

Q: How to read the clipboard contents?
A: Use the clipboard_read command by typing something like "read clipboard" (which uses exact matching).

Q: How to clear the clipboard?
A: Use the run_cmd command by typing something like "clear clipboard" (which uses exact matching).


## Priority 7: Utilities & System Queries
Q: How to take a screenshot of the desktop?
A: Use the take_screenshot command by typing something like "take screenshot" (which uses regex matching).

Q: How to capture a photo from the webcam?
A: Use the capture_camera command by typing something like "take photo" (which uses regex matching).

Q: How to view recent files?
A: Use the run_cmd command by typing something like "show recent files" (which uses regex matching).

Q: How to view startup programs?
A: Use the run_cmd command by typing something like "show startup apps" (which uses regex matching).

Q: How to check environment variables?
A: Use the run_cmd command by typing something like "show environment variables" (which uses regex matching).

Q: How to create a system restore point?
A: Use the run_cmd command by typing something like "system restore" (which uses regex matching).

Q: How to check the Windows version?
A: Use the run_cmd command by typing something like "check windows version" (which uses regex matching).

Q: How to run the DirectX diagnostic tool?
A: Use the run_cmd command by typing something like "run dxdiag" (which uses regex matching).

Q: How to open a command prompt as administrator?
A: Use the run_cmd command by typing something like "run command as admin" (which uses regex matching).

Q: How to ask for the current time?
A: Use the run_cmd command by typing something like "what time is it" (which uses regex matching).

Q: How to ask for the current date?
A: Use the run_cmd command by typing something like "what date" (which uses regex matching).

Q: How to ask for the day of the week?
A: Use the run_cmd command by typing something like "what day is it" (which uses regex matching).

Q: How to list installed programs?
A: Use the run_cmd command by typing something like "list installed programs" (which uses regex matching).

Q: How to clear the command history?
A: Use the run_cmd command by typing something like "clear command history" (which uses regex matching).

Q: How to find the current logged-in username?
A: Use the run_cmd command by typing something like "what is my username" (which uses regex matching).

Q: How to list all user accounts?
A: Use the run_cmd command by typing something like "list all users" (which uses regex matching).

Q: How to reveal the Wi-Fi password for the current network?
A: Use the run_cmd command by typing something like "show wifi password" (which uses regex matching).

Q: How to retrieve a saved Wi-Fi password for a specific network?
A: Use the get_wifi_password command by typing something like "what is my wifi password for " (which uses regex matching).

Q: How to generate a secure random password?
A: Use the run_cmd command by typing something like "generate secure password" (which uses regex matching).

Q: How to generate a new UUID/GUID?
A: Use the run_cmd command by typing something like "generate uuid" (which uses regex matching).

Q: How to learn who created SARA?
A: Use the run_cmd command by typing something like "who created you" (which uses regex matching).

Q: How to view SARA's capabilities and help menu?
A: Use the show_help command by typing something like "what can you do" (which uses regex matching).


## Priority 8: Networking
Q: How to find your IP address?
A: Use the get_ip_address command by typing something like "my ip" (which uses contains matching).

Q: How to check network connection status?
A: Use the run_cmd command by typing something like "network status" (which uses contains matching).

Q: How to check disk usage and free space?
A: Use the run_cmd command by typing something like "disk usage" (which uses contains matching).

Q: How to check the system uptime?
A: Use the run_cmd command by typing something like "system uptime" (which uses contains matching).

Q: How to test the internet connection speed/ping?
A: Use the run_cmd command by typing something like "test internet connection" (which uses regex matching).

Q: How to find the computer's hostname?
A: Use the run_cmd command by typing something like "what is my hostname" (which uses regex matching).

Q: How to find the network MAC address?
A: Use the run_cmd command by typing something like "show my mac address" (which uses regex matching).

Q: How to flush the DNS cache?
A: Use the run_cmd command by typing something like "flush dns cache" (which uses regex matching).

Q: How to renew the IP address?
A: Use the run_cmd command by typing something like "renew ip address" (which uses regex matching).

Q: How to list available Wi-Fi networks?
A: Use the run_cmd command by typing something like "scan wifi networks" (which uses regex matching).

Q: How to show active network connections?
A: Use the run_cmd command by typing something like "active connections" (which uses contains matching).


## Priority 9: Processes & Services
Q: How to list all running processes?
A: Use the run_cmd command by typing something like "list processes" (which uses regex matching).

Q: How to kill or terminate a process?
A: Use the task_kill command by typing something like "kill process " (which uses regex matching).

Q: How to get the total number of running processes?
A: Use the run_cmd command by typing something like "process count" (which uses regex matching).

Q: How to find the highest CPU/memory consuming process?
A: Use the run_cmd command by typing something like "highest cpu process" (which uses regex matching).

Q: How to check the status of Windows services?
A: Use the run_cmd command by typing something like "services status" (which uses regex matching).

Q: How to start, stop, or restart a service?
A: Use the run_cmd command by typing something like "start service " (which uses prefix matching).


## Priority 10: News & Notifications (Least Common)
Q: How to get the latest news in India?
A: Use the news_plugin command by typing something like "latest news india" (which uses regex matching).

Q: How to get global/world news?
A: Use the news_plugin command by typing something like "world news" (which uses regex matching).

Q: How to get technology news?
A: Use the news_plugin command by typing something like "tech news" (which uses regex matching).

Q: How to get business news?
A: Use the news_plugin command by typing something like "finance news" (which uses regex matching).

Q: How to get sports news?
A: Use the news_plugin command by typing something like "sports news" (which uses regex matching).

Q: How to get entertainment news?
A: Use the news_plugin command by typing something like "movies news" (which uses regex matching).

Q: How to get general daily headlines?
A: Use the news_plugin command by typing something like "today's news" (which uses regex matching).

Q: How to send a custom notification to the screen?
A: Use the notify_text command by typing something like "notify me " (which uses prefix matching).

Q: How to trigger a basic test notification?
A: Use the notify command by typing something like "send notification" (which uses exact matching).

Q: How to set a quick system reminder?
A: Use the run_cmd command by typing something like "remind me" (which uses exact matching).

Q: How to set a custom reminder text?
A: Use the notify_text command by typing something like "remind me to " (which uses prefix matching).
