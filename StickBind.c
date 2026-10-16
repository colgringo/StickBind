#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <wchar.h>
#include <math.h>

#define MAX_CODES 8

static BOOL g_use_scancodes = TRUE;
static BOOL g_require_focus = TRUE;
static float g_dt_scale = 1.0f;

static BOOL is_extended(WORD vk) {
    switch (vk) {
        case VK_RCONTROL: case VK_RMENU: case VK_INSERT: case VK_DELETE:
        case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
        case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
        case VK_NUMLOCK: case VK_DIVIDE: case VK_SNAPSHOT:
        case VK_LWIN: case VK_RWIN: case VK_APPS:
            return TRUE;
        default:
            return FALSE;
    }
}

static void send_key(WORD vk, BOOL down) {
    if (!vk) return;
    INPUT in; ZeroMemory(&in, sizeof(in));
    in.type = INPUT_KEYBOARD;

    WORD scan = (WORD)MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    BOOL use_scan = g_use_scancodes && scan && vk != VK_PAUSE && vk != VK_NUMLOCK;

    if (use_scan) {
        in.ki.wScan = scan;
        in.ki.dwFlags = KEYEVENTF_SCANCODE;
    } else {
        in.ki.wVk = vk;
    }
    if (is_extended(vk)) in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    if (!down) in.ki.dwFlags |= KEYEVENTF_KEYUP;
    SendInput(1, &in, sizeof(in));
}

static void send_mouse_button(int button, BOOL down) {
    DWORD flags = 0, data = 0;
    switch (button) {
        case 0: flags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP; break;
        case 1: flags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP; break;
        case 2: flags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP; break;
        case 3: flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; data = XBUTTON1; break;
        case 4: flags = down ? MOUSEEVENTF_XDOWN : MOUSEEVENTF_XUP; data = XBUTTON2; break;
        default: return;
    }
    INPUT in; ZeroMemory(&in, sizeof(in));
    in.type = INPUT_MOUSE; in.mi.dwFlags = flags; in.mi.mouseData = data;
    SendInput(1, &in, sizeof(in));
}

static void send_mouse_wheel(BOOL up) {
    INPUT in; ZeroMemory(&in, sizeof(in));
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    in.mi.mouseData = (DWORD)(up ? WHEEL_DELTA : -WHEEL_DELTA);
    SendInput(1, &in, sizeof(in));
}

static void send_mouse_move(LONG dx, LONG dy) {
    if (!dx && !dy) return;
    INPUT in; ZeroMemory(&in, sizeof(in));
    in.type = INPUT_MOUSE; in.mi.dx = dx; in.mi.dy = dy; in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(in));
}

typedef enum { CODE_NONE = 0, CODE_KEY, CODE_MOUSE, CODE_WHEEL } CodeKind;
typedef struct { CodeKind kind; WORD value; } Code;
typedef struct { Code codes[MAX_CODES]; int count; BOOL was_down; } Binding;

int StickBind_Ordinal1(void) { return 1; }

static void binding_clear(Binding *b) { ZeroMemory(b, sizeof(*b)); }

static int g_key_refs[256];
static int g_mouse_refs[5];

static void key_ref(WORD vk, BOOL down) {
    if (vk >= 256) { send_key(vk, down); return; }
    if (down) {
        if (g_key_refs[vk]++ == 0) send_key(vk, TRUE);
    } else {
        if (g_key_refs[vk] > 0 && --g_key_refs[vk] == 0) send_key(vk, FALSE);
    }
}

static void mouse_ref(int button, BOOL down) {
    if (button < 0 || button >= 5) return;
    if (down) {
        if (g_mouse_refs[button]++ == 0) send_mouse_button(button, TRUE);
    } else {
        if (g_mouse_refs[button] > 0 && --g_mouse_refs[button] == 0) send_mouse_button(button, FALSE);
    }
}

static void binding_emit(Binding *b, BOOL down) {
    if (down) {
        for (int i = 0; i < b->count; i++) {
            if (b->codes[i].kind == CODE_KEY) key_ref(b->codes[i].value, TRUE);
            else if (b->codes[i].kind == CODE_WHEEL) send_mouse_wheel(b->codes[i].value == 1);
            else if (b->codes[i].kind == CODE_MOUSE) mouse_ref((int)b->codes[i].value, TRUE);
        }
    } else {
        for (int i = b->count - 1; i >= 0; i--) {
            if (b->codes[i].kind == CODE_KEY) key_ref(b->codes[i].value, FALSE);
            else if (b->codes[i].kind == CODE_MOUSE) mouse_ref((int)b->codes[i].value, FALSE);
        }
    }
}

