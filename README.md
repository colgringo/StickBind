# Stick Bind

A simple DLL that maps an XInput controller to keyboard and mouse for Windows games which have no controller support. Useful for when a very simple and lightweight solution is desirable over a more full-featured solution such as AntiMicroX. Similar in concept to GtoMnk, but with more of a focus on simplicity and ease of use with everything configured from the INI file.

A mapper DLL is loaded into the game process, which then polls the controller and emits
the keys / mouse events configured in a plain INI file.

**Status:** Feature-complete for the intended use case. Bugs and extra
features will be handled as they come up.

---

## Features

- All standard Xbox 360 digital buttons (face, shoulders, Start/Back, stick clicks, D-Pad)
- Analog triggers as digital buttons (configurable threshold)
- Relative mouse movement with circular deadzone, response curve, and sensitivity
- Mouse buttons mappable from any controller input
- Multi-key bindings. One button can send several keys at once
- Two modifier layers. Hold Mod1 or Mod2 to switch to alternate button/stick maps
- Full keyboard coverage: letters, digits, punctuation, modifiers, arrows, F-keys, numpad
- No GUI required

---

## How the DLL gets into the game

### A. Recommended — `setdll`

Add the mapper to the game EXE import table once. After that the game loads the
DLL itself; no special launcher is needed.

1. Put the matching DLL and `StickBind.ini` next to the game EXE.
2. Run the matching architecture of `setdll`. Select the game EXE and the DLL and click `inject`. The game EXE will be patched.

3. Start the game normally.

To remove later, repeat steps 1 and 2. Instead of `inject`, the option will now be to `restore`.

Architecture of setdll, the DLL, and the game must match.

### B. Optional fallback — launcher injection

Use when you do not want to modify the game EXE.

1. Put matching `StickBindLauncher32/64.exe`, DLL, and `StickBind.ini` together beside the game EXE.
2. Set `[Launcher] GAME=` (and optional `ARGS=`).
3. Run the launcher instead of the game EXE.

---

## Requirements

**Build**

- MinGW cross-compilers
  - 32-bit: `i686-w64-mingw32-gcc` (Fedora: `mingw32-gcc`)
  - 64-bit: `x86_64-w64-mingw32-gcc` (Fedora: `mingw64-gcc`)

**Run**

- Windows game
- XInput-compatible controller
- For option A: patched game EXE
- For option B: matching `StickBindLauncher32/64.exe`

---

## Build
On Fedora. Not tested on other distros

    sudo dnf install mingw32-gcc mingw64-gcc
    ./build.sh

| Arch   | Launcher (optional)         | DLL          |
|--------|------------------------     |--------------|
| 32-bit | `StickBindLauncher32.exe`   | `SB32.dll`   |
| 64-bit | `StickBindLauncher64.exe`   | `SB64.dll`   |

---

## Configuration (`StickBind.ini`)

Place next to the DLL.

### `[Launcher]`

Only used by the optional launcher:

    GAME=Game.exe
    ARGS=

Not required when using a patched EXE. Relative paths resolve from the launcher directory.

### `[Controller]`

| Key                | Default | Meaning                                         |
|--------------------|---------|-------------------------------------------------|
| `Deadzone`         | `20`    | Stick deadzone % (0–90)                         |
| `MouseSensitivity` | `10`    | Stick→mouse speed (1–100)                       |
| `InvertY`          | `false` | Invert vertical mouse axis                      |
| `TriggerThreshold` | `30`    | 0–255; LT/RT count as pressed at or above this  |

### `[Modifiers]`

Up to **two** modifier buttons (any button name, or `NONE`):

    [Modifiers]
    Mod1=LB
    Mod2=RB

| Held        | Layer used                          |
|-------------|-------------------------------------|
| neither     | Base (`[Buttons]`, sticks)          |
| Mod1 only   | `Buttons_Mod1`, `*Stick_Mod1`       |
| Mod2 only   | `Buttons_Mod2`, `*Stick_Mod2`       |
| both        | Mod1 wins                           |

### `[Buttons]` (base layer)

| Key in ini                     | Controller input   |
|--------------------------------|--------------------|
| `A` `B` `X` `Y`                | Face               |
| `LB` `RB`                      | Shoulders          |
| `START` `BACK`                 | Menu               |
| `LS` `RS`                      | Stick clicks       |
| `DUP` `DDOWN` `DLEFT` `DRIGHT` | D-Pad              |
| `LT` `RT`                      | Triggers (digital) |

