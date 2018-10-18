/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not
 * use this file except in compliance with the License. You may obtain a copy of
 * the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations under
 * the License.
 */
package com.android.tts.compat;

import android.database.Cursor;
import android.net.Uri;
import android.speech.tts.SynthesisCallback;
import android.speech.tts.SynthesisRequest;
import android.speech.tts.TextToSpeech;
import android.speech.tts.TextToSpeechService;
import android.util.Log;

import java.io.File;

public abstract class CompatTtsService extends TextToSpeechService {

    private static final boolean DBG = false;
    private static final String TAG = "CompatTtsService";

    private SynthProxy mNativeSynth = null;

    protected abstract String getSoFilename();

    @Override
    public void onCreate() {
        if (DBG) Log.d(TAG, "onCreate()");

        String soFilename = getSoFilename();

        File f = new File(soFilename);
        if (!f.exists()) {
            Log.e(TAG, "Invalid TTS Binary: " + soFilename);
            return;
        }

        if (mNativeSynth != null) {
            mNativeSynth.stopSync();
            mNativeSynth.shutdown();
            mNativeSynth = null;
        }

        // Load the engineConfig from the plugin if it has any special configuration
        // to be loaded. By convention, if an engine wants the TTS framework to pass
        // in any configuration, it must put it into its content provider which has the URI:
        // content://<packageName>.providers.SettingsProvider
        // That content provider must provide a Cursor which returns the String that
        // is to be passed back to the native .so file for the plugin when getString(0) is
        // called on it.
        // Note that the TTS framework does not care what this String data is: it is something
        // that comes from the engine plugin and is consumed only by the engine plugin itself.
        String engineConfig = "";
        Cursor c = getContentResolver().query(Uri.parse("content://" + getPackageName()
                + ".providers.SettingsProvider"), null, null, null, null);
        if (c != null){
            c.moveToFirst();
            engineConfig = c.getString(0);
            c.close();
        }
        mNativeSynth = new SynthProxy(soFilename, engineConfig);

        // mNativeSynth is used by TextToSpeechService#onCreate so it must be set prior
        // to that call.
        // getContentResolver() is also moved prior to super.onCreate(), and it works
        // because the super method don't sets a field or value that affects getContentResolver();
        // (including the content resolver itself).
        super.onCreate();
    }

    @Override
    public void onDestroy() {
        if (DBG) Log.d(TAG, "onDestroy()");
        super.onDestroy();

        if (mNativeSynth != null) {
            mNativeSynth.shutdown();
        }
        mNativeSynth = null;
    }

    @Override
    protected String[] onGetLanguage() {
        if (mNativeSynth == null) return null;
        return mNativeSynth.getLanguage();
    }

    @Override
    protected int onIsLanguageAvailable(String lang, String country, String variant) {
        if (DBG) Log.d(TAG, "onIsLanguageAvailable(" + lang + "," + country + "," + variant + ")");
        if (mNativeSynth == null) return TextToSpeech.ERROR;
        return mNativeSynth.isLanguageAvailable(lang, country, variant);
    }

    @Override
    protected int onLoadLanguage(String lang, String country, String variant) {
        if (DBG) Log.d(TAG, "onLoadLanguage(" + lang + "," + country + "," + variant + ")");
        int result = onIsLanguageAvailable(lang, country, variant);
        if (result >= TextToSpeech.LANG_AVAILABLE) {
            mNativeSynth.setLanguage(lang, country, variant);
        }

        return result;
    }

    @Override
    protected void onSynthesizeText(SynthesisRequest request, SynthesisCallback callback) {
        if (mNativeSynth == null) {
            callback.error();
            return;
        }

        // Set language
        String lang = request.getLanguage();
        String country = request.getCountry();
        String variant = request.getVariant();
        if (mNativeSynth.setLanguage(lang, country, variant) != TextToSpeech.SUCCESS) {
            Log.e(TAG, "setLanguage(" + lang + "," + country + "," + variant + ") failed");
            callback.error();
            return;
        }

        // Set speech rate
        int speechRate = request.getSpeechRate();
        if (mNativeSynth.setSpeechRate(speechRate) != TextToSpeech.SUCCESS) {
            Log.e(TAG, "setSpeechRate(" + speechRate + ") failed");
            callback.error();
            return;
        }

        // Set speech
        int pitch = request.getPitch();
        if (mNativeSynth.setPitch(pitch) != TextToSpeech.SUCCESS) {
            Log.e(TAG, "setPitch(" + pitch + ") failed");
            callback.error();
            return;
        }

        // Synthesize
        if (mNativeSynth.speak(request, callback) != TextToSpeech.SUCCESS) {
            callback.error();
            return;
        }
    }

    @Override
    protected void onStop() {
        if (DBG) Log.d(TAG, "onStop()");
        if (mNativeSynth == null) return;
        mNativeSynth.stop();
    }

}