static void binding_set(Binding *b, BOOL down) {
    if (b->count == 0) return;
    if (down == b->was_down) return;
    binding_emit(b, down);
    b->was_down = down;
}

static void binding_release(Binding *b) {
    if (b->was_down) { binding_emit(b, FALSE); b->was_down = FALSE; }
}

static Code parse_one_token(const wchar_t *s) {
    Code c = { CODE_NONE, 0 };
    if (!s || !s[0]) return c;
    if (_wcsicmp(s, L"NONE") == 0 || _wcsicmp(s, L"OFF") == 0) return c;

    if (_wcsicmp(s, L"MOUSE1") == 0 || _wcsicmp(s, L"LBUTTON") == 0 || _wcsicmp(s, L"LMB") == 0) {
        c.kind = CODE_MOUSE; c.value = 0; return c;
    }
    if (_wcsicmp(s, L"MOUSE2") == 0 || _wcsicmp(s, L"RBUTTON") == 0 || _wcsicmp(s, L"RMB") == 0) {
        c.kind = CODE_MOUSE; c.value = 1; return c;
    }
    if (_wcsicmp(s, L"MOUSE3") == 0 || _wcsicmp(s, L"MBUTTON") == 0 || _wcsicmp(s, L"MMB") == 0) {
        c.kind = CODE_MOUSE; c.value = 2; return c;
    }
    if (_wcsicmp(s, L"MOUSE4") == 0 || _wcsicmp(s, L"XBUTTON1") == 0) {
        c.kind = CODE_MOUSE; c.value = 3; return c;
    }
    if (_wcsicmp(s, L"MOUSE5") == 0 || _wcsicmp(s, L"XBUTTON2") == 0) {
        c.kind = CODE_MOUSE; c.value = 4; return c;
    }

    if (_wcsicmp(s, L"MOUSEWHEELUP") == 0) { c.kind = CODE_WHEEL; c.value = 1; return c; }
    if (_wcsicmp(s, L"MOUSEWHEELDOWN") == 0) { c.kind = CODE_WHEEL; c.value = 0; return c; }

    if (s[0] && !s[1]) {
        wchar_t ch = s[0];
        if (ch >= L'a' && ch <= L'z') ch -= (L'a' - L'A');
        if ((ch >= L'A' && ch <= L'Z') || (ch >= L'0' && ch <= L'9')) {
            c.kind = CODE_KEY; c.value = (WORD)ch; return c;
        }
        switch (ch) {
            case L';': case L':': c.kind = CODE_KEY; c.value = VK_OEM_1; return c;
            case L'/': case L'?': c.kind = CODE_KEY; c.value = VK_OEM_2; return c;
            case L'`': case L'~': c.kind = CODE_KEY; c.value = VK_OEM_3; return c;
            case L'[': case L'{': c.kind = CODE_KEY; c.value = VK_OEM_4; return c;
            case L'\\': case L'|': c.kind = CODE_KEY; c.value = VK_OEM_5; return c;
            case L']': case L'}': c.kind = CODE_KEY; c.value = VK_OEM_6; return c;
            case L'\'': case L'"': c.kind = CODE_KEY; c.value = VK_OEM_7; return c;
            case L'=': case L'+': c.kind = CODE_KEY; c.value = VK_OEM_PLUS; return c;
            case L',': case L'<': c.kind = CODE_KEY; c.value = VK_OEM_COMMA; return c;
            case L'-': case L'_': c.kind = CODE_KEY; c.value = VK_OEM_MINUS; return c;
            case L'.': case L'>': c.kind = CODE_KEY; c.value = VK_OEM_PERIOD; return c;
            default: break;
        }
    }

    WORD vk = 0;
    if (_wcsicmp(s, L"SPACE") == 0) vk = VK_SPACE;
    else if (_wcsicmp(s, L"ENTER") == 0 || _wcsicmp(s, L"RETURN") == 0) vk = VK_RETURN;
    else if (_wcsicmp(s, L"ESC") == 0 || _wcsicmp(s, L"ESCAPE") == 0) vk = VK_ESCAPE;
    else if (_wcsicmp(s, L"TAB") == 0) vk = VK_TAB;
    else if (_wcsicmp(s, L"SHIFT") == 0 || _wcsicmp(s, L"LSHIFT") == 0) vk = VK_SHIFT;
    else if (_wcsicmp(s, L"RSHIFT") == 0) vk = VK_RSHIFT;
    else if (_wcsicmp(s, L"CTRL") == 0 || _wcsicmp(s, L"CONTROL") == 0 || _wcsicmp(s, L"LCTRL") == 0) vk = VK_CONTROL;
    else if (_wcsicmp(s, L"RCTRL") == 0) vk = VK_RCONTROL;
    else if (_wcsicmp(s, L"ALT") == 0 || _wcsicmp(s, L"LALT") == 0) vk = VK_MENU;
    else if (_wcsicmp(s, L"RALT") == 0) vk = VK_RMENU;
    else if (_wcsicmp(s, L"BACKSPACE") == 0 || _wcsicmp(s, L"BKSP") == 0 || _wcsicmp(s, L"BACK") == 0) vk = VK_BACK;
    else if (_wcsicmp(s, L"DELETE") == 0 || _wcsicmp(s, L"DEL") == 0) vk = VK_DELETE;
    else if (_wcsicmp(s, L"INSERT") == 0 || _wcsicmp(s, L"INS") == 0) vk = VK_INSERT;
    else if (_wcsicmp(s, L"HOME") == 0) vk = VK_HOME;
    else if (_wcsicmp(s, L"END") == 0) vk = VK_END;
    else if (_wcsicmp(s, L"PGUP") == 0 || _wcsicmp(s, L"PAGEUP") == 0) vk = VK_PRIOR;
    else if (_wcsicmp(s, L"PGDN") == 0 || _wcsicmp(s, L"PAGEDOWN") == 0) vk = VK_NEXT;
    else if (_wcsicmp(s, L"UP") == 0) vk = VK_UP;
    else if (_wcsicmp(s, L"DOWN") == 0) vk = VK_DOWN;
    else if (_wcsicmp(s, L"LEFT") == 0) vk = VK_LEFT;
    else if (_wcsicmp(s, L"RIGHT") == 0) vk = VK_RIGHT;
    else if (_wcsicmp(s, L"CAPSLOCK") == 0 || _wcsicmp(s, L"CAPS") == 0) vk = VK_CAPITAL;
    else if (_wcsicmp(s, L"NUMLOCK") == 0) vk = VK_NUMLOCK;
    else if (_wcsicmp(s, L"SCROLLLOCK") == 0 || _wcsicmp(s, L"SCROLL") == 0) vk = VK_SCROLL;
    else if (_wcsicmp(s, L"PAUSE") == 0 || _wcsicmp(s, L"BREAK") == 0) vk = VK_PAUSE;
    else if (_wcsicmp(s, L"PRINTSCREEN") == 0 || _wcsicmp(s, L"PRTSC") == 0) vk = VK_SNAPSHOT;
    else if (_wcsicmp(s, L"LWIN") == 0) vk = VK_LWIN;
    else if (_wcsicmp(s, L"RWIN") == 0) vk = VK_RWIN;
    else if (_wcsicmp(s, L"APPS") == 0 || _wcsicmp(s, L"MENU") == 0) vk = VK_APPS;
    else if (_wcsicmp(s, L"SEMICOLON") == 0) vk = VK_OEM_1;
    else if (_wcsicmp(s, L"SLASH") == 0) vk = VK_OEM_2;
    else if (_wcsicmp(s, L"GRAVE") == 0 || _wcsicmp(s, L"TILDE") == 0) vk = VK_OEM_3;
    else if (_wcsicmp(s, L"LBRACKET") == 0 || _wcsicmp(s, L"OPENBRACKET") == 0) vk = VK_OEM_4;
    else if (_wcsicmp(s, L"BACKSLASH") == 0) vk = VK_OEM_5;
    else if (_wcsicmp(s, L"RBRACKET") == 0 || _wcsicmp(s, L"CLOSEBRACKET") == 0) vk = VK_OEM_6;
    else if (_wcsicmp(s, L"QUOTE") == 0) vk = VK_OEM_7;
    else if (_wcsicmp(s, L"EQUALS") == 0) vk = VK_OEM_PLUS;
    else if (_wcsicmp(s, L"COMMA") == 0) vk = VK_OEM_COMMA;
    else if (_wcsicmp(s, L"MINUS") == 0 || _wcsicmp(s, L"DASH") == 0) vk = VK_OEM_MINUS;
    else if (_wcsicmp(s, L"PERIOD") == 0 || _wcsicmp(s, L"DOT") == 0) vk = VK_OEM_PERIOD;
    else if (_wcsicmp(s, L"NUMPAD0") == 0 || _wcsicmp(s, L"NUM0") == 0) vk = VK_NUMPAD0;
    else if (_wcsicmp(s, L"NUMPAD1") == 0 || _wcsicmp(s, L"NUM1") == 0) vk = VK_NUMPAD1;
    else if (_wcsicmp(s, L"NUMPAD2") == 0 || _wcsicmp(s, L"NUM2") == 0) vk = VK_NUMPAD2;
    else if (_wcsicmp(s, L"NUMPAD3") == 0 || _wcsicmp(s, L"NUM3") == 0) vk = VK_NUMPAD3;
    else if (_wcsicmp(s, L"NUMPAD4") == 0 || _wcsicmp(s, L"NUM4") == 0) vk = VK_NUMPAD4;
    else if (_wcsicmp(s, L"NUMPAD5") == 0 || _wcsicmp(s, L"NUM5") == 0) vk = VK_NUMPAD5;
    else if (_wcsicmp(s, L"NUMPAD6") == 0 || _wcsicmp(s, L"NUM6") == 0) vk = VK_NUMPAD6;
    else if (_wcsicmp(s, L"NUMPAD7") == 0 || _wcsicmp(s, L"NUM7") == 0) vk = VK_NUMPAD7;
    else if (_wcsicmp(s, L"NUMPAD8") == 0 || _wcsicmp(s, L"NUM8") == 0) vk = VK_NUMPAD8;
    else if (_wcsicmp(s, L"NUMPAD9") == 0 || _wcsicmp(s, L"NUM9") == 0) vk = VK_NUMPAD9;
    else if (_wcsicmp(s, L"NUMPAD*") == 0 || _wcsicmp(s, L"MULTIPLY") == 0) vk = VK_MULTIPLY;
    else if (_wcsicmp(s, L"NUMPAD+") == 0 || _wcsicmp(s, L"ADD") == 0) vk = VK_ADD;
    else if (_wcsicmp(s, L"NUMPAD-") == 0 || _wcsicmp(s, L"SUBTRACT") == 0) vk = VK_SUBTRACT;
    else if (_wcsicmp(s, L"NUMPAD.") == 0 || _wcsicmp(s, L"DECIMAL") == 0) vk = VK_DECIMAL;
    else if (_wcsicmp(s, L"NUMPAD/") == 0 || _wcsicmp(s, L"DIVIDE") == 0) vk = VK_DIVIDE;
    else if (s[0] == L'F' || s[0] == L'f') {
        int n = 0; const wchar_t *p = s + 1;
        while (*p >= L'0' && *p <= L'9') { n = n * 10 + (*p - L'0'); p++; }
        if (n >= 1 && n <= 12 && !*p) vk = (WORD)(VK_F1 + n - 1);
    }
    if (vk) { c.kind = CODE_KEY; c.value = vk; }
    return c;
}