Each value is a key, mouse button, comma multi-key list, or `NONE`.

    [Buttons]
    A=SPACE
    X=R,F
    RT=MOUSE1
    LB=NONE

### Multi-key (comma)

Works on **base and mod layers**:

    X=R,F
    LB=SHIFT,A

Up to 8 codes per binding. Press sends all downs; release sends ups in reverse order.

### Modifier layers

Only list **overrides**. Unlisted buttons keep the base binding.

    [Buttons_Mod1]
    X=R,F
    A=E

    [Buttons_Mod2]
    A=C

### Sticks

    [LeftStick]
    Mode=Keys
    UP=W
    DOWN=S
    LEFT=A
    RIGHT=D

    [RightStick]
    Mode=Mouse

Optional per-modifier stick maps (include `Mode=` to activate):

    [RightStick_Mod1]
    Mode=Keys
    LEFT=Q
    RIGHT=E
    UP=NONE
    DOWN=NONE

While that modifier is held, the stick uses this map (e.g. mouse → keys).
When released, it returns to the base stick map.

---

## Accepted key names

Case-insensitive.

**Disable:** `NONE` `OFF`

**Mouse:** `MOUSE1`/`LBUTTON`/`LMB`, `MOUSE2`/`RBUTTON`/`RMB`, `MOUSE3`/`MBUTTON`/`MMB`, `MOUSE4`/`XBUTTON1`, `MOUSE5`/`XBUTTON2`, `MOUSEWHEELUP` and `MOUSEWHEELDOWN`

**Letters/digits:** `A`–`Z`, `0`–`9`

**Punctuation (character or name):**

| Character | Names |
|-----------|--------|
| `[` `{` | `LBRACKET` `LBRACE` `OPENBRACKET` |
| `]` `}` | `RBRACKET` `RBRACE` `CLOSEBRACKET` |
| `\` `\|` | `BACKSLASH` `PIPE` |
| `;` `:` | `SEMICOLON` `COLON` |
| `'` `"` | `QUOTE` `APOSTROPHE` |
| `,` `<` | `COMMA` |
| `.` `>` | `PERIOD` `DOT` |
| `/` `?` | `SLASH` `QUESTION` |
| `` ` `` `~` | `GRAVE` `TILDE` `BACKTICK` |
| `-` `_` | `MINUS` `DASH` `UNDERSCORE` |
| `=` `+` | `EQUALS` `PLUS` |

**Named:** `SPACE` `ENTER`/`RETURN` `ESC`/`ESCAPE` `TAB` `SHIFT`/`LSHIFT` `RSHIFT` `CTRL`/`CONTROL`/`LCTRL` `RCTRL` `ALT`/`LALT` `RALT` `BACKSPACE`/`BKSP` `DELETE`/`DEL` `INSERT`/`INS` `HOME` `END` `PGUP`/`PAGEUP` `PGDN`/`PAGEDOWN` `UP` `DOWN` `LEFT` `RIGHT` `CAPSLOCK` `NUMLOCK` `SCROLLLOCK` `PAUSE` `PRINTSCREEN`/`PRTSC` `LWIN` `RWIN` `APPS`/`MENU` `F1`–`F12`

**Numpad:** `NUMPAD0`–`NUMPAD9` (or `NUM0`–`NUM9`), `NUMPAD*` `NUMPAD+` `NUMPAD-` `NUMPAD.` `NUMPAD/`

---

## How the mapper works

1. DLL starts a background thread on process attach.
2. Thread reads `StickBind.ini` beside the DLL.
3. Polls the first attached XInput controller that sends input (~250 Hz).
4. Emits `SendInput` key/mouse events on edges; relative mouse for stick→mouse.

---

## License

StickBind is licensed under the **GNU General Public License v3.0**.
See the [LICENSE](LICENSE) file for the full text.

### Third-party component: setdll

The `setdll-x86.exe` and `setdll-x64.exe` binaries are provided for user convenience.

- Original project: [https://github.com/Bush2021/setdll](https://github.com/Bush2021/setdll)
- License: GNU General Public License v3.0
- Corresponding source code for the exact binaries shipped in this release is included in the release files at:

      src/setdll_src.zip

These binaries are distributed under the principle of aggregation and are not part of the StickBind program itself.
