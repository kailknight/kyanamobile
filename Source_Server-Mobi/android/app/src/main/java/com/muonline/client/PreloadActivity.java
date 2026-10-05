package com.muonline.client;

import android.app.Activity;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ClipDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Bundle;
import android.os.Build;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.Base64;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.zip.CRC32;
import java.util.zip.Inflater;
import java.util.zip.InflaterInputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

public class PreloadActivity extends Activity {

    private static final String TAG = "MuPreload";
    private static final String[] DATA_ZIP_URL_CANDIDATES = {
        // Plain HTTP is deliberate - do not "fix" this to https. 8888 is a
        // cleartext vhost, and the only certificate this host has served on it
        // is XAMPP's stock self-signed CN=localhost one (expired 2019), which
        // HttpURLConnection rejects outright with SSLHandshakeException.
        // Cleartext is permitted for this app by usesCleartextTraffic in
        // AndroidManifest.xml. Moving to https would need a real CA-issued cert
        // on a hostname resolving straight to the origin - not arik-mu.online,
        // which is Cloudflare-fronted and does not proxy 8888 anyway.
        // A test copy lives at /dl2/data.zip (C:\xampp\htdocs\dl2, aliased into
        // the 8888 vhost) - point here at it for a test build, never commit it.
        "http://139.99.24.220:8888/data.zip"
    };
    private static final String BASIC_AUTH_USERNAME = "admin";
    private static final String BASIC_AUTH_PASSWORD = "openmu";
    private static final String DATA_FOLDER_NAME = "Data";
    private static final String DATA_READY_MARKER_FILE = ".mu_data_ready_v1";
    private static final String[] DATA_DIR_HINTS = {
        "Local",
        "Item",
        "Skill",
        "World1",
        "Object1"
    };
    private static final String[] TAKUMI_REQUIRED_FILES = {
        "Local/Mix.bmd",
        "Local/Item.bmd",
        "Local/Filter.bmd",
        "Local/FilterName.bmd",
        "Local/MonsterSkill.bmd",
        "Local/MasterSkillTreeData.bmd",
        "Local/Eng/Dialog_Eng.bmd",
        "Local/Eng/Item_Eng.bmd",
        "Local/Eng/itemtooltip_Eng.bmd",
        "Local/Eng/itemleveltooltip_Eng.bmd",
        "Local/Eng/itemtooltiptext_Eng.bmd",
        "Local/Eng/movereq_Eng.bmd",
        "Local/Eng/NpcName(Eng).txt",
        "Local/Eng/Quest_Eng.bmd",
        "Local/Eng/Skill_Eng.bmd",
        "Local/Eng/SocketItem_Eng.bmd",
        "Local/Eng/MasterSkillTooltip_Eng.bmd",
        "Local/Eng/Text_Eng.bmd",
        "Item/bsummoners01.ozj",
        "Item/bsummoners02_render.OZJ",
        "Item/bsummoners02.ozj",
        "Item/sd_soulsummoner.bmd",
        "Item/sd_soulsummonerstickA01.ozj",
        "Item/sd_soulsummonerstickA02_render.ozj",
        "Item/sd_soulsummonerstickA02.ozj",
        "Gate.bmd",
        "Macro.txt"
    };
    private static final String[] TAKUMI_REQUIRED_DIRS = {
        "Custom",
        "Effect",
        "Interface",
        "Item",
        "Local/Eng",
        "Monster",
        "Object1",
        "Player",
        "Skill",
        "World1"
    };
    private static final int[] BANNER_RES_IDS = {
        R.mipmap.banner_preload_1,
        R.mipmap.banner_preload_2
    };
    private static final long BANNER_SWITCH_MS = 2400L;
    private static final long UI_UPDATE_INTERVAL_MS = 110L;
    private static final int BUFFER_SIZE = 128 * 1024;

    private static final double DOWNLOAD_WEIGHT = 72.0;
    private static final double PARSE_WEIGHT = 28.0;

    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private final ExecutorService ioExecutor = Executors.newSingleThreadExecutor();

    private ImageView bannerView;
    private ProgressBar progressBar;
    private TextView stageText;
    private TextView detailText;
    private TextView timerText;
    private int currentBannerIndex = 0;
    private volatile boolean cancelled = false;

