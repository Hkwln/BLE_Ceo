# LSP Configuration für ESP32 BLE Headphones

## Problem
LSP (Clangd) zeigt Fehler bei Includes wie:
- `#include <BluetoothA2DPSink.h>` nicht gefunden
- `#include <driver/i2s.h>` nicht gefunden
- `#include <USBCDC.h>` nicht gefunden

## Lösung

### 1. **platformio.ini aktualisieren** ✅
Die `platformio.ini` wurde aktualisiert mit:
- Korrekter Library URL (pschatzmann/ESP32-A2DP)
- Build flags mit include paths
- Adafruit MAX98357A Library mit Version

### 2. **.clangd Konfiguration** ✅
Neue `.clangd` Datei mit:
- Compile flags
- Framework include paths
- Arduino ESP32 variant paths
- C++17 Standard

### 3. **.vscode/settings.json** ✅
VSCode/Neovim Integration mit:
- Clangd enabled
- Header insertion (iwyu)
- Exclude patterns für `.pio`

### 4. **compile_commands.json** ✅
Generiert automatisch mit `generate_lsp_config.sh`

## Verwendung

### Für Neovim (LSP):

```vim
" In init.vim oder init.lua:
lua require'lspconfig'.clangd.setup{
  root_dir = require'lspconfig'.util.root_pattern(".clangd", ".git"),
  cmd = {"clangd", "--header-insertion=iwyu"},
}
```

### Für VSCode:

1. Install "clangd" extension
2. Open workspace
3. LSP sollte automatisch aktivieren

### Compile Commands Regenerieren:

Wenn neue Libraries hinzugefügt werden:
```bash
cd esp32
./generate_lsp_config.sh
```

## Includes sollten jetzt funktionieren:

```cpp
#include <Arduino.h>           ✅
#include <BluetoothA2DPSink.h> ✅
#include <driver/i2s.h>        ✅
#include <USB.h>               ✅
#include <USBCDC.h>            ✅
```

## Falls noch Probleme:

1. **Clangd neustarten:**
```bash
# Neovim
:LspRestart clangd

# oder in Terminal
pkill clangd
```

2. **Pfade überprüfen:**
```bash
ls -la ~/.platformio/packages/framework-arduinoespressif32/
```

3. **Library installiert?**
```bash
ls -la esp32/.pio/libdeps/seeeduino_xiao_esp32s3/
```

## Files geändert/erstellt:

- ✅ `platformio.ini` - Updated mit korrekten libs
- ✅ `.clangd` - LSP Konfiguration
- ✅ `.vscode/settings.json` - IDE Einstellungen
- ✅ `generate_lsp_config.sh` - Helper Script
- ✅ `compile_commands.json` - Clangd Index

---

**Stand:** 2026-04-26  
**Status:** ✅ LSP sollte jetzt alle Includes korrekt erkennen
