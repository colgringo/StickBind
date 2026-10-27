# StickBind

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
- Keys are sent as hardware scan codes, so games that read DirectInput, Raw Input or SDL see them
- Input is only sent while the game is the foreground window
- No GUI required

---

## How the DLL gets into the game

### A. `setdll` (for Wine/Proton)

Add the mapper to the game EXE import table once. After that the game loads the
DLL itself; no special launcher is needed.

1. Put the matching DLL and `StickBind.ini` next to the game EXE.
2. Run the matching architecture of `setdll`. Select the game EXE and the DLL and click `inject`. The game EXE will be patched.

3. Start the game normally.

To remove later, repeat steps 1 and 2. Instead of `inject`, the option will now be to `restore`.

Architecture of setdll, the DLL, and the game must match.

## Important note: I have just discovered that this method does not work on a real Windows system. At least not reliably. It seems to crash the whole EXE before booting. I have looked into the issue, but in the meantime if you are on a real Windows system and not Wine/Proton then use option B. (I have no running Windows system to further debug the issue, and have not used Windows since the 7 days, so unless someone else tracks down this issue it probably won't get fixed by me).

### B. Launcher injection (for Windows)

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
On Fedora. Not tested on other distros. Can also be built on Windows using MYS2

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

| Key                | Default | Meaning                                                              |
|--------------------|---------|----------------------------------------------------------------------|
| `Deadzone`         | `20`    | Stick deadzone % (0–90)                                              |
| `MouseSensitivity` | `10`    | Stick→mouse speed (1–100)                                            |
| `InvertY`          | `false` | Invert vertical mouse axis                                           |
| `TriggerThreshold` | `30`    | 1–250; LT/RT count as pressed at or above this                       |
| `ScanCodes`        | `true`  | Send keyboard input as scan codes (see below)                        |
| `RequireFocus`     | `true`  | Only send input while the game window is in the foreground (see below) |

Boolean values accept `true`, `1`, `yes` or `on` for true. Anything else is false.

**`ScanCodes`**

With `true`, keys are sent the way a physical keyboard sends them. This is what most games need, and games that ignore virtual-key-only input (many SDL, DirectInput and Raw Input titles) will only respond with this on. Set it to `false` to send plain virtual-key events instead, which is only worth trying if a specific game or program misbehaves with scan codes. `PAUSE` and `NUMLOCK` always use the virtual-key path either way.

**`RequireFocus`**

With `true`, nothing is sent while another window is in front, and any held keys are released when focus is lost. This stops the controller from typing into other windows after an alt-tab. The check passes only if the foreground window belongs to the same process as the DLL. If a game uses a launcher or wrapper that owns the visible window, set this to `false`, or no input will be sent. If input is completely dead in a game, try this setting first.

**Mouse speed**

Stick-to-mouse movement is scaled by elapsed time, so `MouseSensitivity` gives the same cursor speed regardless of system timer resolution.

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

A modifier button does not fire its own base binding while it is acting as a modifier. If you want it to send something in a layer, list it explicitly in that layer's section (see below). There is no tap-versus-hold behavior: a modifier button cannot also send a key when pressed alone.

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

The comma is the separator, so the comma key itself cannot be written as `,`. Use `COMMA` (for example `A=COMMA` or `A=SHIFT,COMMA`).

### Shared outputs

Several inputs can be bound to the same key or mouse button (for example `A=W` and `DUP=W`). The key stays held until every input using it has been released, so letting go of one does not cut off the other.

Mouse wheel bindings fire once per press, not continuously while held.

### Modifier layers

Only list **overrides**. Unlisted buttons keep the base binding.

    [Buttons_Mod1]
    X=R,F
    A=E

    [Buttons_Mod2]
    A=C

To disable a base binding inside a layer, set it to `NONE`:

    [Buttons_Mod1]
    B=NONE

### Sticks

    [LeftStick]
    Mode=Keys
    UP=W
    DOWN=S
    LEFT=A
    RIGHT=D

    [RightStick]
    Mode=Mouse

Optional per-modifier stick maps. A section is active if it contains `Mode=` or at least one direction:

    [LeftStick_Mod1]
    UP=I

    [RightStick_Mod1]
    Mode=Keys
    LEFT=Q
    RIGHT=E
    UP=NONE
    DOWN=NONE

Like the buttons, only list the directions you want to change. Unlisted directions keep the base stick binding, so the `LeftStick_Mod1` example above changes only UP and leaves `S`, `A` and `D` as they were. Use `NONE` to unbind a direction in that layer. If `Mode=` is left out, the layer keeps the base stick's mode.

