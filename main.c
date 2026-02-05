#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#include <math.h>
#include <stdbool.h>

#define BASE_STEP 6
#define MAX_STEP 40
#define INTERVAL 5000
#define PRECISION_DIVISOR 5

bool enabled = true;
int speed = BASE_STEP;

#define KEYDOWN(kc) (keys[(kc)/8] & (1 << ((kc)%8)))

void grab_key(Display *d, KeyCode kc) {
    unsigned int mods[] = {0, LockMask, Mod2Mask, LockMask | Mod2Mask};
    for (int i = 0; i < 4; i++)
        XGrabKey(d, kc, mods[i], DefaultRootWindow(d), True, GrabModeAsync, GrabModeAsync);
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
    KeyCode kc_ctrl_r = XKeysymToKeycode(d, XK_Control_R);

    KeyCode all_keys[] = {kc_left,kc_right,kc_up,kc_down,kc_ul,kc_ur,kc_dl,kc_dr,
                          kc_mid,kc_lc,kc_rc1,kc_rc2,kc_su,kc_sd,
                          kc_speed_up,kc_speed_down,kc_toggle};

    for (int i=0;i<sizeof(all_keys)/sizeof(KeyCode);i++)
        grab_key(d, all_keys[i]);

    XSelectInput(d, DefaultRootWindow(d), KeyPressMask | KeyReleaseMask);

    char keys[32];
    int accel_counter = 0;
    bool dragging = false;

    while (1) {
        XQueryKeymap(d, keys);

        bool precision = KEYDOWN(kc_ctrl_l) || KEYDOWN(kc_ctrl_r);

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

        if (!enabled) { usleep(INTERVAL); continue; }

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
        } else if (!moving) {
            accel_counter = 0;
        }

        if (dx && dy) { dx /= sqrt(2); dy /= sqrt(2); }

        if (dx || dy) {
            XTestFakeRelativeMotionEvent(d, dx, dy, 0);
            XFlush(d);
        }

        int lc = KEYDOWN(kc_lc);

        if (lc && !dragging) {
            XTestFakeButtonEvent(d, 1, True, 0);
            XFlush(d);
            dragging = true;
        }
        if (!lc && dragging) {
            XTestFakeButtonEvent(d, 1, False, 0);
            XFlush(d);
            dragging = false;
        }

        static int last_mid=0,last_rc=0,last_su=0,last_sd=0;

        int mid = KEYDOWN(kc_mid);
        int rc  = KEYDOWN(kc_rc1)||KEYDOWN(kc_rc2);
        int su  = KEYDOWN(kc_su);
        int sd  = KEYDOWN(kc_sd);

        if (mid && !last_mid) { XTestFakeButtonEvent(d, 2, True, 0); XTestFakeButtonEvent(d, 2, False, 0); XFlush(d); }
        if (rc  && !last_rc)  { XTestFakeButtonEvent(d, 3, True, 0); XTestFakeButtonEvent(d, 3, False, 0); XFlush(d); }
        if (su  && !last_su)  { XTestFakeButtonEvent(d, 4, True, 0); XTestFakeButtonEvent(d, 4, False, 0); XFlush(d); }
        if (sd  && !last_sd)  { XTestFakeButtonEvent(d, 5, True, 0); XTestFakeButtonEvent(d, 5, False, 0); XFlush(d); }

        last_mid=mid; last_rc=rc; last_su=su; last_sd=sd;

        usleep(INTERVAL);
    }

    return 0;
}