    private final Runnable bannerSwitcher = new Runnable() {
        @Override
        public void run() {
            if (cancelled || bannerView == null) {
                return;
            }
            currentBannerIndex = (currentBannerIndex + 1) % BANNER_RES_IDS.length;
            bannerView.animate()
                .alpha(0.08f)
                .setDuration(220L)
                .withEndAction(() -> {
                    if (cancelled || bannerView == null) {
                        return;
                    }
                    bannerView.setImageResource(BANNER_RES_IDS[currentBannerIndex]);
                    bannerView.animate().alpha(1.0f).setDuration(260L).start();
                })
                .start();
            mainHandler.postDelayed(this, BANNER_SWITCH_MS);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        configureWindow();
        buildUi();
        stageText.setText("Preparing preload...");
        detailText.setText("Contacting server...");
        timerText.setText("Waiting for network...");
        mainHandler.postDelayed(bannerSwitcher, BANNER_SWITCH_MS);
        ioExecutor.execute(this::runPreloadFlow);
    }

    @Override
    protected void onDestroy() {
        cancelled = true;
        mainHandler.removeCallbacksAndMessages(null);
        ioExecutor.shutdownNow();
        super.onDestroy();
    }

    private void configureWindow() {
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON
            | WindowManager.LayoutParams.FLAG_FULLSCREEN);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams attrs = getWindow().getAttributes();
            attrs.layoutInDisplayCutoutMode =
                WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attrs);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
        }

        getWindow().setStatusBarColor(Color.TRANSPARENT);
        getWindow().setNavigationBarColor(Color.TRANSPARENT);
        getWindow().getDecorView().setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN);
    }

    private void buildUi() {
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.parseColor("#080B14"));

        bannerView = new ImageView(this);
        bannerView.setScaleType(ImageView.ScaleType.CENTER_CROP);
        bannerView.setImageResource(BANNER_RES_IDS[currentBannerIndex]);
        bannerView.setAlpha(1.0f);
        root.addView(bannerView, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));

        View topShade = new View(this);
        topShade.setBackground(new GradientDrawable(
            GradientDrawable.Orientation.TOP_BOTTOM,
            new int[]{0x22000000, 0xB8000000}
        ));
        root.addView(topShade, new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.MATCH_PARENT
        ));

        LinearLayout panel = new LinearLayout(this);
        panel.setOrientation(LinearLayout.VERTICAL);
        int padH = dp(18);
        int padV = dp(14);
        panel.setPadding(padH, padV, padH, padV);
        panel.setBackground(createPanelBackground());

        TextView title = new TextView(this);
        title.setText("MU DATA SYNC");
        title.setTextSize(18f);
        title.setTextColor(Color.parseColor("#F3F8FF"));
        title.setLetterSpacing(0.06f);
        title.setTypeface(title.getTypeface(), android.graphics.Typeface.BOLD);
        panel.addView(title);

        stageText = new TextView(this);
        stageText.setTextSize(14f);
        stageText.setTextColor(Color.parseColor("#DCE8FF"));
        LinearLayout.LayoutParams stageLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        stageLp.topMargin = dp(8);
        panel.addView(stageText, stageLp);

        progressBar = new ProgressBar(this, null, android.R.attr.progressBarStyleHorizontal);
        progressBar.setMax(1000);
        progressBar.setProgress(0);
        progressBar.setIndeterminate(false);
        progressBar.setProgressDrawable(createProgressDrawable());
        LinearLayout.LayoutParams progressLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            dp(16)
        );
        progressLp.topMargin = dp(10);
        panel.addView(progressBar, progressLp);

        detailText = new TextView(this);
        detailText.setTextSize(12f);
        detailText.setTextColor(Color.parseColor("#C8D6EA"));
        LinearLayout.LayoutParams detailLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        detailLp.topMargin = dp(8);
        panel.addView(detailText, detailLp);

        timerText = new TextView(this);
        timerText.setTextSize(12f);
        timerText.setTextColor(Color.parseColor("#B7C9E8"));
        LinearLayout.LayoutParams timerLp = new LinearLayout.LayoutParams(
            LinearLayout.LayoutParams.MATCH_PARENT,
            LinearLayout.LayoutParams.WRAP_CONTENT
        );
        timerLp.topMargin = dp(3);
        panel.addView(timerText, timerLp);

        FrameLayout.LayoutParams panelLp = new FrameLayout.LayoutParams(
            FrameLayout.LayoutParams.MATCH_PARENT,
            FrameLayout.LayoutParams.WRAP_CONTENT
        );
        panelLp.gravity = Gravity.BOTTOM;
        int margin = dp(20);
        panelLp.setMargins(margin, margin, margin, margin);
        root.addView(panel, panelLp);

        setContentView(root);
    }

    private Drawable createPanelBackground() {
        GradientDrawable panelBg = new GradientDrawable();
        panelBg.setShape(GradientDrawable.RECTANGLE);
        panelBg.setCornerRadius(dp(18));
        panelBg.setColor(Color.parseColor("#B21A2230"));
        panelBg.setStroke(dp(1), Color.parseColor("#52FFFFFF"));
        return panelBg;
    }

    private Drawable createProgressDrawable() {
        GradientDrawable track = new GradientDrawable();
        track.setShape(GradientDrawable.RECTANGLE);
        track.setCornerRadius(dp(999));
        track.setColor(Color.parseColor("#2DFFFFFF"));

        GradientDrawable fill = new GradientDrawable(
            GradientDrawable.Orientation.LEFT_RIGHT,
            new int[]{
                Color.parseColor("#FF5EC9FF"),
                Color.parseColor("#FF56FFA4"),
                Color.parseColor("#FFFFD85B")
            });
        fill.setCornerRadius(dp(999));

        ClipDrawable clipped = new ClipDrawable(fill, Gravity.START, ClipDrawable.HORIZONTAL);
        LayerDrawable layers = new LayerDrawable(new Drawable[]{track, clipped});
        layers.setId(0, android.R.id.background);
        layers.setId(1, android.R.id.progress);
        return layers;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }

    private void runPreloadFlow() {
        final long totalStartMs = SystemClock.elapsedRealtime();
        List<File> dataRoots = collectDataRoots();
        File installRoot = null;
        File fallbackUsableRoot = null;
        try {
            checkCancelled();

            for (File root : dataRoots) {
                if (root == null) {
                    continue;
                }
                try {
                    ensureDirectory(root);
                } catch (IOException rootError) {
                    Log.w(TAG, "Skip data root " + root.getAbsolutePath() + ": " + rootError.getMessage());
                    continue;
                }

                if (installRoot == null) {
                    installRoot = root;
                }

                File existingDataDir = findChildDirectoryIgnoreCase(root, DATA_FOLDER_NAME);
                Log.i(TAG, "Check data root: " + root.getAbsolutePath()
                    + " existingDataDir=" + (existingDataDir != null ? existingDataDir.getAbsolutePath() : "null")
                    + " usable=" + isDataFolderUsable(existingDataDir)
                    + " marker=" + new File(root, DATA_READY_MARKER_FILE).exists());
                if (shouldSkipDownload(existingDataDir, root)) {
                    String dataPath = existingDataDir != null ? existingDataDir.getAbsolutePath() : "(unknown)";
                    postUi(() -> {
                        if (cancelled) {
                            return;
                        }
                        stageText.setText("Data already present. Skip download.");
                        detailText.setText(dataPath);
                        timerText.setText("Fast start mode");
                        progressBar.setProgress(1000);
                    });
                    mainHandler.postDelayed(this::launchGame, 350L);
                    return;
                }

                // data.zip changed and this root has a working copy: fetch only
                // the files that differ. Falls through to the full download if
                // that cannot be done (see runIncrementalUpdate).
                if (isDataFolderUsable(existingDataDir) && tryIncrementalUpdate(root, existingDataDir)) {
                    mainHandler.postDelayed(this::launchGame, 650L);
                    return;
                }

                if (isDataFolderUsable(existingDataDir) && fallbackUsableRoot == null) {
                    fallbackUsableRoot = root;
                }
            }

            if (installRoot == null) {
                throw new IOException("No writable data root available.");
            }

            File preloadCacheRoot = new File(installRoot, "preload_data_sync");
            recreateDirectory(preloadCacheRoot);

            File zipFile = new File(preloadCacheRoot, "data.zip");
            DownloadStats downloadStats = downloadZip(zipFile);

            long parseStartMs = SystemClock.elapsedRealtime();
            File extractedRoot = new File(preloadCacheRoot, "extract_stage");
            recreateDirectory(extractedRoot);

            ZipMetrics zipMetrics = inspectZip(zipFile);
            extractZipCaseInsensitive(zipFile, extractedRoot, zipMetrics);
            installExtractedData(extractedRoot, installRoot);
            // What was just installed, so the next data.zip change can be
            // fetched file by file instead of whole.
            writeDataIndexFromZip(zipFile, installRoot);
            long parseDurationMs = SystemClock.elapsedRealtime() - parseStartMs;

            deleteRecursively(preloadCacheRoot);

            long totalDurationMs = SystemClock.elapsedRealtime() - totalStartMs;
            long finalParseDurationMs = parseDurationMs;
            postUi(() -> {
                if (cancelled) {
                    return;
                }
                progressBar.setProgress(1000);
                stageText.setText("Data ready. Launching game...");
                detailText.setText(
                    "Download: " + formatDuration(downloadStats.durationMs)
                        + " | Parse+Install: " + formatDuration(finalParseDurationMs));
                timerText.setText("Total sync time: " + formatDuration(totalDurationMs));
            });

            mainHandler.postDelayed(this::launchGame, 650L);
        } catch (Exception ex) {
            if (fallbackUsableRoot == null) {
                for (File root : dataRoots) {
                    File fallbackDataDir = findChildDirectoryIgnoreCase(root, DATA_FOLDER_NAME);
                    if (isDataFolderUsable(fallbackDataDir)) {
                        fallbackUsableRoot = root;
                        break;
                    }
                }
            }

            if (fallbackUsableRoot != null) {
                postUi(() -> {
                    if (cancelled) {
                        return;
                    }
                    stageText.setText("Sync failed, using existing data...");
                    detailText.setText("Error code: " + errorCode(ex));
                    timerText.setText("Launching with local data");
                    progressBar.setProgress(1000);
                });
                mainHandler.postDelayed(this::launchGame, 450L);
                return;
            }

            Log.e(TAG, "Preload failed", ex);
            postUi(() -> {
                if (cancelled) {
                    return;
                }
                stageText.setText("Preload failed");
                detailText.setText("Error code: " + errorCode(ex));
                timerText.setText("Please reopen the app and try again.");
            });
        }
    }

    //=========================== incremental update ===========================
    //
    // data.zip ends with a central directory: every file's name, CRC-32, size
    // and byte offset inside the archive. The host already answers Range
    // requests (see downloadZipParallel), so when data.zip changes only that
    // directory - a few MB - has to be fetched to see WHICH files changed, and
    // then only those files' bytes. Nothing changes on the server: data.zip is
    // still uploaded exactly as before.
    //
    // DATA_INDEX_FILE holds the CRC and size of every file this device has
    // installed. It is written after a full install and after every
    // incremental one. A device installed before it existed has none, so its
    // files are checked by CRC once instead.
    //
    // Anything unexpected - no range support, a zip feature this does not
    // read, a CRC mismatch, most of the archive changed - ends in false or an
    // exception, and the caller does the full download it always did.

    private static final String DATA_INDEX_FILE = ".mu_data_index_v1";

    // Past this share of the archive changed, one full parallel download is
    // the quicker way.
    private static final double INCREMENTAL_MAX_CHANGED_SHARE = 0.5;

    // Changed files lying next to each other in the archive are fetched as
    // one range, up to about this size.
    private static final long INCREMENTAL_GROUP_BYTES = 8L * 1024L * 1024L;

    private static final long ZIP_LOCAL_HEADER_SIG = 0x04034b50L;
    private static final long ZIP_CENTRAL_SIG = 0x02014b50L;
    private static final long ZIP_END_SIG = 0x06054b50L;
    private static final long ZIP64_END_SIG = 0x06064b50L;
    private static final long ZIP64_LOCATOR_SIG = 0x07064b50L;

    private static final class RemoteZipEntry {
        String name;          // as stored in the archive
        String relPath;       // inside the Data folder; null when outside it
        String key;           // relPath, lower case
        boolean isDirectory;
        long crc;
        long compressedSize;
        long size;
        int method;
        int flags;
        long localOffset;
        long spanEnd;         // where the next entry, or the directory, starts
    }

    private static final class RemoteZipDirectory {
        final List<RemoteZipEntry> entries;
        final long centralOffset;

        RemoteZipDirectory(List<RemoteZipEntry> entries, long centralOffset) {
            this.entries = entries;
            this.centralOffset = centralOffset;
        }
    }

    private static final class IndexEntry {
        final long crc;
        final long size;

        IndexEntry(long crc, long size) {
            this.crc = crc;
            this.size = size;
        }
    }

    private boolean tryIncrementalUpdate(File root, File dataDir) {
        try {
            return runIncrementalUpdate(root, dataDir);
        } catch (Exception ex) {
            Log.w(TAG, "Incremental update failed, doing the full download: " + ex.getMessage());
            return false;
        }
    }

    private boolean runIncrementalUpdate(File root, File dataDir) throws IOException {
        final long startMs = SystemClock.elapsedRealtime();
        updateStageUi("Checking for updated files...", "Reading the file list", "", 1.0);

        String urlText = null;
        boolean withAuth = false;
        RemoteFileInfo info = null;
        for (int c = 0; c < DATA_ZIP_URL_CANDIDATES.length && info == null; c++) {
            for (boolean auth : new boolean[] { false, true }) {
                try {
                    RemoteFileInfo probed = probeRemoteFile(DATA_ZIP_URL_CANDIDATES[c], auth);
                    if (probed != null) {
                        urlText = DATA_ZIP_URL_CANDIDATES[c];
                        withAuth = auth;
                        info = probed;
                        break;
                    }
                } catch (IOException probeError) {
                    Log.i(TAG, "Incremental probe failed: " + probeError.getMessage());
                }
            }
        }
        if (info == null || !info.supportsRanges) {
            Log.i(TAG, "Incremental update needs range support, doing the full download.");
            return false;
        }

        RemoteZipDirectory directory = readRemoteCentralDirectory(urlText, withAuth, info.totalBytes);
        checkCancelled();

        Map<String, File> localFiles = new HashMap<>();
        listLocalFiles(dataDir, "", localFiles);
        Map<String, IndexEntry> index = readDataIndex(root);
        final boolean verifyByCrc = index.isEmpty();

        int fileCount = 0;
        for (RemoteZipEntry entry : directory.entries) {
            if (entry.relPath != null && !entry.isDirectory) {
                fileCount++;
            }
        }

        List<RemoteZipEntry> changed = new ArrayList<>();
        long changedBytes = 0L;
        int checked = 0;
        long lastUiTick = 0L;
        byte[] buffer = new byte[BUFFER_SIZE];

        for (RemoteZipEntry entry : directory.entries) {
            if (entry.relPath == null || entry.isDirectory) {
                continue;
            }
            checkCancelled();

            File local = localFiles.get(entry.key);
            boolean same;
            if (local == null || local.length() != entry.size) {
                same = false;
            } else if (!verifyByCrc) {
                IndexEntry known = index.get(entry.key);
                same = known != null && known.crc == entry.crc && known.size == entry.size;
            } else {
                same = crcOfFile(local, buffer) == entry.crc;
            }

            if (!same) {
                changed.add(entry);
                changedBytes += entry.spanEnd - entry.localOffset;
            }

            checked++;
            long now = SystemClock.elapsedRealtime();
            if (verifyByCrc && now - lastUiTick >= UI_UPDATE_INTERVAL_MS) {
                lastUiTick = now;
                updateStageUi("Checking local files...",
                    "Checked " + checked + " / " + fileCount + " files",
                    changed.size() + " to update so far",
                    Math.min(1.0, checked / (double) Math.max(1, fileCount)) * 20.0);
            }
        }

        Log.i(TAG, "Incremental: " + changed.size() + " of " + fileCount + " files changed, "
            + changedBytes + " of " + info.totalBytes + " bytes (verifyByCrc=" + verifyByCrc + ")");

        if (changedBytes > info.totalBytes * INCREMENTAL_MAX_CHANGED_SHARE) {
            Log.i(TAG, "Most of data.zip changed, doing the full download.");
            return false;
        }

        if (!changed.isEmpty()) {
            downloadChangedEntries(urlText, withAuth, changed, dataDir, root);
        }

        writeDataIndex(root, directory.entries);
        writeDataReadyMarker(root, dataDir, fetchRemoteDataSignature());

        final int changedCount = changed.size();
        final long fetchedBytes = changedBytes;
        final long tookMs = SystemClock.elapsedRealtime() - startMs;
        postUi(() -> {
            if (cancelled) {
                return;
            }
            progressBar.setProgress(1000);
            stageText.setText(changedCount > 0 ? "Data updated. Launching game..." : "Data up to date. Launching game...");
            detailText.setText(changedCount + " file" + (changedCount == 1 ? "" : "s") + " updated ("
                + formatBytes(fetchedBytes) + ")");
            timerText.setText("Update time: " + formatDuration(tookMs));
        });
        return true;
    }

    private RemoteZipDirectory readRemoteCentralDirectory(String urlText, boolean withAuth, long total)
        throws IOException {
        // The end record sits in the last 22 bytes plus a comment of up to 64 KB;
        // the zip64 locator, when there is one, is the 20 bytes before it.
        int tailLength = (int) Math.min(total, 65536L + 22L + 20L);
        byte[] tail = fetchRangeBytes(urlText, withAuth, total - tailLength, total - 1L);

        int end = -1;
        for (int i = tail.length - 22; i >= 0; i--) {
            if (le32(tail, i) == ZIP_END_SIG) {
                end = i;
                break;
            }
        }
        if (end < 0) {
            throw new IOException("zip end record not found");
        }

        long entryCount = le16(tail, end + 10);
        long centralSize = le32(tail, end + 12);
        long centralOffset = le32(tail, end + 16);

        if (entryCount == 0xFFFFL || centralSize == 0xFFFFFFFFL || centralOffset == 0xFFFFFFFFL) {
            if (end < 20 || le32(tail, end - 20) != ZIP64_LOCATOR_SIG) {
                throw new IOException("zip64 locator not found");
            }
            long zip64EndOffset = le64(tail, end - 20 + 8);
            byte[] zip64End = fetchRangeBytes(urlText, withAuth, zip64EndOffset, zip64EndOffset + 55L);
            if (le32(zip64End, 0) != ZIP64_END_SIG) {
                throw new IOException("zip64 end record not found");
            }
            entryCount = le64(zip64End, 32);
            centralSize = le64(zip64End, 40);
            centralOffset = le64(zip64End, 48);
        }

        if (centralSize <= 0L || centralSize > 64L * 1024L * 1024L || centralOffset + centralSize > total) {
            throw new IOException("zip central directory out of range");
        }

        byte[] central = fetchRangeBytes(urlText, withAuth, centralOffset, centralOffset + centralSize - 1L);

        List<RemoteZipEntry> entries = new ArrayList<>();
        int p = 0;
        while (p + 46 <= central.length && le32(central, p) == ZIP_CENTRAL_SIG) {
            RemoteZipEntry entry = new RemoteZipEntry();
            entry.flags = le16(central, p + 8);
            entry.method = le16(central, p + 10);
            entry.crc = le32(central, p + 16);
            entry.compressedSize = le32(central, p + 20);
            entry.size = le32(central, p + 24);
            int nameLength = le16(central, p + 28);
            int extraLength = le16(central, p + 30);
            int commentLength = le16(central, p + 32);
            entry.localOffset = le32(central, p + 42);

            int nameStart = p + 46;
            int extraEnd = nameStart + nameLength + extraLength;
            if (extraEnd + commentLength > central.length) {
                throw new IOException("zip central directory truncated");
            }

            // Same decoding openDataZip gives ZipFile, so a file lands at the
            // same path a full install would put it.
            entry.name = new String(central, nameStart, nameLength,
                (entry.flags & 0x800) != 0 ? StandardCharsets.UTF_8 : StandardCharsets.ISO_8859_1);

            // Zip64 extra field: 64-bit values for whichever fields are maxed out.
            int e = nameStart + nameLength;
            while (e + 4 <= extraEnd) {
                int id = le16(central, e);
                int size = le16(central, e + 2);
                int d = e + 4;
                int dataEnd = Math.min(extraEnd, d + size);
                if (id == 0x0001) {
                    if (entry.size == 0xFFFFFFFFL && d + 8 <= dataEnd) {
                        entry.size = le64(central, d);
                        d += 8;
                    }
                    if (entry.compressedSize == 0xFFFFFFFFL && d + 8 <= dataEnd) {
                        entry.compressedSize = le64(central, d);
                        d += 8;
                    }
                    if (entry.localOffset == 0xFFFFFFFFL && d + 8 <= dataEnd) {
                        entry.localOffset = le64(central, d);
                    }
                }
                e += 4 + size;
            }

            entry.isDirectory = entry.name.endsWith("/") || entry.name.endsWith("\\");
            entries.add(entry);
            p = extraEnd + commentLength;
        }

        if (entries.isEmpty()) {
            throw new IOException("zip central directory empty");
        }
        if (entries.size() != entryCount && entryCount != 0xFFFFL) {
            Log.w(TAG, "Central directory lists " + entries.size() + " entries, end record says " + entryCount);
        }

        assignDataRelativePaths(entries);

        // Each entry's bytes run up to the next entry's local header (or the
        // directory, for the last one). Fetching that span gets the local
        // header, the data and any data descriptor in one go.
        List<RemoteZipEntry> byOffset = new ArrayList<>(entries);
        Collections.sort(byOffset, (a, b) -> Long.compare(a.localOffset, b.localOffset));
        for (int i = 0; i < byOffset.size(); i++) {
            RemoteZipEntry entry = byOffset.get(i);
            long next = centralOffset;
            for (int j = i + 1; j < byOffset.size(); j++) {
                if (byOffset.get(j).localOffset > entry.localOffset) {
                    next = byOffset.get(j).localOffset;
                    break;
                }
            }
            if (next <= entry.localOffset || next > centralOffset) {
                throw new IOException("zip entry offsets out of order");
            }
            entry.spanEnd = next;
        }

        return new RemoteZipDirectory(entries, centralOffset);
    }

    // The full install keeps only the archive's Data folder (installExtractedData),
    // or its single top-level folder when there is no Data folder. Entries get
    // the path they have inside that folder; anything outside it gets null.
    private static void assignDataRelativePaths(List<RemoteZipEntry> entries) throws IOException {
        String top = null;
        String onlyTop = null;
        boolean severalTops = false;

        for (RemoteZipEntry entry : entries) {
            String normalized = normalizeZipPath(entry.name);
            if (normalized.isEmpty()) {
                continue;
            }
            int slash = normalized.indexOf('/');
            String first = slash >= 0 ? normalized.substring(0, slash) : (entry.isDirectory ? normalized : null);
            if (first == null) {
                continue;
            }
            if (first.equalsIgnoreCase(DATA_FOLDER_NAME)) {
                top = DATA_FOLDER_NAME;
            }
            if (onlyTop == null) {
                onlyTop = first;
            } else if (!onlyTop.equalsIgnoreCase(first)) {
                severalTops = true;
            }
        }

        if (top == null) {
            if (onlyTop == null || severalTops) {
                throw new IOException("data folder not found in data.zip");
            }
            top = onlyTop;
        }

        for (RemoteZipEntry entry : entries) {
            String normalized = normalizeZipPath(entry.name);
            int slash = normalized.indexOf('/');
            if (slash <= 0 || !normalized.substring(0, slash).equalsIgnoreCase(top)) {
                entry.relPath = null;
                entry.key = null;
                continue;
            }
            String rel = normalized.substring(slash + 1);
            if (rel.isEmpty()) {
                entry.relPath = null;
                entry.key = null;
                continue;
            }
            entry.relPath = rel;
            entry.key = rel.toLowerCase(Locale.ROOT);
        }
    }

    private void downloadChangedEntries(String urlText, boolean withAuth, List<RemoteZipEntry> changed,
                                        File dataDir, File root) throws IOException {
        final long startMs = SystemClock.elapsedRealtime();

        // Entries back to back in the archive go in one ranged request.
        List<RemoteZipEntry> sorted = new ArrayList<>(changed);
        Collections.sort(sorted, (a, b) -> Long.compare(a.localOffset, b.localOffset));
        final List<List<RemoteZipEntry>> groups = new ArrayList<>();
        List<RemoteZipEntry> current = null;
        long groupStart = 0L;
        long groupEnd = 0L;
        long totalBytes = 0L;
        for (RemoteZipEntry entry : sorted) {
            if (current != null && entry.localOffset == groupEnd
                && (entry.spanEnd - groupStart) <= INCREMENTAL_GROUP_BYTES) {
                current.add(entry);
                groupEnd = entry.spanEnd;
            } else {
                current = new ArrayList<>();
                current.add(entry);
                groups.add(current);
                groupStart = entry.localOffset;
                groupEnd = entry.spanEnd;
            }
            totalBytes += entry.spanEnd - entry.localOffset;
        }

        final File workDir = new File(root, "preload_data_patch");
        recreateDirectory(workDir);

        final java.util.concurrent.atomic.AtomicLong progress = new java.util.concurrent.atomic.AtomicLong(0L);
        final java.util.concurrent.atomic.AtomicInteger filesDone = new java.util.concurrent.atomic.AtomicInteger(0);
        final java.util.concurrent.atomic.AtomicInteger nextGroup = new java.util.concurrent.atomic.AtomicInteger(0);
        final java.util.concurrent.atomic.AtomicReference<IOException> failure =
            new java.util.concurrent.atomic.AtomicReference<>(null);

        List<Thread> workers = new ArrayList<>();
        final int workerCount = Math.min(DOWNLOAD_CHUNK_THREADS, groups.size());
        for (int w = 0; w < workerCount; w++) {
            Thread worker = new Thread(() -> {
                while (failure.get() == null && !cancelled) {
                    final int groupIndex = nextGroup.getAndIncrement();
                    if (groupIndex >= groups.size()) {
                        return;
                    }
                    List<RemoteZipEntry> group = groups.get(groupIndex);
                    long start = group.get(0).localOffset;
                    long endInclusive = group.get(group.size() - 1).spanEnd - 1L;
                    File part = new File(workDir, "g" + groupIndex + ".bin");

                    IOException lastError = null;
                    for (int attempt = 0; attempt < DOWNLOAD_CHUNK_ATTEMPTS && !cancelled; attempt++) {
                        java.util.concurrent.atomic.AtomicLong attemptBytes =
                            new java.util.concurrent.atomic.AtomicLong(0L);
                        try {
                            fetchRangeToFile(urlText, withAuth, start, endInclusive, part, progress, attemptBytes);
                            lastError = null;
                            break;
                        } catch (IOException fetchError) {
                            lastError = fetchError;
                            progress.addAndGet(-attemptBytes.get());
                            Log.w(TAG, "Update group " + groupIndex + " attempt " + (attempt + 1)
                                + " failed: " + fetchError.getMessage());
                            try {
                                Thread.sleep(400L * (attempt + 1));
                            } catch (InterruptedException interrupted) {
                                Thread.currentThread().interrupt();
                                break;
                            }
                        }
                    }

                    if (lastError == null) {
                        try {
                            applyEntriesFromPart(part, start, group, dataDir);
                            filesDone.addAndGet(group.size());
                        } catch (IOException applyError) {
                            lastError = applyError;
                        }
                    }
                    deleteQuietly(part);

                    if (lastError != null) {
                        failure.compareAndSet(null, lastError);
                        return;
                    }
                }
            }, "mu-patch-" + w);
            worker.start();
            workers.add(worker);
        }

        final int fileTotal = changed.size();
        long lastUiTick = 0L;
        long windowStartMs = startMs;
        long windowStartBytes = 0L;
        double recentSpeed = 0.0;
        while (true) {
            boolean anyAlive = false;
            for (Thread worker : workers) {
                if (worker.isAlive()) {
                    anyAlive = true;
                    break;
                }
            }

            long now = SystemClock.elapsedRealtime();
            if (now - lastUiTick >= UI_UPDATE_INTERVAL_MS) {
                lastUiTick = now;
                long soFar = progress.get();
                long windowMs = now - windowStartMs;
                if (windowMs >= 1000L) {
                    recentSpeed = (soFar - windowStartBytes) / (windowMs / 1000.0);
                    windowStartMs = now;
                    windowStartBytes = soFar;
                }
                double share = Math.min(1.0, soFar / (double) Math.max(1L, totalBytes));
                long etaMs = recentSpeed > 0.0 ? (long) (((totalBytes - soFar) / recentSpeed) * 1000.0) : 0L;
                updateStageUi(
                    String.format(Locale.US, "Updating changed files %.1f%%", share * 100.0),
                    filesDone.get() + " / " + fileTotal + " files | "
                        + formatBytes(soFar) + " / " + formatBytes(totalBytes),
                    "Speed " + formatBytes((long) recentSpeed) + "/s | ETA " + formatDuration(Math.max(0L, etaMs)),
                    20.0 + share * 78.0);
            }

            if (!anyAlive || cancelled) {
                break;
            }
            try {
                Thread.sleep(50L);
            } catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
                break;
            }
        }

        for (Thread worker : workers) {
            try {
                worker.join();
            } catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
            }
        }
        deleteRecursivelyQuietly(workDir);

        if (cancelled) {
            throw new IOException("Preload cancelled");
        }
        IOException groupFailure = failure.get();
        if (groupFailure != null) {
            throw groupFailure;
        }
    }

    // Unpacks the entries of one fetched range. Each file is written next to
    // its target and only renamed over it once its CRC matches the directory,
    // so a bad fetch never leaves a half-written game file behind.
    private void applyEntriesFromPart(File part, long partStart, List<RemoteZipEntry> entries, File dataDir)
        throws IOException {
        byte[] buffer = new byte[BUFFER_SIZE];
        byte[] header = new byte[30];

        try (java.io.RandomAccessFile input = new java.io.RandomAccessFile(part, "r")) {
            for (RemoteZipEntry entry : entries) {
                checkCancelled();

                if ((entry.flags & 0x1) != 0) {
                    throw new IOException("encrypted entry: " + entry.relPath);
                }
                if (entry.method != 0 && entry.method != 8) {
                    throw new IOException("unsupported compression " + entry.method + ": " + entry.relPath);
                }

                long base = entry.localOffset - partStart;
                input.seek(base);
                input.readFully(header);
                if (le32(header, 0) != ZIP_LOCAL_HEADER_SIG) {
                    throw new IOException("bad local header: " + entry.relPath);
                }
                long dataStart = base + 30L + le16(header, 26) + le16(header, 28);
                if (dataStart + entry.compressedSize > input.length()) {
                    throw new IOException("entry data cut short: " + entry.relPath);
                }

                File target = resolvePathCaseInsensitive(dataDir, entry.relPath, false);
                assertUnderRoot(dataDir, target);
                File temp = new File(target.getParentFile(), target.getName() + ".mu_patch");

                CRC32 crc = new CRC32();
                long written = 0L;
                input.seek(dataStart);
                Inflater inflater = entry.method == 8 ? new Inflater(true) : null;
                try {
                    InputStream raw = new RangeInputStream(input, entry.compressedSize, inflater != null);
                    InputStream data = inflater != null ? new InflaterInputStream(raw, inflater, BUFFER_SIZE) : raw;
                    try (OutputStream output = new BufferedOutputStream(new FileOutputStream(temp, false), BUFFER_SIZE)) {
                        int read;
                        while ((read = data.read(buffer)) != -1) {
                            output.write(buffer, 0, read);
                            crc.update(buffer, 0, read);
                            written += read;
                        }
                        output.flush();
                    }
                } catch (IOException writeError) {
                    deleteQuietly(temp);
                    throw writeError;
                } finally {
                    if (inflater != null) {
                        inflater.end();
                    }
                }

                if (crc.getValue() != entry.crc || written != entry.size) {
                    deleteQuietly(temp);
                    throw new IOException("CRC mismatch: " + entry.relPath);
                }

                if (target.exists() && !target.delete()) {
                    deleteQuietly(temp);
                    throw new IOException("cannot replace " + entry.relPath);
                }
                if (!temp.renameTo(target)) {
                    deleteQuietly(temp);
                    throw new IOException("cannot install " + entry.relPath);
                }
            }
        }
    }

    // A window of a RandomAccessFile. With dummyByte, one extra zero byte
    // follows the window: a raw ("nowrap") Inflater may ask for one more byte
    // than the deflate stream holds, which is what ZipFile does for it too.
    private static final class RangeInputStream extends InputStream {
        private final java.io.RandomAccessFile file;
        private long remaining;
        private boolean dummyPending;

        RangeInputStream(java.io.RandomAccessFile file, long length, boolean dummyByte) {
            this.file = file;
            this.remaining = length;
            this.dummyPending = dummyByte;
        }

        @Override
        public int read() throws IOException {
            byte[] one = new byte[1];
            int read = read(one, 0, 1);
            return read < 0 ? -1 : (one[0] & 0xFF);
        }

        @Override
        public int read(byte[] b, int off, int len) throws IOException {
            if (len == 0) {
                return 0;
            }
            if (remaining <= 0L) {
                if (dummyPending) {
                    dummyPending = false;
                    b[off] = 0;
                    return 1;
                }
                return -1;
            }
            int read = file.read(b, off, (int) Math.min(len, remaining));
            if (read < 0) {
                return -1;
            }
            remaining -= read;
            return read;
        }
    }

    private HttpURLConnection openRangeConnection(String urlText, boolean withAuth, long start, long endInclusive)
        throws IOException {
        URL url = new URL(urlText);
        HttpURLConnection connection = (HttpURLConnection) url.openConnection();
        connection.setConnectTimeout(15000);
        connection.setReadTimeout(30000);
        connection.setRequestProperty("Accept-Encoding", "identity");
        connection.setRequestProperty("User-Agent", "MuMain-Android-Preload/1.0");
        connection.setRequestProperty("Range", "bytes=" + start + "-" + endInclusive);
        connection.setInstanceFollowRedirects(true);
        if (withAuth) {
            applyBasicAuthorization(connection, BASIC_AUTH_USERNAME, BASIC_AUTH_PASSWORD);
        }
        connection.connect();

        int responseCode = connection.getResponseCode();
        if (responseCode != HttpURLConnection.HTTP_PARTIAL) {
            connection.disconnect();
            throw new HttpStatusException(responseCode,
                "HTTP " + responseCode + " for range " + start + "-" + endInclusive);
        }
        return connection;
    }

    private byte[] fetchRangeBytes(String urlText, boolean withAuth, long start, long endInclusive)
        throws IOException {
        long length = endInclusive - start + 1L;
        if (start < 0L || length <= 0L || length > 64L * 1024L * 1024L) {
            throw new IOException("bad range " + start + "-" + endInclusive);
        }

        HttpURLConnection connection = openRangeConnection(urlText, withAuth, start, endInclusive);
        try {
            byte[] data = new byte[(int) length];
            int filled = 0;
            try (InputStream input = connection.getInputStream()) {
                while (filled < data.length) {
                    int read = input.read(data, filled, data.length - filled);
                    if (read < 0) {
                        break;
                    }
                    filled += read;
                }
            }
            if (filled != data.length) {
                throw new IOException("range cut short: " + filled + " of " + data.length);
            }
            return data;
        } finally {
            connection.disconnect();
        }
    }

    private void fetchRangeToFile(String urlText, boolean withAuth, long start, long endInclusive, File output,
                                  java.util.concurrent.atomic.AtomicLong progress,
                                  java.util.concurrent.atomic.AtomicLong attemptBytes) throws IOException {
        long expected = endInclusive - start + 1L;
        HttpURLConnection connection = openRangeConnection(urlText, withAuth, start, endInclusive);
        try {
            long written = 0L;
            byte[] buffer = new byte[BUFFER_SIZE];
            try (InputStream input = new BufferedInputStream(connection.getInputStream(), BUFFER_SIZE);
                 OutputStream out = new BufferedOutputStream(new FileOutputStream(output, false), BUFFER_SIZE)) {
                int read;
                while ((read = input.read(buffer)) != -1) {
                    if (cancelled) {
                        throw new IOException("Preload cancelled");
                    }
                    out.write(buffer, 0, read);
                    written += read;
                    progress.addAndGet(read);
                    attemptBytes.addAndGet(read);
                }
                out.flush();
            }
            if (written != expected) {
                throw new IOException("range cut short: " + written + " of " + expected);
            }
        } finally {
            connection.disconnect();
        }
    }

    private static void listLocalFiles(File dir, String prefix, Map<String, File> out) {
        File[] children = dir.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            String rel = prefix.isEmpty() ? child.getName() : prefix + "/" + child.getName();
            if (child.isDirectory()) {
                listLocalFiles(child, rel, out);
            } else {
                out.put(rel.toLowerCase(Locale.ROOT), child);
            }
        }
    }

    private static long crcOfFile(File file, byte[] buffer) throws IOException {
        CRC32 crc = new CRC32();
        try (InputStream input = new FileInputStream(file)) {
            int read;
            while ((read = input.read(buffer)) != -1) {
                crc.update(buffer, 0, read);
            }
        }
        return crc.getValue();
    }

    private static Map<String, IndexEntry> readDataIndex(File root) {
        Map<String, IndexEntry> index = new HashMap<>();
        File file = new File(root, DATA_INDEX_FILE);
        if (!file.isFile()) {
            return index;
        }
        try (java.io.BufferedReader reader = new java.io.BufferedReader(
            new java.io.InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            String line;
            while ((line = reader.readLine()) != null) {
                String[] parts = line.split("\t", 3);
                if (parts.length != 3) {
                    continue;
                }
                try {
                    index.put(parts[2], new IndexEntry(Long.parseLong(parts[0], 16), Long.parseLong(parts[1])));
                } catch (NumberFormatException ignored) {
                    // skip a damaged line; that file is simply re-checked
                }
            }
        } catch (IOException readError) {
            Log.w(TAG, "Could not read the data index: " + readError.getMessage());
            index.clear();
        }
        return index;
    }

    private static void writeDataIndex(File root, Collection<RemoteZipEntry> entries) {
        File file = new File(root, DATA_INDEX_FILE);
        File temp = new File(root, DATA_INDEX_FILE + ".tmp");
        StringBuilder builder = new StringBuilder();
        for (RemoteZipEntry entry : entries) {
            if (entry.key == null || entry.isDirectory) {
                continue;
            }
            builder.append(Long.toHexString(entry.crc)).append('\t')
                .append(entry.size).append('\t')
                .append(entry.key).append('\n');
        }
        try (OutputStream output = new FileOutputStream(temp, false)) {
            output.write(builder.toString().getBytes(StandardCharsets.UTF_8));
            output.flush();
        } catch (IOException writeError) {
            Log.w(TAG, "Could not write the data index: " + writeError.getMessage());
            deleteQuietly(temp);
            return;
        }
        deleteQuietly(file);
        if (!temp.renameTo(file)) {
            Log.w(TAG, "Could not install the data index");
            deleteQuietly(temp);
        }
    }

    // After a full install the archive is still on disk: record its contents.
    // Best effort - without an index the next update checks files by CRC.
    private static void writeDataIndexFromZip(File zipFile, File root) {
        try (ZipFile zip = openDataZip(zipFile)) {
            List<RemoteZipEntry> entries = new ArrayList<>();
            Enumeration<? extends ZipEntry> all = zip.entries();
            while (all.hasMoreElements()) {
                ZipEntry zipEntry = all.nextElement();
                RemoteZipEntry entry = new RemoteZipEntry();
                entry.name = zipEntry.getName();
                entry.isDirectory = zipEntry.isDirectory();
                entry.crc = zipEntry.getCrc();
                entry.size = zipEntry.getSize();
                entries.add(entry);
            }
            assignDataRelativePaths(entries);
            writeDataIndex(root, entries);
        } catch (IOException indexError) {
            Log.w(TAG, "Could not index data.zip: " + indexError.getMessage());
            deleteQuietly(new File(root, DATA_INDEX_FILE));
        }
    }

    private static int le16(byte[] b, int at) {
        return (b[at] & 0xFF) | ((b[at + 1] & 0xFF) << 8);
    }

    private static long le32(byte[] b, int at) {
        return ((long) le16(b, at)) | (((long) le16(b, at + 2)) << 16);
    }

    private static long le64(byte[] b, int at) {
        return le32(b, at) | (le32(b, at + 4) << 32);
    }

    private List<File> collectDataRoots() {
        List<File> roots = new ArrayList<>();

        File[] externalDirs = getExternalFilesDirs(null);
        if (externalDirs != null) {
            for (File dir : externalDirs) {
                addUniqueDataRoot(roots, dir);
            }
        }

        addUniqueDataRoot(roots, getExternalFilesDir(null));

        // Extra fallback for emulators/devices where getExternalFilesDir may return null intermittently.
        File legacyExternalRoot = new File(
            Environment.getExternalStorageDirectory(),
            "Android/data/" + getPackageName() + "/files");
        addUniqueDataRoot(roots, legacyExternalRoot);

        addUniqueDataRoot(roots, getFilesDir());
        return roots;
    }

    private void addUniqueDataRoot(List<File> roots, File candidate) {
        if (candidate == null) {
            return;
        }

        String candidatePath = safeCanonicalPath(candidate);
        for (File existing : roots) {
            String existingPath = safeCanonicalPath(existing);
            if (candidatePath.equals(existingPath)) {
                return;
            }
        }

        roots.add(candidate);
    }

    private static String safeCanonicalPath(File file) {
        if (file == null) {
            return "";
        }
        try {
            return file.getCanonicalPath();
        } catch (IOException ex) {
            return file.getAbsolutePath();
        }
    }

    // Measured on the test device against the live host: one TCP stream gets
    // ~630 KB/s, four parallel range requests aggregate to ~3.8 MB/s. The limit
    // is per connection, not the server's uplink or the route (connect is 59
    // ms), so splitting the file across sockets is worth roughly 6x - about 15
    // minutes down to 2.5 for a 552 MB archive.
    //
    // Costs nothing server side: the host already answers Range requests with
    // 206, which is stock Apache behaviour, and data.zip itself is unchanged.
    private static final int DOWNLOAD_CHUNK_THREADS = 4;

    // Work-stealing chunk size. Splitting the file into one big range per
    // worker is wrong: the ranges are equal but the connections are not, so the
    // quick streams finish and exit while one straggler is left to carry the
    // rest of the file on its own. On a 1.26 GB archive that showed as fast
    // progress to roughly 830 MB and then a collapse to single-stream speed for
    // the remainder - parallelism retiring, not the network degrading.
    //
    // Many small chunks handed out from a shared counter instead: a fast
    // connection simply takes more of them, every worker stays busy until the
    // file is done, and an interrupted download loses at most one chunk rather
    // than a quarter of the archive.
    private static final long DOWNLOAD_CHUNK_BYTES = 8L * 1024L * 1024L;

    // Attempts per chunk before the transfer is given up on. Read timeouts and
    // dropped sockets are normal on mobile over a transfer this long; they
    // should cost one chunk, not the download.
    private static final int DOWNLOAD_CHUNK_ATTEMPTS = 3;

    // Below this, the setup overhead outweighs the parallelism.
    private static final long PARALLEL_DOWNLOAD_MIN_BYTES = 8L * 1024L * 1024L;

    private DownloadStats downloadZip(File outputZip) throws IOException {
        IOException lastError = null;

        // Try the parallel path first; it falls back to the sequential one on
        // any host that will not do ranges, or if anything goes wrong.
        for (String urlCandidate : DATA_ZIP_URL_CANDIDATES) {
            for (boolean withAuth : new boolean[] { false, true }) {
                try {
                    RemoteFileInfo info = probeRemoteFile(urlCandidate, withAuth);
                    if (info == null) {
                        continue;
                    }
                    if (!info.supportsRanges || info.totalBytes < PARALLEL_DOWNLOAD_MIN_BYTES) {
                        break;
                    }
                    return downloadZipParallel(urlCandidate, withAuth, outputZip, info);
                } catch (IOException parallelError) {
                    Log.w(TAG, "Parallel download failed, falling back to single stream: "
                        + parallelError.getMessage());
                    lastError = parallelError;
                }
            }
        }

        for (String urlCandidate : DATA_ZIP_URL_CANDIDATES) {
            try {
                return downloadZipFromUrl(urlCandidate, false, outputZip);
            } catch (HttpStatusException httpEx) {
                if (httpEx.statusCode == 401) {
                    try {
                        return downloadZipFromUrl(urlCandidate, true, outputZip);
                    } catch (IOException authEx) {
                        lastError = authEx;
                    }
                } else {
                    lastError = httpEx;
                }
            } catch (IOException ex) {
                lastError = ex;
            }
        }

        if (lastError != null) {
            throw lastError;
        }
        throw new IOException("Unable to download data.zip from all configured URLs.");
    }

    private static final class RemoteFileInfo {
        final long totalBytes;
        final boolean supportsRanges;
        final String signature;

        RemoteFileInfo(long totalBytes, boolean supportsRanges, String signature) {
            this.totalBytes = totalBytes;
            this.supportsRanges = supportsRanges;
            this.signature = signature;
        }
    }

    // One small ranged GET rather than a HEAD: some hosts answer HEAD without
    // Accept-Ranges even when they honour ranges, and a 206 here is proof
    // rather than a promise. Content-Range also carries the true total length.
    private RemoteFileInfo probeRemoteFile(String urlText, boolean withAuth) throws IOException {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(urlText);
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(15000);
            connection.setRequestProperty("Accept-Encoding", "identity");
            connection.setRequestProperty("User-Agent", "MuMain-Android-Preload/1.0");
            connection.setRequestProperty("Range", "bytes=0-0");
            connection.setInstanceFollowRedirects(true);
            if (withAuth) {
                applyBasicAuthorization(connection, BASIC_AUTH_USERNAME, BASIC_AUTH_PASSWORD);
            }
            connection.connect();

            int responseCode = connection.getResponseCode();
            if (responseCode == HttpURLConnection.HTTP_UNAUTHORIZED) {
                return null;
            }

            long total = -1L;
            boolean ranges = false;
            if (responseCode == HttpURLConnection.HTTP_PARTIAL) {
                ranges = true;
                String contentRange = connection.getHeaderField("Content-Range");
                if (contentRange != null) {
                    int slash = contentRange.lastIndexOf('/');
                    if (slash >= 0 && slash + 1 < contentRange.length()) {
                        try {
                            total = Long.parseLong(contentRange.substring(slash + 1).trim());
                        } catch (NumberFormatException ignored) {
                            total = -1L;
                        }
                    }
                }
            } else if (responseCode >= 200 && responseCode < 300) {
                total = connection.getContentLengthLong();
            } else {
                throw new HttpStatusException(responseCode,
                    "HTTP " + responseCode + " while probing data.zip (" + urlText + ")");
            }

            String etag = connection.getHeaderField("ETag");
            String lastModified = connection.getHeaderField("Last-Modified");
            String signature = "len=" + total
                + ";etag=" + (etag == null ? "" : etag.trim())
                + ";mod=" + (lastModified == null ? "" : lastModified.trim());

            if (total <= 0) {
                return null;
            }
            Log.i(TAG, "Probe " + urlText + ": total=" + total + " ranges=" + ranges);
            return new RemoteFileInfo(total, ranges, signature);
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    // Splits the archive across DOWNLOAD_CHUNK_THREADS sockets writing into one
    // pre-allocated file at their own offsets, and records finished chunks in a
    // sidecar so an interrupted download resumes instead of starting the whole
    // 552 MB again. The sidecar is keyed on the remote signature, so a data.zip
    // replaced mid-download is detected and the partial file discarded rather
    // than stitched together from two different archives.
    private DownloadStats downloadZipParallel(String urlText, boolean withAuth, File outputZip,
                                              RemoteFileInfo info) throws IOException {
        final long startMs = SystemClock.elapsedRealtime();
        final long total = info.totalBytes;

        File partFile = new File(outputZip.getParentFile(), outputZip.getName() + ".part");
        File stateFile = new File(outputZip.getParentFile(), outputZip.getName() + ".part.state");

        final long chunkSize = DOWNLOAD_CHUNK_BYTES;
        final int chunkCount = (int) ((total + chunkSize - 1) / chunkSize);

        boolean[] chunkDone = new boolean[chunkCount];
        long resumedBytes = 0L;
        if (partFile.isFile() && partFile.length() == total) {
            String storedState = readFirstLine(stateFile);
            if (storedState != null && storedState.startsWith(info.signature + "|")) {
                String flags = storedState.substring(info.signature.length() + 1);
                for (int i = 0; i < chunkCount && i < flags.length(); i++) {
                    if (flags.charAt(i) == '1') {
                        chunkDone[i] = true;
                        resumedBytes += chunkLength(i, chunkCount, chunkSize, total);
                    }
                }
                Log.i(TAG, "Resuming download, " + resumedBytes + " of " + total + " already present.");
            }
        }
        if (resumedBytes == 0L) {
            deleteQuietly(stateFile);
        }

        final java.util.concurrent.atomic.AtomicLong progress =
            new java.util.concurrent.atomic.AtomicLong(resumedBytes);
        final java.util.concurrent.atomic.AtomicReference<IOException> failure =
            new java.util.concurrent.atomic.AtomicReference<>(null);
        final boolean[] doneFlags = chunkDone;

        try (java.io.RandomAccessFile output = new java.io.RandomAccessFile(partFile, "rw")) {
            output.setLength(total);
        }

        // Shared hand-out counter. Every worker keeps taking the next unclaimed
        // chunk until the file is finished, so a slow connection holds up at
        // most one chunk instead of a fixed quarter of the archive.
        final java.util.concurrent.atomic.AtomicInteger nextChunk =
            new java.util.concurrent.atomic.AtomicInteger(0);

        List<Thread> workers = new ArrayList<>();
        final int workerCount = Math.min(DOWNLOAD_CHUNK_THREADS, Math.max(1, chunkCount));
        for (int w = 0; w < workerCount; w++) {
            Thread worker = new Thread(() -> {
                while (failure.get() == null && !cancelled) {
                    final int chunkIndex = nextChunk.getAndIncrement();
                    if (chunkIndex >= chunkCount) {
                        return;
                    }
                    synchronized (doneFlags) {
                        if (doneFlags[chunkIndex]) {
                            continue;
                        }
                    }

                    final long chunkStart = chunkIndex * chunkSize;
                    final long chunkEnd = Math.min(chunkStart + chunkSize, total) - 1L;

                    // Retry the chunk rather than failing the transfer. One
                    // flaky connection out of 141 used to be terminal: the
                    // chunk was abandoned, so the archive could never be
                    // completed or renamed, and 1.17 GB of good data sat in a
                    // .part file waiting on a single 8 MB hole.
                    IOException lastChunkError = null;
                    for (int attempt = 0; attempt < DOWNLOAD_CHUNK_ATTEMPTS && !cancelled; attempt++) {
                        long attemptStartBytes = progress.get();
                        try {
                            downloadRange(urlText, withAuth, partFile, chunkStart, chunkEnd, progress);
                            lastChunkError = null;
                            break;
                        } catch (IOException chunkError) {
                            lastChunkError = chunkError;
                            // A partial attempt already counted bytes towards
                            // progress; roll them back so the bar and the rate
                            // stay honest when the retry re-fetches them.
                            progress.addAndGet(attemptStartBytes - progress.get());
                            Log.w(TAG, "Chunk " + chunkIndex + " attempt " + (attempt + 1)
                                + " failed: " + chunkError.getMessage());
                            try {
                                Thread.sleep(400L * (attempt + 1));
                            } catch (InterruptedException interrupted) {
                                Thread.currentThread().interrupt();
                                break;
                            }
                        }
                    }

                    if (lastChunkError != null) {
                        failure.compareAndSet(null, lastChunkError);
                        return;
                    }

                    synchronized (doneFlags) {
                        doneFlags[chunkIndex] = true;
                        writeChunkState(stateFile, info.signature, doneFlags);
                    }
                }
            }, "mu-dl-" + w);
            worker.start();
            workers.add(worker);
        }

        long lastUiTick = 0L;
        // Sliding one-second window, so the figure shown is the rate right now
        // rather than the average since the start. Seeded past the resumed
        // bytes so they are never counted as throughput.
        long windowStartMs = startMs;
        long windowStartBytes = resumedBytes;
        double recentSpeed = 0.0;

        while (true) {
            boolean anyAlive = false;
            for (Thread worker : workers) {
                if (worker.isAlive()) {
                    anyAlive = true;
                    break;
                }
            }

            long now = SystemClock.elapsedRealtime();
            if (now - lastUiTick >= UI_UPDATE_INTERVAL_MS) {
                lastUiTick = now;
                long soFar = progress.get();

                long windowMs = now - windowStartMs;
                if (windowMs >= 1000L) {
                    recentSpeed = (soFar - windowStartBytes) / (windowMs / 1000.0);
                    windowStartMs = now;
                    windowStartBytes = soFar;
                }

                double overallPercent = Math.min(1.0, soFar / (double) total) * DOWNLOAD_WEIGHT;
                updateDownloadUi(soFar, total, Math.max(1L, now - startMs), overallPercent, recentSpeed);
            }

            if (!anyAlive || cancelled) {
                break;
            }
            try {
                Thread.sleep(50L);
            } catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
                break;
            }
        }

        for (Thread worker : workers) {
            try {
                worker.join();
            } catch (InterruptedException interrupted) {
                Thread.currentThread().interrupt();
            }
        }

        if (cancelled) {
            throw new IOException("Download cancelled");
        }
        IOException chunkFailure = failure.get();
        if (chunkFailure != null) {
            // The sidecar keeps whatever finished, so the retry resumes.
            throw chunkFailure;
        }

        deleteQuietly(outputZip);
        if (!partFile.renameTo(outputZip)) {
            throw new IOException("Unable to finalise downloaded archive: " + partFile.getAbsolutePath());
        }
        deleteQuietly(stateFile);

        long durationMs = Math.max(1L, SystemClock.elapsedRealtime() - startMs);
        updateDownloadUi(total, total, durationMs, DOWNLOAD_WEIGHT);
        Log.i(TAG, "Parallel download complete: " + total + " bytes in " + durationMs + " ms across "
            + workers.size() + " streams (" + resumedBytes + " resumed)");
        return new DownloadStats(total, total, durationMs);
    }

    private static long chunkLength(int index, int chunkCount, long chunkSize, long total) {
        long start = index * chunkSize;
        long end = Math.min(start + chunkSize, total);
        return Math.max(0L, end - start);
    }

    private void downloadRange(String urlText, boolean withAuth, File partFile,
                               long rangeStart, long rangeEnd,
                               java.util.concurrent.atomic.AtomicLong progress) throws IOException {
        HttpURLConnection connection = null;
        try {
            URL url = new URL(urlText);
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(30000);
            connection.setRequestProperty("Accept-Encoding", "identity");
            connection.setRequestProperty("User-Agent", "MuMain-Android-Preload/1.0");
            connection.setRequestProperty("Range", "bytes=" + rangeStart + "-" + rangeEnd);
            connection.setInstanceFollowRedirects(true);
            if (withAuth) {
                applyBasicAuthorization(connection, BASIC_AUTH_USERNAME, BASIC_AUTH_PASSWORD);
            }
            connection.connect();

            int responseCode = connection.getResponseCode();
            if (responseCode != HttpURLConnection.HTTP_PARTIAL) {
                throw new HttpStatusException(responseCode,
                    "HTTP " + responseCode + " for range " + rangeStart + "-" + rangeEnd);
            }

            byte[] buffer = new byte[BUFFER_SIZE];
            try (InputStream input = new BufferedInputStream(connection.getInputStream(), BUFFER_SIZE);
                 java.io.RandomAccessFile output = new java.io.RandomAccessFile(partFile, "rw")) {
                output.seek(rangeStart);
                int read;
                while ((read = input.read(buffer)) != -1) {
                    if (cancelled) {
                        throw new IOException("Download cancelled");
                    }
                    output.write(buffer, 0, read);
                    progress.addAndGet(read);
                }
            }
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private static void writeChunkState(File stateFile, String signature, boolean[] chunkDone) {
        StringBuilder flags = new StringBuilder(signature).append('|');
        for (boolean done : chunkDone) {
            flags.append(done ? '1' : '0');
        }
        try (OutputStream output = new FileOutputStream(stateFile, false)) {
            output.write(flags.toString().getBytes(StandardCharsets.UTF_8));
            output.flush();
        } catch (IOException stateError) {
            Log.w(TAG, "Failed to write resume state: " + stateError.getMessage());
        }
    }

    private static String readFirstLine(File file) {
        if (file == null || !file.isFile()) {
            return null;
        }
        try {
            byte[] raw = new byte[(int) Math.min(file.length(), 4096L)];
            try (FileInputStream input = new FileInputStream(file)) {
                int read = input.read(raw);
                if (read <= 0) {
                    return null;
                }
                return new String(raw, 0, read, StandardCharsets.UTF_8).trim();
            }
        } catch (IOException readError) {
            return null;
        }
    }

    private static void deleteQuietly(File file) {
        if (file != null && file.exists() && !file.delete()) {
            Log.w(TAG, "Could not delete " + file.getAbsolutePath());
        }
    }

    private DownloadStats downloadZipFromUrl(String urlText, boolean withAuth, File outputZip) throws IOException {
        long startMs = SystemClock.elapsedRealtime();
        long downloadedBytes = 0L;
        long totalBytes = -1L;
        long lastUiTick = 0L;

        HttpURLConnection connection = null;
        try {
            postUi(() -> {
                if (cancelled) {
                    return;
                }
                stageText.setText(withAuth
                    ? "Connecting with Basic Auth..."
                    : "Connecting to download host...");
                // Never the address or the download login - players see neither.
                detailText.setText("Please wait...");
                timerText.setText("");
            });

            URL url = new URL(urlText);
            connection = (HttpURLConnection) url.openConnection();
            connection.setConnectTimeout(15000);
            connection.setReadTimeout(25000);
            connection.setRequestProperty("Accept-Encoding", "identity");
            connection.setRequestProperty("User-Agent", "MuMain-Android-Preload/1.0");
            connection.setInstanceFollowRedirects(true);
            if (withAuth) {
                applyBasicAuthorization(connection, BASIC_AUTH_USERNAME, BASIC_AUTH_PASSWORD);
            }
            connection.connect();

            int responseCode = connection.getResponseCode();
            if (responseCode < 200 || responseCode >= 300) {
                throw new HttpStatusException(
                    responseCode,
                    "HTTP " + responseCode + " while downloading data.zip (" + urlText + ")");
            }

            totalBytes = connection.getContentLengthLong();
            if (totalBytes == 0) {
                throw new IOException("Server returned empty data.zip");
            }

            try (InputStream rawStream = connection.getInputStream();
                 BufferedInputStream inputStream = new BufferedInputStream(rawStream, BUFFER_SIZE);
                 BufferedOutputStream outputStream = new BufferedOutputStream(new FileOutputStream(outputZip, false), BUFFER_SIZE)) {

                byte[] buffer = new byte[BUFFER_SIZE];
                int read;
                while ((read = inputStream.read(buffer)) != -1) {
                    checkCancelled();
                    outputStream.write(buffer, 0, read);
                    downloadedBytes += read;

                    long now = SystemClock.elapsedRealtime();
                    if (now - lastUiTick >= UI_UPDATE_INTERVAL_MS) {
                        final long elapsedMs = Math.max(1L, now - startMs);
                        double stageProgress = totalBytes > 0
                            ? Math.min(1.0, downloadedBytes / (double) totalBytes)
                            : 0.0;
                        double overallPercent = stageProgress * DOWNLOAD_WEIGHT;
                        updateDownloadUi(downloadedBytes, totalBytes, elapsedMs, overallPercent);
                        lastUiTick = now;
                    }
                }
                outputStream.flush();
            }
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }

        long durationMs = SystemClock.elapsedRealtime() - startMs;
        updateDownloadUi(downloadedBytes, totalBytes, Math.max(1L, durationMs), DOWNLOAD_WEIGHT);
        return new DownloadStats(downloadedBytes, totalBytes, durationMs);
    }

    private static void applyBasicAuthorization(HttpURLConnection connection, String username, String password) {
        if (connection == null) {
            return;
        }
        String tokenRaw = username + ":" + password;
        String token = Base64.encodeToString(tokenRaw.getBytes(), Base64.NO_WRAP);
        connection.setRequestProperty("Authorization", "Basic " + token);
    }

    /**
     * Opens data.zip in a way that cannot fail on its entry names.
     *
     * The archive carries at least one name that is CP949 Korean rather than
     * UTF-8 - Data/Object34/Object01_<0xB0 0xA6>.bmd. ZipFile's single-argument
     * constructor decodes names as UTF-8, and 0xB0 is a continuation byte with
     * no lead byte, so it throws IllegalArgumentException("MALFORMED[1]") and
     * takes the whole preload down. That only shows up on a device doing a
     * fresh extract; anywhere the data is already present the step is skipped
     * and the archive is never opened.
     *
     * ISO-8859-1 maps all 256 byte values, so no name can fail to decode. The
     * odd name round-trips to mojibake on disk, which is harmless - it is an
     * unused leftover model, and every file the game actually opens is ASCII.
     */
    private static ZipFile openDataZip(File zipFile) throws IOException {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            return new ZipFile(zipFile, StandardCharsets.ISO_8859_1);
        }
        // Pre-24 ZipFile decodes names leniently and does not throw here.
        return new ZipFile(zipFile);
    }

    private ZipMetrics inspectZip(File zipFile) throws IOException {
        long totalBytes = 0L;
        int fileCount = 0;

        try (ZipFile zip = openDataZip(zipFile)) {
            Enumeration<? extends ZipEntry> entries = zip.entries();
            while (entries.hasMoreElements()) {
                ZipEntry entry = entries.nextElement();
                if (entry.isDirectory()) {
                    continue;
                }
                fileCount++;
                long size = entry.getSize();
                if (size > 0) {
                    totalBytes += size;
                }
            }
        }

        return new ZipMetrics(totalBytes, fileCount);
    }

    private void extractZipCaseInsensitive(File zipFile, File extractRoot, ZipMetrics metrics) throws IOException {
        final long parseStartMs = SystemClock.elapsedRealtime();
        long extractedBytes = 0L;
        int extractedFiles = 0;
        long lastUiTick = 0L;

        try (ZipFile zip = openDataZip(zipFile)) {
            Enumeration<? extends ZipEntry> entries = zip.entries();
            byte[] buffer = new byte[BUFFER_SIZE];

            while (entries.hasMoreElements()) {
                checkCancelled();
                ZipEntry entry = entries.nextElement();
                String normalizedName = normalizeZipPath(entry.getName());
                if (normalizedName.isEmpty()) {
                    continue;
                }

                boolean isDirectory = entry.isDirectory() || normalizedName.endsWith("/");
                File outFile = resolvePathCaseInsensitive(extractRoot, normalizedName, isDirectory);
                assertUnderRoot(extractRoot, outFile);

                if (isDirectory) {
                    ensureDirectory(outFile);
                    continue;
                }

                File parent = outFile.getParentFile();
                if (parent != null) {
                    ensureDirectory(parent);
                }

                try (InputStream inputStream = new BufferedInputStream(zip.getInputStream(entry), BUFFER_SIZE);
                     OutputStream outputStream = new BufferedOutputStream(new FileOutputStream(outFile, false), BUFFER_SIZE)) {
                    int read;
                    while ((read = inputStream.read(buffer)) != -1) {
                        checkCancelled();
                        outputStream.write(buffer, 0, read);
                        extractedBytes += read;

                        long now = SystemClock.elapsedRealtime();
                        if (now - lastUiTick >= UI_UPDATE_INTERVAL_MS) {
                            final long elapsedMs = Math.max(1L, now - parseStartMs);
                            double stageProgress = computeParseProgress(extractedBytes, extractedFiles, metrics);
                            double overallPercent = DOWNLOAD_WEIGHT + (stageProgress * PARSE_WEIGHT * 0.95);
                            updateExtractUi(extractedBytes, extractedFiles, metrics, elapsedMs, overallPercent);
                            lastUiTick = now;
                        }
                    }
                    outputStream.flush();
                }

                extractedFiles++;
            }
        }

        final long parseElapsedMs = Math.max(1L, SystemClock.elapsedRealtime() - parseStartMs);
        updateExtractUi(extractedBytes, extractedFiles, metrics, parseElapsedMs, DOWNLOAD_WEIGHT + (PARSE_WEIGHT * 0.95));
        updateInstallUi("Installing parsed data into old folder...");
    }

    private void installExtractedData(File extractRoot, File externalRoot) throws IOException {
        checkCancelled();

        File extractedDataDir = findChildDirectoryIgnoreCase(extractRoot, DATA_FOLDER_NAME);
        if (extractedDataDir == null) {
            File[] directories = extractRoot.listFiles(File::isDirectory);
            if (directories != null && directories.length == 1) {
                extractedDataDir = directories[0];
            }
        }
        if (extractedDataDir == null || !extractedDataDir.exists()) {
            throw new IOException("data folder not found in downloaded data.zip");
        }

        File existingDataDir = findChildDirectoryIgnoreCase(externalRoot, DATA_FOLDER_NAME);
        String targetName = existingDataDir != null ? existingDataDir.getName() : DATA_FOLDER_NAME;
        File targetDataDir = new File(externalRoot, targetName);

        boolean targetCleared = false;
        if (existingDataDir != null && existingDataDir.exists()) {
            targetCleared = tryDeleteExistingDataDir(existingDataDir);
        } else if (targetDataDir.exists()) {
            targetCleared = tryDeleteExistingDataDir(targetDataDir);
        } else {
            targetCleared = true;
        }

        if (targetCleared && moveDirectory(extractedDataDir, targetDataDir)) {
            // Fast-path: replace whole folder in one move.
        } else {
            // Fallback: merge-copy when old folder cannot be deleted (owner/permission mismatch).
            copyDirectory(extractedDataDir, targetDataDir);
            deleteRecursivelyQuietly(extractedDataDir);
        }
        List<String> missingEntries = findMissingTakumiDataEntries(targetDataDir);
        if (!missingEntries.isEmpty()) {
            throw new IOException("downloaded Takumi data incomplete, missing: "
                + joinMissingEntries(missingEntries));
        }
        // Record what the server was serving, so the next launch can tell
        // whether this copy is still current instead of assuming it is.
        writeDataReadyMarker(externalRoot, targetDataDir, fetchRemoteDataSignature());

        postUi(() -> progressBar.setProgress(1000));
    }

    private boolean tryDeleteExistingDataDir(File dir) {
        try {
            deleteRecursively(dir);
            return true;
        } catch (IOException deleteError) {
            Log.w(TAG, "Unable to clean existing data dir, fallback to in-place overwrite: "
                + dir.getAbsolutePath() + " (" + deleteError.getMessage() + ")");
            return false;
        }
    }

    private static void deleteRecursivelyQuietly(File file) {
        try {
            deleteRecursively(file);
        } catch (IOException ignored) {
            // best effort cleanup for temporary extraction folder
        }
    }

    private boolean moveDirectory(File sourceDir, File targetDir) {
        if (sourceDir == null || targetDir == null) {
            return false;
        }
        File parent = targetDir.getParentFile();
        if (parent != null && !parent.exists() && !parent.mkdirs()) {
            return false;
        }
        return sourceDir.renameTo(targetDir);
    }

    private void copyDirectory(File sourceDir, File targetDir) throws IOException {
        ensureDirectory(targetDir);
        File[] children = sourceDir.listFiles();
        if (children == null) {
            return;
        }
        for (File child : children) {
            checkCancelled();
            File targetChild = resolvePathCaseInsensitive(targetDir, child.getName(), child.isDirectory());
            if (child.isDirectory()) {
                copyDirectory(child, targetChild);
            } else {
                copyFile(child, targetChild);
            }
        }
    }

    private void copyFile(File sourceFile, File targetFile) throws IOException {
        File parent = targetFile.getParentFile();
        if (parent != null) {
            ensureDirectory(parent);
        }
        try (InputStream input = new BufferedInputStream(new FileInputStream(sourceFile), BUFFER_SIZE);
             OutputStream output = new BufferedOutputStream(new FileOutputStream(targetFile, false), BUFFER_SIZE)) {
            byte[] buffer = new byte[BUFFER_SIZE];
            int read;
            while ((read = input.read(buffer)) != -1) {
                checkCancelled();
                output.write(buffer, 0, read);
            }
            output.flush();
        }
    }

    private static File findChildDirectoryIgnoreCase(File parent, String childName) {
        if (parent == null || childName == null) {
            return null;
        }
        File[] children = parent.listFiles();
        if (children == null) {
            return null;
        }
        for (File child : children) {
            if (child.isDirectory() && child.getName().equalsIgnoreCase(childName)) {
                return child;
            }
        }
        return null;
    }

    private boolean shouldSkipDownload(File existingDataDir, File externalRoot) {
        File marker = new File(externalRoot, DATA_READY_MARKER_FILE);
        if (isDataFolderUsable(existingDataDir)) {
            // Usable local data is not the same as current local data. This
            // check used to end here, so once a device had any complete data
            // folder it would never pick up a newer data.zip - the only way to
            // update was to clear app data and re-download the lot.
            //
            // Ask the server what it is serving now and compare it with what
            // was recorded when this folder was written. A missing stored
            // signature means the data predates this check, so it is refreshed
            // once and then tracked from then on.
            String storedSignature = readStoredDataSignature(externalRoot);
            String remoteSignature = fetchRemoteDataSignature();

            if (remoteSignature != null && !remoteSignature.equals(storedSignature)) {
                Log.i(TAG, "Remote data.zip changed, re-downloading. stored=" + storedSignature
                    + " remote=" + remoteSignature);
                return false;
            }

            // Unreachable server, or the host gave us nothing to compare on:
            // keep playing with what is on disk rather than blocking startup.
            if (remoteSignature == null) {
                Log.i(TAG, "Could not read remote data.zip signature, keeping local data.");
            }

            writeDataReadyMarker(externalRoot, existingDataDir, storedSignature);
            Log.i(TAG, "Skip download, usable data exists: " + existingDataDir.getAbsolutePath()
                + " markerWasPresent=" + marker.exists());
            return true;
        }

        Log.i(TAG, "Data not usable, download required. root=" + externalRoot.getAbsolutePath()
            + " marker=" + marker.exists());
        return false;
    }

    private boolean isDataFolderUsable(File dataDir) {
        if (dataDir == null || !dataDir.exists() || !dataDir.isDirectory()) {
            return false;
        }

        List<String> missingEntries = findMissingTakumiDataEntries(dataDir);
        if (!missingEntries.isEmpty()) {
            Log.i(TAG, "Takumi data incomplete at " + dataDir.getAbsolutePath()
                + ", missing: " + joinMissingEntries(missingEntries));
            return false;
        }

        int hintMatches = 0;
        for (String hint : DATA_DIR_HINTS) {
            File child = findChildDirectoryIgnoreCase(dataDir, hint);
            if (child != null && child.isDirectory()) {
                hintMatches++;
            }
        }
        if (hintMatches >= 2) {
            return true;
        }

        DataScanResult scanResult = quickScanDataFolder(dataDir, 3, 64);
        return scanResult.fileCount >= 10 && scanResult.totalBytes >= (256L * 1024L);
    }

    private static List<String> findMissingTakumiDataEntries(File dataDir) {
        List<String> missingEntries = new ArrayList<>();

        for (String requiredDir : TAKUMI_REQUIRED_DIRS) {
            File candidate = findPathIgnoreCase(dataDir, requiredDir);
            if (candidate == null || !candidate.isDirectory()) {
                missingEntries.add(requiredDir + "/");
            }
        }

        for (String requiredFile : TAKUMI_REQUIRED_FILES) {
            File candidate = findPathIgnoreCase(dataDir, requiredFile);
            if (candidate == null || !candidate.isFile() || candidate.length() <= 0L) {
                missingEntries.add(requiredFile);
            }
        }

        return missingEntries;
    }

    private static File findPathIgnoreCase(File rootDir, String relativePath) {
        String normalizedPath = normalizeZipPath(relativePath);
        if (rootDir == null || normalizedPath.isEmpty()) {
            return null;
        }

        File current = rootDir;
        String[] segments = normalizedPath.split("/");
        for (String segment : segments) {
            current = findChildIgnoreCase(current, segment);
            if (current == null) {
                return null;
            }
        }
        return current;
    }

    private static String joinMissingEntries(List<String> missingEntries) {
        if (missingEntries == null || missingEntries.isEmpty()) {
            return "";
        }

        int limit = Math.min(8, missingEntries.size());
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < limit; i++) {
            if (i > 0) {
                builder.append(", ");
            }
            builder.append(missingEntries.get(i));
        }
        if (missingEntries.size() > limit) {
            builder.append(" ... +").append(missingEntries.size() - limit).append(" more");
        }
        return builder.toString();
    }

    private static DataScanResult quickScanDataFolder(File dir, int maxDepth, int fileLimit) {
        DataScanResult result = new DataScanResult();
        quickScanRecursive(dir, 0, maxDepth, fileLimit, result);
        return result;
    }

    private static void quickScanRecursive(
        File dir,
        int depth,
        int maxDepth,
        int fileLimit,
        DataScanResult result) {

        if (dir == null || result.fileCount >= fileLimit) {
            return;
        }

        File[] children = dir.listFiles();
        if (children == null) {
            return;
        }

        for (File child : children) {
            if (result.fileCount >= fileLimit) {
                break;
            }

            if (child.isFile()) {
                long len = child.length();
                if (len > 0L) {
                    result.fileCount++;
                    result.totalBytes += len;
                }
                continue;
            }

            if (child.isDirectory() && depth < maxDepth) {
                quickScanRecursive(child, depth + 1, maxDepth, fileLimit, result);
            }
        }
    }

    // Identity of the data.zip the server is serving, cheaply: a HEAD request
    // for Content-Length plus whichever of ETag / Last-Modified the host sets.
    // Any change to any of those means the archive was replaced.
    //
    // Returns null when nothing usable could be read - offline, host down, or a
    // server that reports none of the three - and callers treat null as "cannot
    // tell", never as "changed", so a broken network can never wipe a working
    // install.
    private String fetchRemoteDataSignature() {
        for (String urlText : DATA_ZIP_URL_CANDIDATES) {
            for (boolean withAuth : new boolean[] { false, true }) {
                HttpURLConnection connection = null;
                try {
                    URL url = new URL(urlText);
                    connection = (HttpURLConnection) url.openConnection();
                    connection.setRequestMethod("HEAD");
                    connection.setConnectTimeout(8000);
                    connection.setReadTimeout(8000);
                    connection.setRequestProperty("Accept-Encoding", "identity");
                    connection.setRequestProperty("User-Agent", "MuMain-Android-Preload/1.0");
                    connection.setInstanceFollowRedirects(true);
                    if (withAuth) {
                        applyBasicAuthorization(connection, BASIC_AUTH_USERNAME, BASIC_AUTH_PASSWORD);
                    }
                    connection.connect();

                    int responseCode = connection.getResponseCode();
                    if (responseCode == HttpURLConnection.HTTP_UNAUTHORIZED && !withAuth) {
                        continue;
                    }
                    if (responseCode < 200 || responseCode >= 300) {
                        break;
                    }

                    long length = connection.getContentLengthLong();
                    String etag = connection.getHeaderField("ETag");
                    String lastModified = connection.getHeaderField("Last-Modified");

                    if (length < 0 && etag == null && lastModified == null) {
                        Log.i(TAG, "Host gave no length/etag/last-modified, cannot tell if data.zip changed.");
                        return null;
                    }

                    return "len=" + length
                        + ";etag=" + (etag == null ? "" : etag.trim())
                        + ";mod=" + (lastModified == null ? "" : lastModified.trim());
                } catch (IOException headError) {
                    Log.i(TAG, "HEAD failed for " + urlText + " (auth=" + withAuth + "): "
                        + headError.getMessage());
                } finally {
                    if (connection != null) {
                        connection.disconnect();
                    }
                }
            }
        }
        return null;
    }

    private String readStoredDataSignature(File externalRoot) {
        if (externalRoot == null) {
            return null;
        }

        File marker = new File(externalRoot, DATA_READY_MARKER_FILE);
        if (!marker.isFile()) {
            return null;
        }

        try {
            byte[] raw = new byte[(int) Math.min(marker.length(), 8192L)];
            try (FileInputStream input = new FileInputStream(marker)) {
                int read = input.read(raw);
                if (read <= 0) {
                    return null;
                }
                for (String line : new String(raw, 0, read, StandardCharsets.UTF_8).split("\n")) {
                    if (line.startsWith("signature=")) {
                        String value = line.substring("signature=".length()).trim();
                        return value.isEmpty() ? null : value;
                    }
                }
            }
        } catch (IOException readError) {
            Log.w(TAG, "Failed to read data marker: " + readError.getMessage());
        }
        return null;
    }

    private void writeDataReadyMarker(File externalRoot, File dataDir) {
        writeDataReadyMarker(externalRoot, dataDir, null);
    }

    private void writeDataReadyMarker(File externalRoot, File dataDir, String signature) {
        if (externalRoot == null || dataDir == null) {
            return;
        }

        File marker = new File(externalRoot, DATA_READY_MARKER_FILE);
        String payload = "ready_at=" + System.currentTimeMillis()
            + "\npath=" + dataDir.getAbsolutePath()
            + "\nsignature=" + (signature == null ? "" : signature) + "\n";
        try (OutputStream output = new FileOutputStream(marker, false)) {
            output.write(payload.getBytes(StandardCharsets.UTF_8));
            output.flush();
        } catch (IOException markerError) {
            Log.w(TAG, "Failed to write data marker: " + markerError.getMessage());
        }
    }

    private static String normalizeZipPath(String entryName) {
        if (entryName == null) {
            return "";
        }
        String path = entryName.replace('\\', '/');
        while (path.startsWith("/")) {
            path = path.substring(1);
        }

        String[] rawParts = path.split("/");
        List<String> safeParts = new ArrayList<>(rawParts.length);
        for (String part : rawParts) {
            if (part.isEmpty() || ".".equals(part)) {
                continue;
            }
            if ("..".equals(part)) {
                return "";
            }
            safeParts.add(part);
        }
        if (safeParts.isEmpty()) {
            return "";
        }
        StringBuilder builder = new StringBuilder();
        for (int i = 0; i < safeParts.size(); i++) {
            if (i > 0) {
                builder.append('/');
            }
            builder.append(safeParts.get(i));
        }
        return builder.toString();
    }

    private static File resolvePathCaseInsensitive(File rootDir, String relativePath, boolean asDirectory) throws IOException {
        String normalizedPath = normalizeZipPath(relativePath);
        if (normalizedPath.isEmpty()) {
            return rootDir;
        }

        String[] segments = normalizedPath.split("/");
        File current = rootDir;

        for (int i = 0; i < segments.length; i++) {
            String segment = segments[i];
            boolean last = i == segments.length - 1;
            File existing = findChildIgnoreCase(current, segment);
            File resolved = existing != null ? existing : new File(current, segment);

            if (!last || asDirectory) {
                ensureDirectory(resolved);
            } else {
                File parent = resolved.getParentFile();
                if (parent != null) {
                    ensureDirectory(parent);
                }
            }

            current = resolved;
        }

        return current;
    }

    private static File findChildIgnoreCase(File parent, String childName) {
        if (parent == null || childName == null) {
            return null;
        }
        File[] children = parent.listFiles();
        if (children == null) {
            return null;
        }
        for (File child : children) {
            if (child.getName().equalsIgnoreCase(childName)) {
                return child;
            }
        }
        return null;
    }

    private static void assertUnderRoot(File root, File candidate) throws IOException {
        File canonicalRoot = root.getCanonicalFile();
        File canonicalCandidate = candidate.getCanonicalFile();
        String rootPath = canonicalRoot.getPath();
        String candidatePath = canonicalCandidate.getPath();
        if (!candidatePath.equals(rootPath) && !candidatePath.startsWith(rootPath + File.separator)) {
            throw new IOException("Invalid zip entry path: " + candidatePath);
        }
    }

    private static void recreateDirectory(File dir) throws IOException {
        deleteRecursively(dir);
        ensureDirectory(dir);
    }

    private static void ensureDirectory(File dir) throws IOException {
        if (dir == null) {
            throw new IOException("Directory is null");
        }
        if (dir.exists()) {
            if (!dir.isDirectory()) {
                throw new IOException("Path is not a directory: " + dir.getAbsolutePath());
            }
            return;
        }
        if (!dir.mkdirs() && !dir.isDirectory()) {
            throw new IOException("Failed to create directory: " + dir.getAbsolutePath());
        }
    }

    private static void deleteRecursively(File file) throws IOException {
        if (file == null || !file.exists()) {
            return;
        }
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children != null) {
                for (File child : children) {
                    deleteRecursively(child);
                }
            }
        }
        if (!file.delete() && file.exists()) {
            throw new IOException("Unable to delete: " + file.getAbsolutePath());
        }
    }

    private void checkCancelled() throws IOException {
        if (cancelled || Thread.currentThread().isInterrupted()) {
            throw new IOException("Preload cancelled");
        }
    }

    private double computeParseProgress(long extractedBytes, int extractedFiles, ZipMetrics metrics) {
        if (metrics.totalBytes > 0L) {
            return Math.min(1.0, extractedBytes / (double) metrics.totalBytes);
        }
        if (metrics.fileCount > 0) {
            return Math.min(1.0, extractedFiles / (double) metrics.fileCount);
        }
        return 1.0;
    }

    private void updateDownloadUi(long downloadedBytes, long totalBytes, long elapsedMs, double overallPercent) {
        updateDownloadUi(downloadedBytes, totalBytes, elapsedMs, overallPercent, -1.0);
    }

    // explicitSpeedBytesPerSecond < 0 keeps the old behaviour: bytes since the
    // start divided by time since the start. That is an average, not a rate,
    // and it is wrong for the parallel path in two ways - resumed chunks are
    // counted as having arrived in zero seconds, and any fast opening seconds
    // keep dragging the figure down for the rest of the transfer. A download
    // running at a perfectly steady rate would show a number falling the whole
    // way, which is exactly what it looked like.
    private void updateDownloadUi(long downloadedBytes, long totalBytes, long elapsedMs,
                                  double overallPercent, double explicitSpeedBytesPerSecond) {
        double speedBytesPerSecond = explicitSpeedBytesPerSecond >= 0.0
            ? explicitSpeedBytesPerSecond
            : downloadedBytes / Math.max(0.001, elapsedMs / 1000.0);
        long etaMs = 0L;
        if (totalBytes > 0 && speedBytesPerSecond > 0.0) {
            etaMs = (long) (((double) (totalBytes - downloadedBytes) / speedBytesPerSecond) * 1000.0);
            if (etaMs < 0L) {
                etaMs = 0L;
            }
        }

        final String stage = totalBytes > 0
            ? String.format(Locale.US, "Downloading data.zip %.2f%%",
                (downloadedBytes * 100.0) / Math.max(1L, totalBytes))
            : "Downloading data.zip";
        final String detail = totalBytes > 0
            ? "Downloaded " + formatBytes(downloadedBytes) + " / " + formatBytes(totalBytes)
            : "Downloaded " + formatBytes(downloadedBytes);
        final String timer = "Speed " + formatBytes((long) speedBytesPerSecond) + "/s"
            + " | ETA " + (totalBytes > 0 ? formatDuration(etaMs) : "--:--");

        updateStageUi(stage, detail, timer, overallPercent);
    }

    private void updateExtractUi(
        long extractedBytes,
        int extractedFiles,
        ZipMetrics metrics,
        long elapsedMs,
        double overallPercent) {

        double speedBytesPerSecond = extractedBytes / Math.max(0.001, elapsedMs / 1000.0);
        long etaMs = 0L;
        if (metrics.totalBytes > 0 && speedBytesPerSecond > 0.0) {
            etaMs = (long) (((double) (metrics.totalBytes - extractedBytes) / speedBytesPerSecond) * 1000.0);
            if (etaMs < 0L) {
                etaMs = 0L;
            }
        }

        double stagePercent = metrics.totalBytes > 0
            ? (extractedBytes * 100.0) / Math.max(1L, metrics.totalBytes)
            : (metrics.fileCount > 0 ? (extractedFiles * 100.0) / metrics.fileCount : 100.0);
        final String stage = String.format(Locale.US, "Parsing data folder %.2f%%", stagePercent);
        final String detail = metrics.totalBytes > 0
            ? "Parsed " + formatBytes(extractedBytes) + " / " + formatBytes(metrics.totalBytes)
            : "Parsed files " + extractedFiles + " / " + metrics.fileCount;
        final String timer = metrics.totalBytes > 0
            ? ("Speed " + formatBytes((long) speedBytesPerSecond) + "/s | ETA " + formatDuration(etaMs))
            : ("Elapsed " + formatDuration(elapsedMs));

        updateStageUi(stage, detail, timer, overallPercent);
    }

    private void updateInstallUi(String stage) {
        updateStageUi(stage, "Applying into device old data location...", "Finalizing...", 99.0);
    }

    private void updateStageUi(String stage, String detail, String timer, double overallPercent) {
        final int progressValue = toProgress(overallPercent);
        postUi(() -> {
            if (cancelled) {
                return;
            }
            stageText.setText(hideAddress(stage));
            detailText.setText(hideAddress(detail));
            timerText.setText(hideAddress(timer));
            progressBar.setProgress(progressValue);
        });
    }

    private int toProgress(double percent) {
        double safe = Math.max(0.0, Math.min(100.0, percent));
        return (int) Math.round(safe * 10.0);
    }

    private void postUi(Runnable uiWork) {
        mainHandler.post(uiWork);
    }

    private void launchGame() {
        if (cancelled) {
            return;
        }
        Intent intent = new Intent(this, MuMainNativeActivity.class);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        startActivity(intent);
        finish();
    }

    private static String formatDuration(long millis) {
        long totalSeconds = Math.max(0L, millis / 1000L);
        long hours = totalSeconds / 3600L;
        long minutes = (totalSeconds % 3600L) / 60L;
        long seconds = totalSeconds % 60L;

        if (hours > 0) {
            return String.format(Locale.US, "%d:%02d:%02d", hours, minutes, seconds);
        }
        return String.format(Locale.US, "%02d:%02d", minutes, seconds);
    }

    private static String formatBytes(long bytes) {
        if (bytes < 1024L) {
            return bytes + " B";
        }

        final String[] units = {"KB", "MB", "GB", "TB"};
        double value = bytes / 1024.0;
        int unitIndex = 0;
        while (value >= 1024.0 && unitIndex < units.length - 1) {
            value /= 1024.0;
            unitIndex++;
        }
        return String.format(Locale.US, "%.2f %s", value, units[unitIndex]);
    }

    private static final class DownloadStats {
        final long downloadedBytes;
        final long totalBytes;
        final long durationMs;

        DownloadStats(long downloadedBytes, long totalBytes, long durationMs) {
            this.downloadedBytes = downloadedBytes;
            this.totalBytes = totalBytes;
            this.durationMs = durationMs;
        }
    }

    private static final class ZipMetrics {
        final long totalBytes;
        final int fileCount;

        ZipMetrics(long totalBytes, int fileCount) {
            this.totalBytes = totalBytes;
            this.fileCount = fileCount;
        }
    }

    private static final class DataScanResult {
        int fileCount;
        long totalBytes;
    }

    // What the player sees when the sync fails: a short code instead of the
    // exception text, which carries the server's address and port. The full
    // message still goes to the log (Log.e / Log.w above).
    //   E101 host name not resolved       E102 cannot connect (down / port closed)
    //   E103 timed out                    E104 secure connection failed
    //   E4xx / E5xx the server's HTTP status
    //   E301 data empty or incomplete     E302 archive damaged
    //   E303 cannot write to storage      E199 anything else
    private static String errorCode(Throwable ex) {
        if (ex == null) {
            return "E199";
        }
        if (ex instanceof HttpStatusException) {
            return "E" + ((HttpStatusException) ex).statusCode;
        }
        if (ex instanceof java.net.UnknownHostException) {
            return "E101";
        }
        if (ex instanceof java.net.ConnectException || ex instanceof java.net.NoRouteToHostException) {
            return "E102";
        }
        if (ex instanceof java.net.SocketTimeoutException) {
            return "E103";
        }
        if (ex instanceof javax.net.ssl.SSLException) {
            return "E104";
        }
        if (ex instanceof java.util.zip.ZipException) {
            return "E302";
        }
        final String message = (ex.getMessage() != null) ? ex.getMessage().toLowerCase(Locale.US) : "";
        if (message.contains("empty") || message.contains("incomplete") || message.contains("not found in downloaded")) {
            return "E301";
        }
        if (message.contains("invalid zip")) {
            return "E302";
        }
        if (message.contains("writable") || message.contains("create directory") || message.contains("unable to delete")
                || message.contains("finalise") || message.contains("not a directory")) {
            return "E303";
        }
        if (ex.getCause() != null && ex.getCause() != ex) {
            return errorCode(ex.getCause());
        }
        return "E199";
    }

    // Safety net for every status line: no address or port reaches the screen,
    // whatever a message happens to contain.
    private static String hideAddress(String text) {
        if (text == null) {
            return "";
        }
        return text
            .replaceAll("(?i)https?://\\S+", "server")
            .replaceAll("\\b\\d{1,3}(\\.\\d{1,3}){3}(:\\d+)?\\b", "server");
    }

    private static final class HttpStatusException extends IOException {
        final int statusCode;

        HttpStatusException(int statusCode, String message) {
            super(message);
            this.statusCode = statusCode;
        }
    }
}
