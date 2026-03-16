#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#include <sys/types.h>
#include <math.h>
#include <stdbool.h>
#include <sys/time.h>

#define BASE_STEP 6
#define MAX_STEP 40
#define INTERVAL 5000
#define PRECISION_DIVISOR 5
#define MAX_EVENTS 10000
#define SCROLL_REPEAT 2

bool enabled = true;
int speed = BASE_STEP;

#define KEYDOWN(kc) (keys[(kc)/8] & (1 << ((kc)%8)))

typedef struct {
    int type;
    int dx, dy;
    int button;
    useconds_t delay;
} Event;

Event events[MAX_EVENTS];
int event_count = 0;
bool recording = false;
bool playing = false;

struct timeval last_time;

void grab_key(Display *d, KeyCode kc) {
    unsigned int mods[] = {
        0, LockMask, Mod2Mask, LockMask | Mod2Mask,
        ControlMask, ControlMask|LockMask, ControlMask|Mod2Mask, ControlMask|LockMask|Mod2Mask,
        ControlMask|ShiftMask, ControlMask|ShiftMask|LockMask,
        ControlMask|ShiftMask|Mod2Mask, ControlMask|ShiftMask|LockMask|Mod2Mask
    };
    for (int i = 0; i < 12; i++)
        XGrabKey(d, kc, mods[i], DefaultRootWindow(d), True, GrabModeAsync, GrabModeAsync);
}

useconds_t time_diff() {
    struct timeval now;
    gettimeofday(&now, NULL);
    useconds_t diff = (now.tv_sec - last_time.tv_sec) * 1000000 + (now.tv_usec - last_time.tv_usec);
    last_time = now;
    return diff;
}

void record_motion(int dx, int dy) {
    if (!recording || event_count >= MAX_EVENTS) return;
    events[event_count++] = (Event){0, dx, dy, 0, time_diff()};
}

void record_button(int button) {
    if (!recording || event_count >= MAX_EVENTS) return;
    events[event_count++] = (Event){1, 0, 0, button, time_diff()};
}

void playback_loop(Display *d, KeyCode kc_ctrl_l, KeyCode kc_shift_l, KeyCode kc_esc) {
    char keys[32];
    while (playing) {
        for (int i = 0; i < event_count && playing; i++) {
            XQueryKeymap(d, keys);
            bool ctrl = KEYDOWN(kc_ctrl_l);
            bool shift = KEYDOWN(kc_shift_l);
            bool esc = KEYDOWN(kc_esc);
            if (ctrl && shift && esc) {
                playing = false;
                break;
            }

            usleep(events[i].delay);
            if (events[i].type == 0)
                XTestFakeRelativeMotionEvent(d, events[i].dx, events[i].dy, 0);
            else {
                XTestFakeButtonEvent(d, events[i].button, True, 0);
                XTestFakeButtonEvent(d, events[i].button, False, 0);
            }
            XFlush(d);
        }
    }
}

