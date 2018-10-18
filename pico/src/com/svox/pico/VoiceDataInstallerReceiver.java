/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.svox.pico;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import android.util.Log;

/*
 * Is notified when the language pack installer is added to the system, and runs the installer.
 */
public class VoiceDataInstallerReceiver extends BroadcastReceiver {

    private final static String TAG = "RunVoiceDataInstaller";

    private final static String INSTALLER_PACKAGE = "com.svox.langpack.installer";

    @Override
    public void onReceive(Context context, Intent intent) {
        if (Intent.ACTION_PACKAGE_ADDED.equals(intent.getAction())
                && INSTALLER_PACKAGE.equals(getPackageName(intent))) {
            Log.v(TAG, INSTALLER_PACKAGE + " package was added, running installer...");
            // the language pack installer has been added to the system
            // now run the installer
            Intent runIntent = new Intent("com.svox.langpack.installer.RUN_TTS_DATA_INSTALLER");
            runIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(runIntent);
        }
    }

    /**
     * Returns the name of the package that was added from the intent.
     * @param intent the {@link Intent}
     */
    private static String getPackageName(Intent intent) {
        return intent.getData().getSchemeSpecificPart();
    }
}
