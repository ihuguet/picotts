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

import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.Preference.OnPreferenceClickListener;
import android.speech.tts.TextToSpeech;
import android.util.Log;

import java.util.ArrayList;
import java.util.Locale;

/*
 * Checks if the voice data for the SVOX Pico Engine is present on the
 * sd card.
 */
public class EngineSettings extends PreferenceActivity {
    private final static String MARKET_URI_START = "market://search?q=pname:com.svox.pico.voice.";
    private static final int VOICE_DATA_CHECK_CODE = 42;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        
        Intent i = new Intent();
        i.setClass(this, CheckVoiceData.class);
        startActivityForResult(i, VOICE_DATA_CHECK_CODE);
    }
    
    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent data){
        if (requestCode == VOICE_DATA_CHECK_CODE){
            ArrayList<String> available = data.getStringArrayListExtra(TextToSpeech.Engine.EXTRA_AVAILABLE_VOICES);
            ArrayList<String> unavailable = data.getStringArrayListExtra(TextToSpeech.Engine.EXTRA_UNAVAILABLE_VOICES);

            addPreferencesFromResource(R.xml.voices_list);
            
            for (int i = 0; i < available.size(); i++){
                Log.e("debug", available.get(i));
                String[] languageCountry = available.get(i).split("-");
                Locale loc = new Locale(languageCountry[0], languageCountry[1]);
                Preference pref = findPreference(available.get(i));
                pref.setTitle(loc.getDisplayLanguage() + " (" + loc.getDisplayCountry() + ")");
                pref.setSummary(R.string.installed);
                pref.setEnabled(false);
            }

            
            for (int i = 0; i < unavailable.size(); i++){
                final String unavailableLang = unavailable.get(i);
                String[] languageCountry = unavailableLang.split("-");
                Locale loc = new Locale(languageCountry[0], languageCountry[1]);
                Preference pref = findPreference(unavailableLang);
                pref.setTitle(loc.getDisplayLanguage() + " (" + loc.getDisplayCountry() + ")");
                pref.setSummary(R.string.not_installed);
                pref.setEnabled(true);
                pref.setOnPreferenceClickListener(new OnPreferenceClickListener(){
                    public boolean onPreferenceClick(Preference preference) {
                        Uri marketUri = Uri.parse(MARKET_URI_START + unavailableLang.toLowerCase().replace("-", "."));
                        Intent marketIntent = new Intent(Intent.ACTION_VIEW, marketUri);
                        startActivity(marketIntent);
                        return false;
                    }
                });
            }
        }
    }
    

}