int main() {
    Display *d = XOpenDisplay(NULL);
    if (!d) return 1;

    KeyCode kc_left  = XKeysymToKeycode(d, XK_KP_4);
    KeyCode kc_right = XKeysymToKeycode(d, XK_KP_6);
    KeyCode kc_up    = XKeysymToKeycode(d, XK_KP_8);
    KeyCode kc_down  = XKeysymToKeycode(d, XK_KP_2);
    KeyCode kc_ul = XKeysymToKeycode(d, XK_KP_7);
    KeyCode kc_ur = XKeysymToKeycode(d, XK_KP_9);
    KeyCode kc_dl = XKeysymToKeycode(d, XK_KP_1);
    KeyCode kc_dr = XKeysymToKeycode(d, XK_KP_3);
    KeyCode kc_mid = XKeysymToKeycode(d, XK_KP_5);
    KeyCode kc_lc  = XKeysymToKeycode(d, XK_KP_0);
    KeyCode kc_rc1 = XKeysymToKeycode(d, XK_KP_Decimal);
    KeyCode kc_rc2 = XKeysymToKeycode(d, XK_KP_Delete);
    KeyCode kc_su  = XKeysymToKeycode(d, XK_KP_Subtract);
    KeyCode kc_sd  = XKeysymToKeycode(d, XK_KP_Add);
    KeyCode kc_speed_up   = XKeysymToKeycode(d, XK_KP_Multiply);
    KeyCode kc_speed_down = XKeysymToKeycode(d, XK_KP_Divide);
    KeyCode kc_toggle = XKeysymToKeycode(d, XK_KP_Enter);

    KeyCode kc_ctrl_l = XKeysymToKeycode(d, XK_Control_L);
    KeyCode kc_shift_l = XKeysymToKeycode(d, XK_Shift_L);
    KeyCode kc_esc = XKeysymToKeycode(d, XK_Escape);

    KeyCode all_keys[] = {kc_left,kc_right,kc_up,kc_down,kc_ul,kc_ur,kc_dl,kc_dr,kc_lc,kc_rc1,kc_rc2,kc_su,kc_sd,
                          kc_speed_up,kc_speed_down,kc_toggle};

    for (int i=0;i<sizeof(all_keys)/sizeof(KeyCode);i++)
        grab_key(d, all_keys[i]);

    XSelectInput(d, DefaultRootWindow(d), KeyPressMask | KeyReleaseMask);

    char keys[32];
    int accel_counter = 0;
    bool dragging = false;
    bool last_macro = false;
    int scroll_up_counter = 0;
    int scroll_down_counter = 0;

    while (1) {
        XQueryKeymap(d, keys);

        bool ctrl = KEYDOWN(kc_ctrl_l);
        bool shift = KEYDOWN(kc_shift_l);
        bool esc = KEYDOWN(kc_esc);

        bool macro_combo = shift && esc;

        if (macro_combo && !last_macro) {
            if (!recording && !playing) {
                event_count = 0;
                gettimeofday(&last_time, NULL);
                recording = true;
            } else if (recording) {
                recording = false;
                playing = true;
                playback_loop(d, kc_ctrl_l, kc_shift_l, kc_esc);
            }
        }
        last_macro = macro_combo;

        static int last_toggle=0;
        int t = KEYDOWN(kc_toggle);
        if (t && !last_toggle) enabled = !enabled;
        last_toggle = t;

        static int last_su_key=0,last_sd_key=0;
        int su_key = KEYDOWN(kc_speed_up);
        int sd_key = KEYDOWN(kc_speed_down);
        if (su_key && !last_su_key && speed < MAX_STEP) speed += 2;
        if (sd_key && !last_sd_key && speed > 2) speed -= 2;
        last_su_key=su_key; last_sd_key=sd_key;

        if (!enabled || playing) { usleep(INTERVAL); continue; }

        bool precision = ctrl;
        int move_speed = precision ? speed / PRECISION_DIVISOR : speed;

        int dx = 0, dy = 0;
        bool moving=false;

        if (KEYDOWN(kc_left))  { dx -= move_speed; moving=true; }
        if (KEYDOWN(kc_right)) { dx += move_speed; moving=true; }
        if (KEYDOWN(kc_up))    { dy -= move_speed; moving=true; }
        if (KEYDOWN(kc_down))  { dy += move_speed; moving=true; }
        if (KEYDOWN(kc_ul)) { dx -= move_speed; dy -= move_speed; moving=true; }
        if (KEYDOWN(kc_ur)) { dx += move_speed; dy -= move_speed; moving=true; }
        if (KEYDOWN(kc_dl)) { dx -= move_speed; dy += move_speed; moving=true; }
        if (KEYDOWN(kc_dr)) { dx += move_speed; dy += move_speed; moving=true; }

        if (moving && !precision) {
            accel_counter++;
            int boost = accel_counter / 15;
            if (boost > 5) boost = 5;
            if (dx > 0) dx += boost;
            if (dx < 0) dx -= boost;
            if (dy > 0) dy += boost;
            if (dy < 0) dy -= boost;
        } else if (!moving) accel_counter = 0;

        if (dx && dy) { dx /= sqrt(2); dy /= sqrt(2); }

        if (dx || dy) {
            XTestFakeRelativeMotionEvent(d, dx, dy, 0);
            XFlush(d);
            record_motion(dx, dy);
        }

        int lc = KEYDOWN(kc_lc);
        if (lc && !dragging) { XTestFakeButtonEvent(d, 1, True, 0); XFlush(d); dragging = true; }
        if (!lc && dragging) { XTestFakeButtonEvent(d, 1, False, 0); XFlush(d); dragging = false; }

        static int last_mid=0,last_rc=0;
        int mid = KEYDOWN(kc_mid);
        int rc  = KEYDOWN(kc_rc1)||KEYDOWN(kc_rc2);

        if (mid && !last_mid) { XTestFakeButtonEvent(d, 2, True, 0); XTestFakeButtonEvent(d, 2, False, 0); XFlush(d); record_button(2); }
        if (rc  && !last_rc)  { XTestFakeButtonEvent(d, 3, True, 0); XTestFakeButtonEvent(d, 3, False, 0); XFlush(d); record_button(3); }

        last_mid=mid; last_rc=rc;

        if (KEYDOWN(kc_su)) {
            if (++scroll_up_counter >= SCROLL_REPEAT) {
                XTestFakeButtonEvent(d, 4, True, 0);
                XTestFakeButtonEvent(d, 4, False, 0);
                XFlush(d);
                record_button(4);
                scroll_up_counter = 0;
            }
        } else scroll_up_counter = 0;

        if (KEYDOWN(kc_sd)) {
            if (++scroll_down_counter >= SCROLL_REPEAT) {
                XTestFakeButtonEvent(d, 5, True, 0);
                XTestFakeButtonEvent(d, 5, False, 0);
                XFlush(d);
                record_button(5);
                scroll_down_counter = 0;
            }
        } else scroll_down_counter = 0;

        usleep(INTERVAL);
    }
}
