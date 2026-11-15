package com.muonline.client;

import android.content.Context;
import android.content.pm.ServiceInfo;
import android.view.MotionEvent;
import android.view.accessibility.AccessibilityManager;
import android.accessibilityservice.AccessibilityServiceInfo;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Auto clicker detection for the phone client.
 *
 * Two independent checks, both reported through {@link #lastReason()}:
 *
 *  1. Apps: an auto clicker has to be an accessibility service to tap for the player, so the list of INSTALLED accessibility
 *     services (no special permission needed, unlike listing every app) is searched for the usual names. Installed counts, not
 *     only enabled.
 *  2. Behaviour: a person never taps with machine regularity. Taps (a quick down + up that stays in place) are collected; when
 *     many in a row come at an almost identical interval, on the same spot, and with the same press length, it is a machine.
 *
 * The numbers are deliberately far from what a hand can do (a finger's tap interval varies by tens of milliseconds, an auto
 * clicker's by 0-3 ms), so a legitimate player is never caught.
 */
public final class MuAntiClicker {
    /** Master switch. */
    public static final boolean ENABLED = true;

    // ---- behaviour thresholds ----
    private static final int TAPS_NEEDED = 30;           // consecutive taps analysed
    private static final long MAX_TAP_MS = 400;          // longer than this is a hold, not a tap
    private static final float MAX_TAP_MOVE_PX = 30f;    // moved more than this is a drag, not a tap
    private static final long MAX_GAP_MS = 2500;         // a pause this long starts the series again
    private static final long MIN_INTERVAL_MS = 25;      // faster than this is not a tap series of a single finger
    private static final double MAX_INTERVAL_STD_MS = 6.0;   // humans: 20+ ms
    private static final double MAX_POSITION_STD_PX = 1.5;   // humans: several px
    private static final double MAX_DURATION_STD_MS = 3.0;   // humans: 8+ ms

    // names an auto clicker / macro tool uses (lower case, package name or label)
    private static final String[] KEYWORDS = {
        "autoclick", "auto click", "auto-click", "auto_click", "autotap", "auto tap", "auto-tap", "auto_tap",
        "clicker", "tapper", "tap macro", "macro", "tapping", "touch automation", "autoclic", "auto clic"
    };

    private static String sReason;

    private MuAntiClicker() {
    }

    public static synchronized String lastReason() {
        return sReason;
    }

    private static synchronized void setReason(String reason) {
        if (sReason == null) {
            sReason = reason;
        }
    }

    // ------------------------------------------------------------------
    // 1. installed accessibility services
    // ------------------------------------------------------------------

    /** Returns a description of the auto clicker found, or null. */
    public static String scanInstalledServices(Context context) {
        if (!ENABLED) {
            return null;
        }

        // Enabled accessibility services come from a plain system setting that any app may read, whatever the package
        // visibility rules (Android 11+ hides other apps' services from the installed list unless declared in <queries>).
        try {
            final String enabled = android.provider.Settings.Secure.getString(
                context.getContentResolver(), android.provider.Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES);
            if (enabled != null) {
                final String lower = enabled.toLowerCase(Locale.ROOT);
                for (String key : KEYWORDS) {
                    if (lower.contains(key)) {
                        int start = lower.indexOf(key);
                        int from = enabled.lastIndexOf(':', start);
                        int to = enabled.indexOf(':', start);
                        final String found = enabled.substring(from < 0 ? 0 : from + 1, to < 0 ? enabled.length() : to);
                        setReason("Auto clicker app found: " + found);
                        return sReason;
                    }
                }
            }
        } catch (Throwable ignored) {
        }

        try {
            final AccessibilityManager am = (AccessibilityManager) context.getSystemService(Context.ACCESSIBILITY_SERVICE);
            if (am == null) {
                return null;
            }

            final List<AccessibilityServiceInfo> list = am.getInstalledAccessibilityServiceList();
            if (list == null) {
                return null;
            }

            for (AccessibilityServiceInfo info : list) {
                final ServiceInfo si = info.getResolveInfo().serviceInfo;
                final String pkg = si.packageName == null ? "" : si.packageName;
                String label = "";
                try {
                    final CharSequence l = info.getResolveInfo().loadLabel(context.getPackageManager());
                    label = l == null ? "" : l.toString();
                } catch (Throwable ignored) {
                }

                final String hay = (pkg + " " + label).toLowerCase(Locale.ROOT);
                for (String key : KEYWORDS) {
                    if (hay.contains(key)) {
                        final String found = (label.isEmpty() ? pkg : label + " (" + pkg + ")");
                        setReason("Auto clicker app found: " + found);
                        return sReason;
                    }
                }
            }
        } catch (Throwable ignored) {
        }

        return null;
    }

    // ------------------------------------------------------------------
    // 2. tap behaviour
    // ------------------------------------------------------------------

    private static final class Tap {
        long downTime;
        float x;
        float y;
        long duration;
    }

    private static final class Down {
        long time;
        float x;
        float y;
        boolean valid;
    }

    private static final int MAX_POINTERS = 10;
    private static final Down[] sDown = new Down[MAX_POINTERS];
    private static final ArrayList<Tap> sTaps = new ArrayList<>();

    static {
        for (int i = 0; i < MAX_POINTERS; i++) {
            sDown[i] = new Down();
        }
    }

    /** Feed every touch event of the game window. Returns a reason when the series looks automated, else null. */
    public static String onTouch(MotionEvent ev) {
        if (!ENABLED || ev == null) {
            return null;
        }

        final int action = ev.getActionMasked();

        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            final int index = ev.getActionIndex();
            final int id = ev.getPointerId(index);
            if (id >= 0 && id < MAX_POINTERS) {
                final Down d = sDown[id];
                d.time = ev.getEventTime();
                d.x = ev.getX(index);
                d.y = ev.getY(index);
                d.valid = true;
            }
            return null;
        }

        if (action == MotionEvent.ACTION_MOVE) {
            for (int i = 0; i < ev.getPointerCount(); i++) {
                final int id = ev.getPointerId(i);
                if (id >= 0 && id < MAX_POINTERS && sDown[id].valid) {
                    final float dx = ev.getX(i) - sDown[id].x;
                    final float dy = ev.getY(i) - sDown[id].y;
                    if (dx * dx + dy * dy > MAX_TAP_MOVE_PX * MAX_TAP_MOVE_PX) {
                        sDown[id].valid = false;   // a drag (joystick, swipe): not a tap
                    }
                }
            }
            return null;
        }

        if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
            final int index = ev.getActionIndex();
            final int id = ev.getPointerId(index);
            if (id < 0 || id >= MAX_POINTERS || !sDown[id].valid) {
                return null;
            }

            final Down d = sDown[id];
            d.valid = false;

            final long duration = ev.getEventTime() - d.time;
            if (duration < 0 || duration > MAX_TAP_MS) {
                return null;   // a hold
            }

            final Tap t = new Tap();
            t.downTime = d.time;
            t.x = d.x;
            t.y = d.y;
            t.duration = duration;
            return addTap(t);
        }

        if (action == MotionEvent.ACTION_CANCEL) {
            for (int i = 0; i < MAX_POINTERS; i++) {
                sDown[i].valid = false;
            }
        }

        return null;
    }

    private static String addTap(Tap t) {
        if (!sTaps.isEmpty()) {
            final long gap = t.downTime - sTaps.get(sTaps.size() - 1).downTime;
            if (gap > MAX_GAP_MS || gap < MIN_INTERVAL_MS) {
                sTaps.clear();   // a pause, or another finger: a new series
            }
        }

        sTaps.add(t);

        if (sTaps.size() > TAPS_NEEDED) {
            sTaps.remove(0);
        }

        if (sTaps.size() < TAPS_NEEDED) {
            return null;
        }

        // the intervals, positions and press lengths of the series
        final int n = sTaps.size();
        final double[] interval = new double[n - 1];
        final double[] xs = new double[n];
        final double[] ys = new double[n];
        final double[] dur = new double[n];

        for (int i = 0; i < n; i++) {
            final Tap a = sTaps.get(i);
            xs[i] = a.x;
            ys[i] = a.y;
            dur[i] = a.duration;
            if (i > 0) {
                interval[i - 1] = a.downTime - sTaps.get(i - 1).downTime;
            }
        }

        final double intervalStd = std(interval);
        final double posStd = Math.max(std(xs), std(ys));
        final double durStd = std(dur);

        if (intervalStd <= MAX_INTERVAL_STD_MS && posStd <= MAX_POSITION_STD_PX && durStd <= MAX_DURATION_STD_MS) {
            sTaps.clear();
            final String reason = String.format(Locale.US,
                "Automated tapping detected (%d taps, interval spread %.1f ms, position spread %.1f px, press spread %.1f ms)",
                n, intervalStd, posStd, durStd);
            setReason(reason);
            return reason;
        }

        return null;
    }

    private static double std(double[] v) {
        double sum = 0;
        for (double x : v) {
            sum += x;
        }
        final double mean = sum / v.length;
        double sq = 0;
        for (double x : v) {
            sq += (x - mean) * (x - mean);
        }
        return Math.sqrt(sq / v.length);
    }
}
