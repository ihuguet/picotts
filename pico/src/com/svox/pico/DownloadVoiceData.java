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

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;

/*
 * Downloads the voice data for the SVOX Pico Engine by getting the language
 * pack from the Android Market.
 */
public class DownloadVoiceData extends Activity {
    private final static String MARKET_URI = "market://search?q=pname:com.svox.langpack.installer";

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Uri marketUri = Uri.parse(MARKET_URI);
        Intent marketIntent = new Intent(Intent.ACTION_VIEW, marketUri);
        startActivityForResult(marketIntent, 0);
        finish();
    }
}