static Binding parse_binding(const wchar_t *s) {
    Binding b; binding_clear(&b);
    if (!s || !s[0]) return b;
    wchar_t buf[256];
    wcsncpy(buf, s, 255); buf[255] = 0;
    wchar_t *ctx = NULL;
    for (wchar_t *tok = wcstok(buf, L",", &ctx); tok && b.count < MAX_CODES; tok = wcstok(NULL, L",", &ctx)) {
        while (*tok == L' ' || *tok == L'\t') tok++;
        wchar_t *end = tok + wcslen(tok);
        while (end > tok && (end[-1] == L' ' || end[-1] == L'\t')) *--end = 0;
        Code c = parse_one_token(tok);
        if (c.kind != CODE_NONE) b.codes[b.count++] = c;
    }
    return b;
}

enum {
    IDX_A, IDX_B, IDX_X, IDX_Y, IDX_LB, IDX_RB, IDX_START, IDX_BACK,
    IDX_LS, IDX_RS, IDX_DUP, IDX_DDOWN, IDX_DLEFT, IDX_DRIGHT, IDX_LT, IDX_RT, IDX_COUNT
};

static const WORD xinput_masks[IDX_COUNT] = {
    XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
    XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
    XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK,
    XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
    XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN,
    XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT, 0, 0
};

