#include "overlay_hotkey_add.h"

gboolean overlay_hotkey_add_should_capture_event(const GdkEventKey *event) {
    if (!event) {
        return FALSE;
    }

    if (event->keyval == GDK_KEY_Escape) {
        return FALSE;
    }

    GdkModifierType relevant = event->state & (GDK_CONTROL_MASK | GDK_MOD1_MASK |
                                               GDK_SUPER_MASK | GDK_META_MASK |
                                               GDK_HYPER_MASK);
    if ((event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter) && !relevant) {
        return FALSE;
    }

    GdkModifierType mods = event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK |
                                           GDK_MOD1_MASK | GDK_SUPER_MASK |
                                           GDK_META_MASK | GDK_HYPER_MASK);
    mods &= ~(GDK_LOCK_MASK | GDK_MOD2_MASK);

    if (mods & ~GDK_SHIFT_MASK) {
        return TRUE;
    }

    gunichar uni = gdk_keyval_to_unicode(event->keyval);
    if (mods == GDK_SHIFT_MASK) {
        return !(uni != 0 && g_unichar_isprint(uni));
    }

    return !(uni != 0 && g_unichar_isprint(uni));
}
