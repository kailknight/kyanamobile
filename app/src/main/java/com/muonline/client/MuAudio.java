package com.muonline.client;

import android.media.AudioAttributes;
import android.media.MediaPlayer;
import android.media.SoundPool;
import android.os.SystemClock;
import android.util.Log;

import java.util.HashMap;

/**
 * Audio for the native client, on the Java side.
 *
 * The engine originally played sound through DirectSound, and the Android port
 * routed that to SDL_mixer. That can never work here: the client drives its
 * window and GL through sokol_app rather than SDL_main, so SDL's Android Java
 * bootstrap never runs and SDL refuses to start any subsystem. SoundPool and
 * MediaPlayer need neither SDL nor an Activity context, and SoundPool is the
 * right tool for the engine's short WAVs.
 *
 * Paths arriving from native are absolute. A relative path would be resolved
 * against the JVM's user.dir - "/" for an app process - not against the working
 * directory the client chdir's to at startup.
 *
 * Two behaviours here exist to emulate DirectSound, which the engine assumes:
 *
 *  - No sound can overlap itself. The engine gives each sound one DirectSound
 *    buffer and never advances the channel index, and Play() on a buffer that
 *    is already playing is a no-op. SoundPool instead starts another stream
 *    every call. The scene update asks for the ambient beds every frame
 *    (SceneManager.cpp) - looped for wind, as a repeated one-shot for Noria's
 *    forest - so without the guards below all 24 slots fill with overlapping
 *    copies of the same sample: an audible drone, and no slot left for anything
 *    else. Looping sounds are tracked by stream; one-shots are held off until
 *    the previous instance has run its length, which native side reads from the
 *    WAV header and passes to register().
 *  - SoundPool.load is asynchronous, and a sample played before it finishes
 *    decoding is silently dropped. Since each sound is loaded on first use,
 *    that would lose the first occurrence of every sound in the game. Requests
 *    arriving mid-decode are deferred to the load callback instead.
 */
public final class MuAudio {

    private static final String TAG = "MuAudio";
    private static final int MAX_STREAMS = 24;

    private static SoundPool sPool;
    private static MediaPlayer sMusic;
    private static float sVolume = 1.0f;
    private static boolean sEnabled = true;

    /** Engine sound id -> absolute file path, filled by OpenSounds(). */
    private static final HashMap<Integer, String> sPaths = new HashMap<>();
    /** Engine sound id -> SoundPool sample id, filled on first play. */
    private static final HashMap<Integer, Integer> sSamples = new HashMap<>();
    /** Engine sound id -> stream id, for one-shot sounds. */
    private static final HashMap<Integer, Integer> sStreams = new HashMap<>();
    /** Engine sound id -> stream id, for sounds started with loop=true. */
    private static final HashMap<Integer, Integer> sLoops = new HashMap<>();
    /** Sample id still decoding -> the engine id and loop flag waiting on it. */
    private static final HashMap<Integer, Integer> sPendingIds = new HashMap<>();
    private static final HashMap<Integer, Boolean> sPendingLoops = new HashMap<>();
    /** Engine sound id -> sample length in ms, from the WAV header native side. */
    private static final HashMap<Integer, Integer> sDurations = new HashMap<>();
    /** Engine sound id -> uptime at which its current one-shot finishes. */
    private static final HashMap<Integer, Long> sBusyUntil = new HashMap<>();

    private MuAudio() {
    }

    public static synchronized void init() {
        if (sPool != null) {
            return;
        }

        AudioAttributes attributes = new AudioAttributes.Builder()
            .setUsage(AudioAttributes.USAGE_GAME)
            .setContentType(AudioAttributes.CONTENT_TYPE_SONIFICATION)
            .build();

        sPool = new SoundPool.Builder()
            .setMaxStreams(MAX_STREAMS)
            .setAudioAttributes(attributes)
            .build();

        sPool.setOnLoadCompleteListener(new SoundPool.OnLoadCompleteListener() {
            @Override
            public void onLoadComplete(SoundPool pool, int sampleId, int status) {
                int engineId;
                boolean loop;
                synchronized (MuAudio.class) {
                    Integer pendingId = sPendingIds.remove(sampleId);
                    Boolean pendingLoop = sPendingLoops.remove(sampleId);
                    if (status != 0) {
                        Log.w(TAG, "decode failed sample=" + sampleId + " status=" + status);
                        return;
                    }
                    if (pendingId == null) {
                        return;
                    }
                    engineId = pendingId;
                    loop = (pendingLoop != null) && pendingLoop;
                }
                // Outside the lock: play() takes it again.
                play(engineId, loop);
            }
        });

        Log.i(TAG, "SoundPool ready, " + MAX_STREAMS + " streams");
    }

    /**
     * Records where a sound lives. Decoding is deferred to the first play:
     * OpenSounds() registers the whole table at startup and most of it is
     * monsters a given session never meets.
     */
    public static synchronized void register(int id, String path, int durationMs) {
        if (path != null && !path.isEmpty()) {
            sPaths.put(id, path);
            sDurations.put(id, durationMs);
        }
    }