static const wchar_t *btn_names[IDX_COUNT] = {
    L"A", L"B", L"X", L"Y", L"LB", L"RB", L"START", L"BACK",
    L"LS", L"RS", L"DUP", L"DDOWN", L"DLEFT", L"DRIGHT", L"LT", L"RT"
};

enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT, DIR_COUNT };
static const wchar_t *dir_names[DIR_COUNT] = { L"UP", L"DOWN", L"LEFT", L"RIGHT" };
enum { LAYER_BASE = 0, LAYER_MOD1 = 1, LAYER_MOD2 = 2, LAYER_COUNT = 3 };

typedef struct {
    Binding btn[IDX_COUNT];
    Binding stick_l[DIR_COUNT];
    Binding stick_r[DIR_COUNT];
    BOOL left_is_mouse;
    BOOL right_is_mouse;
} Layer;

static HMODULE g_module;
static HANDLE g_thread;
static volatile LONG g_running = 1;
static Layer g_layer[LAYER_COUNT];
static int g_mod1_btn = -1, g_mod2_btn = -1;
static int g_deadzone_pct = 20, g_mouse_sens = 10, g_trigger_threshold = 30;
static BOOL g_invert_y = FALSE;
static int g_active_layer = LAYER_BASE;
static int g_pad_index = -1;

