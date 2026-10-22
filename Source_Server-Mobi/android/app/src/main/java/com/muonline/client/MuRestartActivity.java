package com.muonline.client;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

// Restarts the game process. It runs in its own process (":restart", see the manifest), so
// it is still alive after the game's process is killed: it starts the launch intent it was
// given, which a killed process could not do (Android 10+ blocks activity starts from the
// background), and then ends itself.
public class MuRestartActivity extends Activity {
    public static final String EXTRA_INTENT = "mu_restart_intent";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Intent next = getIntent().getParcelableExtra(EXTRA_INTENT);
        if (next != null) {
            startActivity(next);
        }
        finish();
        Runtime.getRuntime().exit(0);
    }
}