    public static synchronized void play(int id, boolean loop) {
        if (!sEnabled) {
            return;
        }
        init();

        final long now = SystemClock.uptimeMillis();
        if (loop) {
            // Already looping. The engine re-requests these every frame.
            if (sLoops.containsKey(id)) {
                return;
            }
        } else {
            // Still running from its last trigger. A duration of 0 means the
            // header could not be read, and those are left free to retrigger
            // rather than risk silencing them.
            Long busyUntil = sBusyUntil.get(id);
            if (busyUntil != null && now < busyUntil) {
                return;
            }
        }

        Integer sample = sSamples.get(id);
        if (sample == null) {
            String path = sPaths.get(id);
            if (path == null) {
                return;
            }
            int loaded = sPool.load(path, 1);
            if (loaded == 0) {
                Log.w(TAG, "load failed id=" + id + " path=" + path);
                // Remember the failure so a missing file does not retry on
                // every play attempt during combat.
                sSamples.put(id, 0);
                return;
            }
            sSamples.put(id, loaded);
            sPendingIds.put(loaded, id);
            sPendingLoops.put(loaded, loop);
            return;
        }
        if (sample == 0) {
            return;
        }
        // Still decoding; the load callback will start it.
        if (sPendingIds.containsKey(sample)) {
            return;
        }

        int stream = sPool.play(sample, sVolume, sVolume, 1, loop ? -1 : 0, 1.0f);
        if (stream == 0) {
            return;
        }
        if (loop) {
            sLoops.put(id, stream);
        } else {
            sStreams.put(id, stream);
            Integer durationMs = sDurations.get(id);
            if (durationMs != null && durationMs > 0) {
                sBusyUntil.put(id, now + durationMs);
            }
        }
    }

    /** Stops one sound - the ambient loops are stopped by id as the map changes. */
    public static synchronized void stop(int id) {
        Integer looped = sLoops.remove(id);
        Integer stream = sStreams.remove(id);
        sBusyUntil.remove(id);
        if (sPool != null) {
            if (looped != null) {
                sPool.stop(looped);
            }
            if (stream != null) {
                sPool.stop(stream);
            }
        }

        // A request still waiting on its decode must not start afterwards.
        Integer sample = sSamples.get(id);
        if (sample != null) {
            sPendingIds.remove(sample);
            sPendingLoops.remove(sample);
        }
    }

    public static synchronized void stopAll() {
        if (sPool != null) {
            for (Integer stream : sLoops.values()) {
                sPool.stop(stream);
            }
            for (Integer stream : sStreams.values()) {
                sPool.stop(stream);
            }
        }
        sLoops.clear();
        sStreams.clear();
        sBusyUntil.clear();
        sPendingIds.clear();
        sPendingLoops.clear();
    }

    public static synchronized void playMusic(String path, boolean loop) {
        if (!sEnabled || path == null || path.isEmpty()) {
            return;
        }

        stopMusic();
        try {
            sMusic = new MediaPlayer();
            sMusic.setDataSource(path);
            sMusic.setLooping(loop);
            sMusic.setVolume(sVolume, sVolume);
            sMusic.prepare();
            sMusic.start();
        } catch (Exception musicError) {
            Log.w(TAG, "music failed path=" + path + " (" + musicError.getMessage() + ")");
            sMusic = null;
        }
    }

    public static synchronized void stopMusic() {
        if (sMusic != null) {
            try {
                sMusic.stop();
            } catch (IllegalStateException ignored) {
                // Already stopped; release below regardless.
            }
            sMusic.release();
            sMusic = null;
        }
    }

    /** Backs IsEndMp3(), which the event maps use to chain one track into the next. */
    public static synchronized boolean isMusicPlaying() {
        if (sMusic == null) {
            return false;
        }
        try {
            return sMusic.isPlaying();
        } catch (IllegalStateException alreadyReleased) {
            return false;
        }
    }

    /**
     * volume is 0-100. The engine speaks DirectSound's hundredths-of-a-decibel
     * scale; that is converted to this linear percentage native side, where the
     * existing conversion already lives.
     */
    public static synchronized void setVolume(int percent) {
        sVolume = Math.max(0.0f, Math.min(1.0f, percent / 100.0f));
        if (sPool != null) {
            for (Integer stream : sLoops.values()) {
                sPool.setVolume(stream, sVolume, sVolume);
            }
            for (Integer stream : sStreams.values()) {
                sPool.setVolume(stream, sVolume, sVolume);
            }
        }
        if (sMusic != null) {
            try {
                sMusic.setVolume(sVolume, sVolume);
            } catch (IllegalStateException ignored) {
                // Released between the null check and here; nothing to set.
            }
        }
    }

    public static synchronized void setEnabled(boolean enabled) {
        sEnabled = enabled;
        if (!enabled) {
            stopAll();
            stopMusic();
        }
    }
}