static int name_to_btn(const wchar_t *s) {
    if (!s || !s[0] || _wcsicmp(s, L"NONE") == 0 || _wcsicmp(s, L"OFF") == 0) return -1;
    for (int i = 0; i < IDX_COUNT; i++)
        if (_wcsicmp(s, btn_names[i]) == 0) return i;
    return -1;
}

static BOOL parse_bool(const wchar_t *s) {
    return _wcsicmp(s, L"true") == 0 || _wcsicmp(s, L"1") == 0 ||
           _wcsicmp(s, L"yes") == 0 || _wcsicmp(s, L"on") == 0;
}

static void load_layer_buttons(const wchar_t *section, Layer *layer, const wchar_t *ini) {
    wchar_t buf[128];
    for (int i = 0; i < IDX_COUNT; i++) {
        GetPrivateProfileStringW(section, btn_names[i], L"NONE", buf, 128, ini);
        layer->btn[i] = parse_binding(buf);
    }
}

static void load_layer_stick(const wchar_t *section, BOOL *is_mouse, Binding dirs[DIR_COUNT],
                             const wchar_t *ini, const wchar_t *default_mode) {
    wchar_t buf[128];
    GetPrivateProfileStringW(section, L"Mode", default_mode, buf, 128, ini);
    *is_mouse = (_wcsicmp(buf, L"Mouse") == 0);
    for (int i = 0; i < DIR_COUNT; i++) {
        GetPrivateProfileStringW(section, dir_names[i], L"NONE", buf, 128, ini);
        dirs[i] = parse_binding(buf);
    }
}

static BOOL stick_section_defined(const wchar_t *section, const wchar_t *ini) {
    wchar_t buf[128];
    GetPrivateProfileStringW(section, L"Mode", L"", buf, 128, ini);
    if (buf[0]) return TRUE;
    for (int i = 0; i < DIR_COUNT; i++) {
        GetPrivateProfileStringW(section, dir_names[i], L"", buf, 128, ini);
        if (buf[0]) return TRUE;
    }
    return FALSE;
}

