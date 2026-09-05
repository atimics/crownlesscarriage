/* The touch panel uses the same button bounds and input path as the canvas. */
#if defined(PLATFORM_WEB)
#define CLIENT_TOUCH_BUTTONS 96
typedef struct ClientTouchButton {
    Rectangle bounds;
    char label[128];
    bool enabled;
    bool active;
} ClientTouchButton;

static ClientTouchButton touch_buttons[CLIENT_TOUCH_BUTTONS];
static ClientTouchButton touch_previous[CLIENT_TOUCH_BUTTONS];
static int touch_count;
static int touch_previous_count;
static int touch_revision;
static bool touch_enabled;
static bool touch_pointer_pending;
static Vector2 touch_pointer;
static char touch_title[128];
static char touch_detail[512];
static char touch_previous_title[128];
static char touch_previous_detail[512];
static char touch_reading[8192];
static char touch_previous_reading[8192];
static size_t touch_reading_length;

EMSCRIPTEN_KEEPALIVE void CrownlessTouchTap(float x, float y)
{
    if (!isfinite(x) || !isfinite(y) || x < 0 || y < 0 ||
        x >= (float)GetScreenWidth() || y >= (float)GetScreenHeight()) return;
    touch_pointer = (Vector2){x, y};
    touch_pointer_pending = true;
    queued_mouse_button_press[MOUSE_BUTTON_LEFT] = true;
}

EMSCRIPTEN_KEEPALIVE void CrownlessTouchActivate(int index, int revision)
{
    if (revision != touch_revision || index < 0 || index >= touch_previous_count ||
        !touch_previous[index].enabled) return;
    Rectangle bounds = touch_previous[index].bounds;
    CrownlessTouchTap(bounds.x + bounds.width * 0.5f, bounds.y + bounds.height * 0.5f);
}

EMSCRIPTEN_KEEPALIVE void CrownlessTouchKey(int key)
{
    /* These are the supplemental controls shown by the touch panel. */
    if (key == KEY_ESCAPE || key == KEY_E || key == KEY_F || key == KEY_J ||
        key == KEY_UP || key == KEY_DOWN || key == KEY_ENTER) queued_key_press[key] = true;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
#pragma clang diagnostic ignored "-Wextra-semi"
EM_JS(int, ClientTouchEnabled, (), { return !!Module.crownlessTouchEnabled; });
EM_JS(void, ClientTouchPublishBegin, (const char *title, const char *detail, const char *reading, int revision), {
    Module.crownlessTouchFrame = {title: UTF8ToString(title), detail: UTF8ToString(detail), reading: UTF8ToString(reading), revision, buttons: []};
});
EM_JS(void, ClientTouchPublishButton, (const char *label, int enabled, int active), {
    Module.crownlessTouchFrame.buttons.push({label: UTF8ToString(label), enabled: !!enabled, active: !!active});
});
EM_JS(void, ClientTouchPublishEnd, (), {
    if (Module.renderCrownlessTouch) Module.renderCrownlessTouch(Module.crownlessTouchFrame);
});
#pragma clang diagnostic pop

static void ClientTouchBegin(void)
{
    touch_enabled = ClientTouchEnabled() != 0;
    touch_count = 0;
    (void)memset(touch_buttons, 0, sizeof(touch_buttons));
    touch_title[0] = '\0';
    touch_detail[0] = '\0';
    touch_reading[0] = '\0';
    touch_reading_length = 0;
}

static void ClientTouchRecordText(const char *text)
{
    if (!touch_enabled || text == NULL) return;
    size_t length = strlen(text);
    if (length < 3U || touch_reading_length + length + 2U > sizeof(touch_reading)) return;
    (void)memcpy(touch_reading + touch_reading_length, text, length);
    touch_reading_length += length;
    touch_reading[touch_reading_length++] = '\n';
    touch_reading[touch_reading_length] = '\0';
}

static void ClientTouchHeading(const char *title, const char *detail)
{
    (void)snprintf(touch_title, sizeof(touch_title), "%s", title);
    (void)snprintf(touch_detail, sizeof(touch_detail), "%s", detail);
}

static void ClientTouchAdd(Rectangle bounds, const char *label, bool enabled, bool active)
{
    if (!touch_enabled || touch_count >= CLIENT_TOUCH_BUTTONS) return;
    ClientTouchButton *button = &touch_buttons[touch_count++];
    button->bounds = bounds;
    button->enabled = enabled;
    button->active = active;
    (void)snprintf(button->label, sizeof(button->label), "%s", label);
}

static void ClientTouchEnd(void)
{
    if (!touch_enabled) { touch_revision = 0; return; }
    bool buttons_changed = touch_revision == 0 || touch_count != touch_previous_count ||
        memcmp(touch_buttons, touch_previous, sizeof(touch_buttons)) != 0;
    if (!buttons_changed &&
        strcmp(touch_title, touch_previous_title) == 0 &&
        strcmp(touch_detail, touch_previous_detail) == 0 &&
        strcmp(touch_reading, touch_previous_reading) == 0) return;
    if (buttons_changed) touch_revision = touch_revision == INT32_MAX ? 1 : touch_revision + 1;
    touch_previous_count = touch_count;
    (void)memcpy(touch_previous, touch_buttons, sizeof(touch_buttons));
    (void)snprintf(touch_previous_title, sizeof(touch_previous_title), "%s", touch_title);
    (void)snprintf(touch_previous_detail, sizeof(touch_previous_detail), "%s", touch_detail);
    (void)snprintf(touch_previous_reading, sizeof(touch_previous_reading), "%s", touch_reading);
    ClientTouchPublishBegin(touch_title, touch_detail, touch_reading, touch_revision);
    for (int i = 0; i < touch_count; ++i)
        ClientTouchPublishButton(touch_buttons[i].label, touch_buttons[i].enabled, touch_buttons[i].active);
    ClientTouchPublishEnd();
}
#else
static void ClientTouchBegin(void) {}
static void ClientTouchEnd(void) {}
static void ClientTouchHeading(const char *title, const char *detail) { (void)title; (void)detail; }
static void ClientTouchAdd(Rectangle bounds, const char *label, bool enabled, bool active)
{ (void)bounds; (void)label; (void)enabled; (void)active; }
#endif

static Vector2 ClientPointerPosition(void)
{
#if defined(CC_CLIENT_SELF_TESTS)
    if (client_test_pointer_active) return client_test_pointer;
#endif
#if defined(PLATFORM_WEB)
    if (touch_pointer_pending) return touch_pointer;
#endif
    return GetMousePosition();
}
