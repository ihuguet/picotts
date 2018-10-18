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

import java.io.File;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.speech.tts.TextToSpeech;

import android.util.Log;

/*
 * Returns the sample text string for the language requested
 */
public class GetSampleText extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        int result = TextToSpeech.LANG_AVAILABLE;
        Intent returnData = new Intent();

        Intent i = getIntent();
        String language = i.getExtras().getString("language");
        String country = i.getExtras().getString("country");
        String variant = i.getExtras().getString("variant");

        if (language.equals("eng")) {
          if (country.equals("GBR")){
            returnData.putExtra("sampleText", getString(R.string.eng_gbr_sample));
          } else {
            returnData.putExtra("sampleText", getString(R.string.eng_usa_sample));
          }
        } else if (language.equals("fra")) {
          returnData.putExtra("sampleText", getString(R.string.fra_fra_sample));
        } else if (language.equals("ita")) {
          returnData.putExtra("sampleText", getString(R.string.ita_ita_sample));
        } else if (language.equals("deu")) {
          returnData.putExtra("sampleText", getString(R.string.deu_deu_sample));
        } else if (language.equals("spa")) {
          returnData.putExtra("sampleText", getString(R.string.spa_esp_sample));
        } else {
          result = TextToSpeech.LANG_NOT_SUPPORTED;
          returnData.putExtra("sampleText", "");
        }

        setResult(result, returnData);
        finish();
    }
}