static void load_config(void) {
    static wchar_t dll_path[32768], ini_path[32768];
    wchar_t buf[128];
    DWORD n = GetModuleFileNameW(g_module, dll_path, 32768);
    if (!n || n >= 32768) return;
    wchar_t *slash = wcsrchr(dll_path, L'\\');
    if (slash) *(slash + 1) = 0;
    int path_len = swprintf(
        ini_path,
        sizeof(ini_path) / sizeof(ini_path[0]),
        L"%lsStickBind.ini",
        dll_path
    );

    if (path_len < 0 ||
        (size_t)path_len >= sizeof(ini_path) / sizeof(ini_path[0])) {
        return;
    }

    g_deadzone_pct = (int)GetPrivateProfileIntW(L"Controller", L"Deadzone", 20, ini_path);
    if (g_deadzone_pct < 0) g_deadzone_pct = 0;
    if (g_deadzone_pct > 90) g_deadzone_pct = 90;
    g_mouse_sens = (int)GetPrivateProfileIntW(L"Controller", L"MouseSensitivity", 10, ini_path);
    if (g_mouse_sens < 1) g_mouse_sens = 1;
    if (g_mouse_sens > 100) g_mouse_sens = 100;
    GetPrivateProfileStringW(L"Controller", L"InvertY", L"false", buf, 128, ini_path);
    g_invert_y = parse_bool(buf);
    g_trigger_threshold = (int)GetPrivateProfileIntW(L"Controller", L"TriggerThreshold", 30, ini_path);
    if (g_trigger_threshold < 1) g_trigger_threshold = 1;
    if (g_trigger_threshold > 250) g_trigger_threshold = 250;

    GetPrivateProfileStringW(L"Controller", L"ScanCodes", L"true", buf, 128, ini_path);
    g_use_scancodes = parse_bool(buf);
    GetPrivateProfileStringW(L"Controller", L"RequireFocus", L"true", buf, 128, ini_path);
    g_require_focus = parse_bool(buf);

    GetPrivateProfileStringW(L"Modifiers", L"Mod1", L"NONE", buf, 128, ini_path);
    g_mod1_btn = name_to_btn(buf);
    GetPrivateProfileStringW(L"Modifiers", L"Mod2", L"NONE", buf, 128, ini_path);
    g_mod2_btn = name_to_btn(buf);

    ZeroMemory(g_layer, sizeof(g_layer));
    load_layer_buttons(L"Buttons", &g_layer[LAYER_BASE], ini_path);
    load_layer_stick(L"LeftStick", &g_layer[LAYER_BASE].left_is_mouse,
                     g_layer[LAYER_BASE].stick_l, ini_path, L"Keys");
    load_layer_stick(L"RightStick", &g_layer[LAYER_BASE].right_is_mouse,
                     g_layer[LAYER_BASE].stick_r, ini_path, L"Mouse");

    g_layer[LAYER_MOD1] = g_layer[LAYER_BASE];
    g_layer[LAYER_MOD2] = g_layer[LAYER_BASE];

    if (g_mod1_btn >= 0) {
        binding_clear(&g_layer[LAYER_MOD1].btn[g_mod1_btn]);
        binding_clear(&g_layer[LAYER_MOD2].btn[g_mod1_btn]);
    }
    if (g_mod2_btn >= 0) {
        binding_clear(&g_layer[LAYER_MOD1].btn[g_mod2_btn]);
        binding_clear(&g_layer[LAYER_MOD2].btn[g_mod2_btn]);
    }

    for (int i = 0; i < IDX_COUNT; i++) {
        GetPrivateProfileStringW(L"Buttons_Mod1", btn_names[i], L"\x01", buf, 128, ini_path);
        if ((unsigned char)buf[0] != 0x01) g_layer[LAYER_MOD1].btn[i] = parse_binding(buf);
        GetPrivateProfileStringW(L"Buttons_Mod2", btn_names[i], L"\x01", buf, 128, ini_path);
        if ((unsigned char)buf[0] != 0x01) g_layer[LAYER_MOD2].btn[i] = parse_binding(buf);
    }

    const wchar_t *left_def  = g_layer[LAYER_BASE].left_is_mouse  ? L"Mouse" : L"Keys";
    const wchar_t *right_def = g_layer[LAYER_BASE].right_is_mouse ? L"Mouse" : L"Keys";

    if (stick_section_defined(L"LeftStick_Mod1", ini_path))
        load_layer_stick(L"LeftStick_Mod1", &g_layer[LAYER_MOD1].left_is_mouse,
                         g_layer[LAYER_MOD1].stick_l, ini_path, left_def);
    if (stick_section_defined(L"RightStick_Mod1", ini_path))
        load_layer_stick(L"RightStick_Mod1", &g_layer[LAYER_MOD1].right_is_mouse,
                         g_layer[LAYER_MOD1].stick_r, ini_path, right_def);
    if (stick_section_defined(L"LeftStick_Mod2", ini_path))
        load_layer_stick(L"LeftStick_Mod2", &g_layer[LAYER_MOD2].left_is_mouse,
                         g_layer[LAYER_MOD2].stick_l, ini_path, left_def);
    if (stick_section_defined(L"RightStick_Mod2", ini_path))
        load_layer_stick(L"RightStick_Mod2", &g_layer[LAYER_MOD2].right_is_mouse,
                         g_layer[LAYER_MOD2].stick_r, ini_path, right_def);
}

