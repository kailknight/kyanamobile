package com.muonline.client;

import android.content.Context;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;

/**
 * One-shot vibration pulses for the native client (teleport aim feedback).
 * Called from the game thread through JNI; does nothing on a device without a
 * vibrator. Needs the normal-level VIBRATE permission.
 */
public final class MuHaptics {
    private static Vibrator sVibrator;

    private MuHaptics() {
    }

    public static synchronized void init(Context context) {
        try {
            sVibrator = (Vibrator) context.getApplicationContext().getSystemService(Context.VIBRATOR_SERVICE);
        } catch (Throwable t) {
            sVibrator = null;
        }
    }

    public static void pulse(int ms) {
        final Vibrator v;
        synchronized (MuHaptics.class) {
            v = sVibrator;
        }
        if (v == null || !v.hasVibrator()) {
            return;
        }
        try {
            if (Build.VERSION.SDK_INT >= 26) {
                v.vibrate(VibrationEffect.createOneShot(ms, VibrationEffect.DEFAULT_AMPLITUDE));
            } else {
                v.vibrate(ms);
            }
        } catch (Throwable ignored) {
        }
    }
}
