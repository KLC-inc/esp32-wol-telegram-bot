# ESP32 Wake-on-LAN Telegram Bot (PRO + Sleep Edition)

This project allows you to remotely turn your PC on and off via a Telegram Bot using an ESP32 microcontroller.

## Required Libraries
To use this project, you will need to install the following dependencies:
**In Arduino IDE (for ESP32):**
- `ArduinoJson` (v6 or v7) — to handle Telegram API requests.
- `WiFiManager` (by tzapu) — for convenient Wi-Fi configuration without hardcoding credentials.

**Built-in ESP32 libraries (no installation needed, they come with the core):**
- `WiFi`, `WiFiClientSecure`, `HTTPClient`, `WiFiUDP`, `esp_task_wdt`.

**On the PC (for the Python script):**
- `psutil` — to automatically detect network interfaces and MAC addresses.

---

## ESP32 Firmware Setup

Before flashing the ESP32, open the `esp32_wol_bot_en.ino` file and fill in the `SETTINGS` block.

### 1. `BOT_TOKEN`
Your Telegram bot token. 
- **Where to get it:** Go to Telegram, find `@BotFather`, send the `/newbot` command, and choose a name. It will give you a long string (the token). 
- **Example:** `"123456789:ABCDefghIJKLmnopQRSTuvwxyz"`

### 2. `ALLOWED_CHAT_ID`
Your personal Telegram ID. This is a security measure so no one else can control your PC.
- **Where to get it:** Send a message to `@getmyid_bot` or `@userinfobot` in Telegram. It will reply with your numeric ID.
- **Example:** `"1234567890"`

### 3. `TARGET_MAC`
The MAC address (physical address) of your computer's network adapter.
- **Where to get it (Windows):** Open the Command Prompt (`Win + R`, type `cmd`) and type `ipconfig /all`. Find your adapter (e.g., "Ethernet adapter") and look for the **"Physical Address"** line (e.g., `A8-A1-59-ED-73-21`).
- **How to format it:** Each pair of characters must be prefixed with `0x`. 
- **Example:** If your address is `A8-A1-59-ED-73-21`, the code should look like this: `{ 0xA8, 0xA1, 0x59, 0xED, 0x73, 0x21 };`

---

## Wi-Fi Connection and Disconnection (WiFiManager)

This project uses `WiFiManager`, so **you do not need to hardcode your Wi-Fi network name and password into the code!**

**How to connect the board to Wi-Fi:**
1. After flashing, turn on the ESP32.
2. The board will try to find the previously saved network. If it fails, it will automatically create an open Wi-Fi access point named **`ESP32-WOL-Setup`**.
3. Connect to this network using your phone or laptop.
4. A settings window should pop up automatically (if not, go to `192.168.4.1` in your browser).
5. Click **"Configure WiFi"**, select your home Wi-Fi from the list, enter the password, and click **Save**.
6. The ESP32 will reboot and connect to your router.

**How to reset or change the Wi-Fi network:**
If you change your router or password, the ESP32 simply won't be able to connect. After a few seconds, it will create the `ESP32-WOL-Setup` network again. 
*(Note: The access point has a 3-minute timeout. If no one connects within 3 minutes, the board reboots and tries again).* 
If you need to force the board to reconfigure to a different network while the old router is still running — simply **unplug the old router temporarily**.

---

## PC Setup (Python Script)

For the `/sleep` command to work, the `wol_sleep_listener_en.pyw` Python script must be running on your computer.

**Important note about network adapters:**
In older versions of the script, you had to manually specify the network adapter name (the default was `'Ethernet'`). 
However, **the current version of the script is Zero-Config (requires no setup)**. It uses the `psutil` library to automatically scan your system, find *all* available network adapters and their MAC addresses, and listens for incoming commands universally on `0.0.0.0`. 
You **do not** need to change anything in the script when switching your connection method (e.g., from Ethernet cable to Wi-Fi).

**How to run it:**
1. Install Python and the required library: `pip install psutil`
2. Double-click `wol_sleep_listener_en.pyw`. No window will appear; the script will run silently in the background.
3. To start it automatically when you turn on your PC, place a shortcut to this file in your Windows Startup folder (`Win + R` -> type `shell:startup`).

---

## Bot Commands
- `/wake` — Turn on the PC (sends a Magic Packet).
- `/sleep` — Turn off the PC (requires the Python script to be running).
- `/status` — Check the ESP32 board status (uptime, signal strength, free RAM).
- `/reboot` — Reboot the ESP32 microcontroller.