static void release_layer(Layer *L) {
    for (int i = 0; i < IDX_COUNT; i++) binding_release(&L->btn[i]);
    for (int i = 0; i < DIR_COUNT; i++) {
        binding_release(&L->stick_l[i]);
        binding_release(&L->stick_r[i]);
    }
}

static void release_all_layers(void) {
    for (int i = 0; i < LAYER_COUNT; i++) release_layer(&g_layer[i]);
}

static void stick_to_mouse(SHORT sx, SHORT sy, float *acc_x, float *acc_y) {
    const float axis_max = 32767.0f;
    const float dead = (float)g_deadzone_pct / 100.0f;
    float fx = (float)sx / axis_max, fy = (float)sy / axis_max;
    float mag = fx * fx + fy * fy;
    if (mag < 0.0001f) return;
    mag = sqrtf(mag);
    if (mag <= dead) return;
    float t = (mag - dead) / (1.0f - dead);
    if (t > 1.0f) t = 1.0f;
    t = t * sqrtf(t);
    fx = (fx / mag) * t; fy = (fy / mag) * t;
    float scale = (float)g_mouse_sens * 0.25f * g_dt_scale;
    *acc_x += fx * scale;
    *acc_y += (g_invert_y ? fy : -fy) * scale;
    LONG dx = (LONG)*acc_x, dy = (LONG)*acc_y;
    *acc_x -= (float)dx; *acc_y -= (float)dy;
    send_mouse_move(dx, dy);
}

static void stick_to_keys(SHORT sx, SHORT sy, Binding dirs[DIR_COUNT], SHORT dead) {
    binding_set(&dirs[DIR_UP], sy > dead);
    binding_set(&dirs[DIR_DOWN], sy < -dead);
    binding_set(&dirs[DIR_LEFT], sx < -dead);
    binding_set(&dirs[DIR_RIGHT], sx > dead);
}

static int pick_layer(const BOOL btn_down[IDX_COUNT]) {
    BOOL m1 = (g_mod1_btn >= 0 && btn_down[g_mod1_btn]);
    BOOL m2 = (g_mod2_btn >= 0 && btn_down[g_mod2_btn]);
    if (m1 && !m2) return LAYER_MOD1;
    if (m2 && !m1) return LAYER_MOD2;
    if (m1 && m2) return LAYER_MOD1;
    return LAYER_BASE;
}

static BOOL process_has_focus(void) {
    HWND fg = GetForegroundWindow();
    if (!fg) return FALSE;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    return pid == GetCurrentProcessId();
}

static BOOL state_has_activity(const XINPUT_STATE *s, SHORT dead) {
    if (s->Gamepad.wButtons) return TRUE;
    if (s->Gamepad.bLeftTrigger >= (BYTE)g_trigger_threshold ||
        s->Gamepad.bRightTrigger >= (BYTE)g_trigger_threshold) return TRUE;
    if (s->Gamepad.sThumbLX > dead || s->Gamepad.sThumbLX < -dead) return TRUE;
    if (s->Gamepad.sThumbLY > dead || s->Gamepad.sThumbLY < -dead) return TRUE;
    if (s->Gamepad.sThumbRX > dead || s->Gamepad.sThumbRX < -dead) return TRUE;
    if (s->Gamepad.sThumbRY > dead || s->Gamepad.sThumbRY < -dead) return TRUE;
    return FALSE;
}

static int pick_pad(DWORD (WINAPI *get_state)(DWORD, XINPUT_STATE *),
                    SHORT dead, XINPUT_STATE *out) {
    static DWORD next_try[4];
    static BOOL  has_try[4];

    if (g_pad_index >= 0) {
        ZeroMemory(out, sizeof(*out));
        if (get_state((DWORD)g_pad_index, out) == ERROR_SUCCESS)
            return g_pad_index;
        g_pad_index = -1;
    }

    DWORD now = GetTickCount();
    for (DWORD i = 0; i < 4; i++) {
        if (has_try[i] && (LONG)(now - next_try[i]) < 0) continue;
        XINPUT_STATE st;
        ZeroMemory(&st, sizeof(st));
        if (get_state(i, &st) != ERROR_SUCCESS) {
            has_try[i] = TRUE;
            next_try[i] = now + 1000;
            continue;
        }
        has_try[i] = FALSE;
        if (state_has_activity(&st, dead)) {
            g_pad_index = (int)i;
            *out = st;
            return g_pad_index;
        }
    }
    return -1;
}

