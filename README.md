# NM-CYD-C5 Spotify mini-player

This repo currently contains a **firmware starter and cloud build workflow**, not a finished Spotify client.

## Build a firmware file
1. Open the **Actions** tab.
2. Select **Build ESP32-C5 firmware**.
3. Click **Run workflow** (or wait for the push-triggered build).
4. Open the completed run and download the `nm-cyd-c5-firmware` artifact.
5. Extract `firmware.bin` and flash it with an ESP32-C5-compatible flashing tool.

## First boot
The board creates a Wi-Fi access point named `CYD-Spotify-Setup`. Connect to it and open `http://192.168.4.1`.

## Not implemented yet
- Spotify account authorization / OAuth
- Live track metadata and album-art download
- Working Spotify play/pause/previous/next controls

These features require an authenticated Spotify API integration or companion service. Pin assignments and touch calibration also need validation against the exact board revision.