While that modifier is held, the stick uses the layer map (e.g. mouse → keys). When released, it returns to the base stick map.

### Switching layers while inputs are held

If a button or stick direction is held when you press or release a modifier, and its binding is the same in both layers, it stays held with no release and re-press. Bindings that differ between the two layers are released and the new layer's binding takes over.

---

## Accepted key names

Case-insensitive.

**Disable:** `NONE` `OFF`

**Mouse:** `MOUSE1`/`LBUTTON`/`LMB`, `MOUSE2`/`RBUTTON`/`RMB`, `MOUSE3`/`MBUTTON`/`MMB`, `MOUSE4`/`XBUTTON1`, `MOUSE5`/`XBUTTON2`, `MOUSEWHEELUP` and `MOUSEWHEELDOWN`

**Letters/digits:** `A`–`Z`, `0`–`9`

**Punctuation (character or name):**

| Character | Names |
|-----------|--------|
| `[` `{` | `LBRACKET` `OPENBRACKET` |
| `]` `}` | `RBRACKET` `CLOSEBRACKET` |
| `\` `\|` | `BACKSLASH` |
| `;` `:` | `SEMICOLON` |
| `'` `"` | `QUOTE` |
| `,` `<` | `COMMA` (the character `,` itself cannot be used, see above) |
| `.` `>` | `PERIOD` `DOT` |
| `/` `?` | `SLASH` |
| `` ` `` `~` | `GRAVE` `TILDE` |
| `-` `_` | `MINUS` `DASH` |
| `=` `+` | `EQUALS` |

Punctuation names a **physical key**, not a typed character. The two characters in each row are the same key, and Shift is never added for you. `=` and `+` are identical, and so are `;` and `:`, `a` and `A`, and so on. To get the shifted result, add `SHIFT` yourself, for example `SHIFT,EQUALS`. The characters match the keycaps on a US keyboard layout. On other layouts, `;` `/` `` ` `` `[` `]` `\` and `'` can land on different physical keys.

**Named:** `SPACE` `ENTER`/`RETURN` `ESC`/`ESCAPE` `TAB` `SHIFT`/`LSHIFT` `RSHIFT` `CTRL`/`CONTROL`/`LCTRL` `RCTRL` `ALT`/`LALT` `RALT` `BACKSPACE`/`BKSP`/`BACK` `DELETE`/`DEL` `INSERT`/`INS` `HOME` `END` `PGUP`/`PAGEUP` `PGDN`/`PAGEDOWN` `UP` `DOWN` `LEFT` `RIGHT` `CAPSLOCK`/`CAPS` `NUMLOCK` `SCROLLLOCK`/`SCROLL` `PAUSE`/`BREAK` `PRINTSCREEN`/`PRTSC` `LWIN` `RWIN` `APPS`/`MENU` `F1`–`F12`

**Numpad:** `NUMPAD0`–`NUMPAD9` (or `NUM0`–`NUM9`), `NUMPAD*`/`MULTIPLY`, `NUMPAD+`/`ADD`, `NUMPAD-`/`SUBTRACT`, `NUMPAD.`/`DECIMAL`, `NUMPAD/`/`DIVIDE`

An unrecognized name is silently ignored and the binding does nothing, so check spelling if a mapping seems dead.

---

## How the mapper works

1. DLL starts a background thread on process attach.
2. Thread reads `StickBind.ini` beside the DLL.
3. Polls the first attached XInput controller that sends input (~250 Hz) and stays on that controller until it disconnects.
4. Emits `SendInput` key/mouse events on edges; relative mouse for stick→mouse.
5. Held inputs are released on disconnect, on focus loss (with `RequireFocus`), and when the DLL unloads.

---

## Troubleshooting and limitations

If a game does not respond, work through these in order:

1. Make sure your controller is actually an XInput controller. If it is not StickBind will see nothing.
2. Set `RequireFocus=false`, in case a launcher or wrapper owns the foreground window.
3. Set `ScanCodes=false`, in case the game handles scan codes badly.
4. Run the game and any launcher at the same privilege level. Windows blocks input injected into an elevated window from a non-elevated process.
5. Confirm the DLL, `setdll` and the game are all the same architecture (32 or 64-bit).

Known limitations:

- Games with anti-cheat may block or flag injected input and DLLs. Do not use this in online games with anti-cheat.
- Some games discard input generated by `SendInput`. There is no workaround short of a virtual input driver.
- If a game also reads the controller natively, it receives both the real controller input and the mapped keyboard/mouse input. Turn off the game's own controller support if prompts or controls conflict.
- Only XInput controllers are supported. If your controller does not work with XInput you can work around this limitation with a wrapper like X360CE.

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