typedef UINT (WINAPI *TimePeriodFn)(UINT);

static DWORD WINAPI mapper_thread(LPVOID unused) {
    (void)unused;
    load_config();

    HMODULE xinput = LoadLibraryW(L"xinput1_4.dll");
    if (!xinput) xinput = LoadLibraryW(L"xinput1_3.dll");
    if (!xinput) xinput = LoadLibraryW(L"xinput9_1_0.dll");
    if (!xinput) return 0;

    DWORD (WINAPI *get_state)(DWORD, XINPUT_STATE *) =
        (DWORD (WINAPI *)(DWORD, XINPUT_STATE *))GetProcAddress(xinput, "XInputGetState");
    if (!get_state) { FreeLibrary(xinput); return 0; }

    HMODULE winmm = LoadLibraryW(L"winmm.dll");
    TimePeriodFn time_begin = winmm ? (TimePeriodFn)GetProcAddress(winmm, "timeBeginPeriod") : NULL;
    TimePeriodFn time_end   = winmm ? (TimePeriodFn)GetProcAddress(winmm, "timeEndPeriod") : NULL;
    if (time_begin) time_begin(1);

    LARGE_INTEGER qpf, qpc_last, qpc_now;
    QueryPerformanceFrequency(&qpf);
    QueryPerformanceCounter(&qpc_last);

    const SHORT dead = (SHORT)((32767 * g_deadzone_pct) / 100);
    float mouse_acc_x = 0.0f, mouse_acc_y = 0.0f;
    g_active_layer = LAYER_BASE;

    while (InterlockedCompareExchange(&g_running, 1, 1)) {
        QueryPerformanceCounter(&qpc_now);
        float dt_ms = (float)((double)(qpc_now.QuadPart - qpc_last.QuadPart) * 1000.0 / (double)qpf.QuadPart);
        qpc_last = qpc_now;
        if (dt_ms < 0.5f) dt_ms = 0.5f;
        if (dt_ms > 20.0f) dt_ms = 20.0f;
        g_dt_scale = dt_ms / 4.0f;

        if (g_require_focus && !process_has_focus()) {
            release_all_layers();
            g_active_layer = LAYER_BASE;
            mouse_acc_x = mouse_acc_y = 0.0f;
            Sleep(20);
            continue;
        }

        XINPUT_STATE state;
        int pad = pick_pad(get_state, dead, &state);
        if (pad < 0) {
            release_all_layers();
            g_active_layer = LAYER_BASE;
            Sleep(4);
            continue;
        }

        BOOL btn_down[IDX_COUNT];
        const WORD buttons = state.Gamepad.wButtons;
        for (int i = 0; i < IDX_LT; i++)
            btn_down[i] = (buttons & xinput_masks[i]) != 0;
        btn_down[IDX_LT] = state.Gamepad.bLeftTrigger >= (BYTE)g_trigger_threshold;
        btn_down[IDX_RT] = state.Gamepad.bRightTrigger >= (BYTE)g_trigger_threshold;

        int layer = pick_layer(btn_down);
        if (layer != g_active_layer) {
            release_layer(&g_layer[g_active_layer]);
            g_active_layer = layer;
        }

        Layer *L = &g_layer[g_active_layer];
        for (int i = 0; i < IDX_COUNT; i++)
            binding_set(&L->btn[i], btn_down[i]);

        if (L->left_is_mouse)
            stick_to_mouse(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, &mouse_acc_x, &mouse_acc_y);
        else
            stick_to_keys(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, L->stick_l, dead);

        if (L->right_is_mouse)
            stick_to_mouse(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, &mouse_acc_x, &mouse_acc_y);
        else
            stick_to_keys(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, L->stick_r, dead);

        Sleep(4);
    }

    release_all_layers();
    if (time_end) time_end(1);
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = instance;
        DisableThreadLibraryCalls(instance);
        g_thread = CreateThread(NULL, 0, mapper_thread, NULL, 0, NULL);
    } else if (reason == DLL_PROCESS_DETACH) {
        InterlockedExchange(&g_running, 0);
        if (!reserved && g_thread) {
            WaitForSingleObject(g_thread, 100);
            CloseHandle(g_thread);
            g_thread = NULL;
            release_all_layers();
        }
    }
    return TRUE;
}
