/*
 * Copyright (C) 2009-2010 Google Inc.
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

#include <stdio.h>
#include <unistd.h>

#define LOG_TAG "SynthProxyJNI"

#include <utils/Log.h>
#include <nativehelper/jni.h>
#include <nativehelper/JNIHelp.h>
#include <android_runtime/AndroidRuntime.h>
#include <media/AudioTrack.h>
#include <math.h>

#include <dlfcn.h>

#include "tts.h"

#define DEFAULT_TTS_RATE        16000
#define DEFAULT_TTS_BUFFERSIZE  2048

// EQ + BOOST parameters
#define FILTER_LOWSHELF_ATTENUATION -18.0f // in dB
#define FILTER_TRANSITION_FREQ 1100.0f     // in Hz
#define FILTER_SHELF_SLOPE 1.0f            // Q
#define FILTER_GAIN 5.5f // linear gain

// android.media.AudioFormat.ENCODING_ values
//
// Note that these constants are different from those
// defined in the native code (system/audio.h and others).
// We use them because we use a Java AudioTrack to play
// back our data.
#define AUDIO_FORMAT_ENCODING_DEFAULT 1
#define AUDIO_FORMAT_ENCODING_PCM_16_BIT 2
#define AUDIO_FORMAT_ENCODING_PCM_8_BIT 3

using namespace android;

// ----------------------------------------------------------------------------
// EQ data
static double m_fa, m_fb, m_fc, m_fd, m_fe;
static double x0;  // x[n]
static double x1;  // x[n-1]
static double x2;  // x[n-2]
static double out0;// y[n]
static double out1;// y[n-1]
static double out2;// y[n-2]

static float fFilterLowshelfAttenuation = FILTER_LOWSHELF_ATTENUATION;
static float fFilterTransitionFreq = FILTER_TRANSITION_FREQ;
static float fFilterShelfSlope = FILTER_SHELF_SLOPE;
static float fFilterGain = FILTER_GAIN;
static bool  bUseFilter = false;

void initializeEQ() {
    double amp = float(pow(10.0, fFilterLowshelfAttenuation / 40.0));
    double w = 2.0 * M_PI * (fFilterTransitionFreq / DEFAULT_TTS_RATE);
    double sinw = float(sin(w));
    double cosw = float(cos(w));
    double beta = float(sqrt(amp)/fFilterShelfSlope);

    // initialize low-shelf parameters
    double b0 = amp * ((amp+1.0F) - ((amp-1.0F)*cosw) + (beta*sinw));
    double b1 = 2.0F * amp * ((amp-1.0F) - ((amp+1.0F)*cosw));
    double b2 = amp * ((amp+1.0F) - ((amp-1.0F)*cosw) - (beta*sinw));
    double a0 = (amp+1.0F) + ((amp-1.0F)*cosw) + (beta*sinw);
    double a1 = 2.0F * ((amp-1.0F) + ((amp+1.0F)*cosw));
    double a2 = -((amp+1.0F) + ((amp-1.0F)*cosw) - (beta*sinw));

    m_fa = fFilterGain * b0/a0;
    m_fb = fFilterGain * b1/a0;
    m_fc = fFilterGain * b2/a0;
    m_fd = a1/a0;
    m_fe = a2/a0;
}

void initializeFilter() {
    x0 = 0.0f;
    x1 = 0.0f;
    x2 = 0.0f;
    out0 = 0.0f;
    out1 = 0.0f;
    out2 = 0.0f;
}

void applyFilter(int16_t* buffer, size_t sampleCount) {

    for (size_t i=0 ; i<sampleCount ; i++) {

        x0 = (double) buffer[i];

        out0 = (m_fa*x0) + (m_fb*x1) + (m_fc*x2) + (m_fd*out1) + (m_fe*out2);

        x2 = x1;
        x1 = x0;

        out2 = out1;
        out1 = out0;

        if (out0 > 32767.0f) {
            buffer[i] = 32767;
        } else if (out0 < -32768.0f) {
            buffer[i] = -32768;
        } else {
            buffer[i] = (int16_t) out0;
        }
    }
}


// ----------------------------------------------------------------------------

static jmethodID synthesisRequest_start;
static jmethodID synthesisRequest_audioAvailable;
static jmethodID synthesisRequest_done;

static Mutex engineMutex;



typedef android_tts_engine_t *(*android_tts_entrypoint)();

// ----------------------------------------------------------------------------
class SynthProxyJniStorage {
  public:
    android_tts_engine_t *mEngine;
    void *mEngineLibHandle;
    int8_t *mBuffer;
    size_t mBufferSize;

    SynthProxyJniStorage() {
        mEngine = NULL;
        mEngineLibHandle = NULL;
        mBufferSize = DEFAULT_TTS_BUFFERSIZE;
        mBuffer = new int8_t[mBufferSize];
        memset(mBuffer, 0, mBufferSize);
    }

    ~SynthProxyJniStorage() {
        if (mEngine) {
            mEngine->funcs->shutdown(mEngine);
            mEngine = NULL;
        }
        if (mEngineLibHandle) {
            int res = dlclose(mEngineLibHandle);
            ALOGE_IF( res != 0, "~SynthProxyJniStorage(): dlclose returned %d", res);
        }
        delete[] mBuffer;
    }

};

// ----------------------------------------------------------------------------

struct SynthRequestData {
    SynthProxyJniStorage *jniStorage;
    JNIEnv *env;
    jobject request;
    bool startCalled;
};

// ----------------------------------------------------------------------------

/*
 * Calls into Java
 */

static bool checkException(JNIEnv *env)
{
    jthrowable ex = env->ExceptionOccurred();
    if (ex == NULL) {
        return false;
    }
    env->ExceptionClear();
    LOGE_EX(env, ex);
    env->DeleteLocalRef(ex);
    return true;
}

static int callRequestStart(JNIEnv *env, jobject request,
        uint32_t rate, android_tts_audio_format_t format, int channelCount)
{
    int encoding;

    switch (format) {
    case ANDROID_TTS_AUDIO_FORMAT_DEFAULT:
        encoding = AUDIO_FORMAT_ENCODING_DEFAULT;
        break;
    case ANDROID_TTS_AUDIO_FORMAT_PCM_8_BIT:
        encoding = AUDIO_FORMAT_ENCODING_PCM_8_BIT;
        break;
    case ANDROID_TTS_AUDIO_FORMAT_PCM_16_BIT:
        encoding = AUDIO_FORMAT_ENCODING_PCM_16_BIT;
        break;
    default:
        ALOGE("Can't play, bad format");
        return ANDROID_TTS_FAILURE;
    }

    int result = env->CallIntMethod(request, synthesisRequest_start, rate, encoding, channelCount);
    if (checkException(env)) {
        return ANDROID_TTS_FAILURE;
    }
    return result;
}

static int callRequestAudioAvailable(JNIEnv *env, jobject request, int8_t *buffer,
        int offset, int length)
{
    // TODO: Not nice to have to copy the buffer. Use ByteBuffer?
    jbyteArray javaBuffer = env->NewByteArray(length);
    if (javaBuffer == NULL) {
        ALOGE("Failed to allocate byte array");
        return ANDROID_TTS_FAILURE;
    }

    env->SetByteArrayRegion(javaBuffer, 0, length, static_cast<jbyte *>(buffer + offset));
    if (checkException(env)) {
        env->DeleteLocalRef(javaBuffer);
        return ANDROID_TTS_FAILURE;
    }
    int result = env->CallIntMethod(request, synthesisRequest_audioAvailable,
            javaBuffer, offset, length);
    if (checkException(env)) {
        env->DeleteLocalRef(javaBuffer);
        return ANDROID_TTS_FAILURE;
    }
    env->DeleteLocalRef(javaBuffer);
    return result;
}

static int callRequestDone(JNIEnv *env, jobject request)
{
    int result = env->CallIntMethod(request, synthesisRequest_done);
    if (checkException(env)) {
        return ANDROID_TTS_FAILURE;
    }
    return result;
}

/*
 * Callback from TTS engine.
 */
extern "C" android_tts_callback_status_t
__ttsSynthDoneCB(void **pUserdata, uint32_t rate,
               android_tts_audio_format_t format, int channelCount,
               int8_t **pWav, size_t *pBufferSize,
               android_tts_synth_status_t status)
{
    if (*pUserdata == NULL){
        ALOGE("userdata == NULL");
        return ANDROID_TTS_CALLBACK_HALT;
    }

    SynthRequestData *pRequestData = static_cast<SynthRequestData*>(*pUserdata);
    SynthProxyJniStorage *pJniData = pRequestData->jniStorage;
    JNIEnv *env = pRequestData->env;

    if (*pWav != NULL && *pBufferSize > 0) {
        if (bUseFilter) {
            applyFilter(reinterpret_cast<int16_t*>(*pWav), *pBufferSize/2);
        }

        if (!pRequestData->startCalled) {
            // TODO: is encoding one of the AudioFormat.ENCODING_* constants?
            pRequestData->startCalled = true;
            if (callRequestStart(env, pRequestData->request, rate, format, channelCount)
                    != ANDROID_TTS_SUCCESS) {
                return ANDROID_TTS_CALLBACK_HALT;
            }
        }

        if (callRequestAudioAvailable(env, pRequestData->request, *pWav, 0, *pBufferSize)
                != ANDROID_TTS_SUCCESS) {
            return ANDROID_TTS_CALLBACK_HALT;
        }

        memset(*pWav, 0, *pBufferSize);
    }

    if (pWav == NULL || status == ANDROID_TTS_SYNTH_DONE) {
        callRequestDone(env, pRequestData->request);
        env->DeleteGlobalRef(pRequestData->request);
        delete pRequestData;
        pRequestData = NULL;
        return ANDROID_TTS_CALLBACK_HALT;
    }

    *pBufferSize = pJniData->mBufferSize;

    return ANDROID_TTS_CALLBACK_CONTINUE;
}


// ----------------------------------------------------------------------------
static jint
com_android_tts_compat_SynthProxy_setLowShelf(JNIEnv *env, jobject thiz, jboolean applyFilter,
        jfloat filterGain, jfloat attenuationInDb, jfloat freqInHz, jfloat slope)
{
    bUseFilter = applyFilter;
    if (applyFilter) {
        fFilterLowshelfAttenuation = attenuationInDb;
        fFilterTransitionFreq = freqInHz;
        fFilterShelfSlope = slope;
        fFilterGain = filterGain;

        if (fFilterShelfSlope != 0.0f) {
            initializeEQ();
        } else {
            ALOGE("Invalid slope, can't be null");
            return ANDROID_TTS_FAILURE;
        }
    }

    return ANDROID_TTS_SUCCESS;
}

// ----------------------------------------------------------------------------
static jlong
com_android_tts_compat_SynthProxy_native_setup(JNIEnv *env, jobject thiz,
        jstring nativeSoLib, jstring engConfig)
{
    jlong result = 0;
    bUseFilter = false;

    const char *nativeSoLibNativeString =  env->GetStringUTFChars(nativeSoLib, 0);
    const char *engConfigString = env->GetStringUTFChars(engConfig, 0);

    void *engine_lib_handle = dlopen(nativeSoLibNativeString,
            RTLD_NOW | RTLD_LOCAL);
    if (engine_lib_handle == NULL) {
        ALOGE("com_android_tts_compat_SynthProxy_native_setup(): engine_lib_handle == NULL");
    } else {
        android_tts_entrypoint get_TtsEngine =
            reinterpret_cast<android_tts_entrypoint>(dlsym(engine_lib_handle, "android_getTtsEngine"));

        // Support obsolete/legacy binary modules
        if (get_TtsEngine == NULL) {
            get_TtsEngine =
                reinterpret_cast<android_tts_entrypoint>(dlsym(engine_lib_handle, "getTtsEngine"));
        }

        android_tts_engine_t *engine = (*get_TtsEngine)();
        if (engine) {
            Mutex::Autolock l(engineMutex);
            engine->funcs->init(engine, __ttsSynthDoneCB, engConfigString);

            SynthProxyJniStorage *pSynthData = new SynthProxyJniStorage();
            pSynthData->mEngine = engine;
            pSynthData->mEngineLibHandle = engine_lib_handle;
            result = reinterpret_cast<jlong>(pSynthData);
        }
    }

    env->ReleaseStringUTFChars(nativeSoLib, nativeSoLibNativeString);
    env->ReleaseStringUTFChars(engConfig, engConfigString);

    return result;
}

static SynthProxyJniStorage *getSynthData(jlong jniData)
{
    if (jniData == 0) {
        ALOGE("Engine not initialized");
        return NULL;
    }
    return reinterpret_cast<SynthProxyJniStorage *>(jniData);
}

static void
com_android_tts_compat_SynthProxy_native_finalize(JNIEnv *env, jobject thiz, jlong jniData)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return;
    }

    Mutex::Autolock l(engineMutex);

    delete pSynthData;
}

static void
com_android_tts_compat_SynthProxy_shutdown(JNIEnv *env, jobject thiz, jlong jniData)
{
    com_android_tts_compat_SynthProxy_native_finalize(env, thiz, jniData);
}

static jint
com_android_tts_compat_SynthProxy_isLanguageAvailable(JNIEnv *env, jobject thiz, jlong jniData,
        jstring language, jstring country, jstring variant)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return ANDROID_TTS_LANG_NOT_SUPPORTED;
    }

    android_tts_engine_t *engine = pSynthData->mEngine;
    if (!engine) {
        return ANDROID_TTS_LANG_NOT_SUPPORTED;
    }

    const char *langNativeString = env->GetStringUTFChars(language, 0);
    const char *countryNativeString = env->GetStringUTFChars(country, 0);
    const char *variantNativeString = env->GetStringUTFChars(variant, 0);

    int result = engine->funcs->isLanguageAvailable(engine, langNativeString,
            countryNativeString, variantNativeString);

    env->ReleaseStringUTFChars(language, langNativeString);
    env->ReleaseStringUTFChars(country, countryNativeString);
    env->ReleaseStringUTFChars(variant, variantNativeString);

    return (jint) result;
}

static jint
com_android_tts_compat_SynthProxy_setLanguage(JNIEnv *env, jobject thiz, jlong jniData,
        jstring language, jstring country, jstring variant)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return ANDROID_TTS_LANG_NOT_SUPPORTED;
    }

    Mutex::Autolock l(engineMutex);

    android_tts_engine_t *engine = pSynthData->mEngine;
    if (!engine) {
        return ANDROID_TTS_LANG_NOT_SUPPORTED;
    }

    const char *langNativeString = env->GetStringUTFChars(language, 0);
    const char *countryNativeString = env->GetStringUTFChars(country, 0);
    const char *variantNativeString = env->GetStringUTFChars(variant, 0);

    int result = engine->funcs->setLanguage(engine, langNativeString,
            countryNativeString, variantNativeString);

    env->ReleaseStringUTFChars(language, langNativeString);
    env->ReleaseStringUTFChars(country, countryNativeString);
    env->ReleaseStringUTFChars(variant, variantNativeString);

    return (jint) result;
}


static jint
com_android_tts_compat_SynthProxy_loadLanguage(JNIEnv *env, jobject thiz, jlong jniData,
        jstring language, jstring country, jstring variant)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return ANDROID_TTS_LANG_NOT_SUPPORTED;
    }

    android_tts_engine_t *engine = pSynthData->mEngine;
    if (!engine) {
        return ANDROID_TTS_LANG_NOT_SUPPORTED;
    }

    const char *langNativeString = env->GetStringUTFChars(language, 0);
    const char *countryNativeString = env->GetStringUTFChars(country, 0);
    const char *variantNativeString = env->GetStringUTFChars(variant, 0);

    int result = engine->funcs->loadLanguage(engine, langNativeString,
            countryNativeString, variantNativeString);

    env->ReleaseStringUTFChars(language, langNativeString);
    env->ReleaseStringUTFChars(country, countryNativeString);
    env->ReleaseStringUTFChars(variant, variantNativeString);

    return (jint) result;
}

static jint
com_android_tts_compat_SynthProxy_setProperty(JNIEnv *env, jobject thiz, jlong jniData,
        jstring name, jstring value)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return ANDROID_TTS_FAILURE;
    }

    Mutex::Autolock l(engineMutex);

    android_tts_engine_t *engine = pSynthData->mEngine;
    if (!engine) {
        return ANDROID_TTS_FAILURE;
    }

    const char *nameChars = env->GetStringUTFChars(name, 0);
    const char *valueChars = env->GetStringUTFChars(value, 0);
    size_t valueLength = env->GetStringUTFLength(value);

    int result = engine->funcs->setProperty(engine, nameChars, valueChars, valueLength);

    env->ReleaseStringUTFChars(name, nameChars);
    env->ReleaseStringUTFChars(name, valueChars);

    return (jint) result;
}

static jint
com_android_tts_compat_SynthProxy_speak(JNIEnv *env, jobject thiz, jlong jniData,
        jstring textJavaString, jobject request)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return ANDROID_TTS_FAILURE;
    }

    initializeFilter();

    Mutex::Autolock l(engineMutex);

    android_tts_engine_t *engine = pSynthData->mEngine;
    if (!engine) {
        return ANDROID_TTS_FAILURE;
    }

    SynthRequestData *pRequestData = new SynthRequestData;
    pRequestData->jniStorage = pSynthData;
    pRequestData->env = env;
    pRequestData->request = env->NewGlobalRef(request);
    pRequestData->startCalled = false;

    const char *textNativeString = env->GetStringUTFChars(textJavaString, 0);
    memset(pSynthData->mBuffer, 0, pSynthData->mBufferSize);

    int result = engine->funcs->synthesizeText(engine, textNativeString,
            pSynthData->mBuffer, pSynthData->mBufferSize, static_cast<void *>(pRequestData));
    env->ReleaseStringUTFChars(textJavaString, textNativeString);

    return (jint) result;
}

static jint
com_android_tts_compat_SynthProxy_stop(JNIEnv *env, jobject thiz, jlong jniData)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return ANDROID_TTS_FAILURE;
    }

    android_tts_engine_t *engine = pSynthData->mEngine;
    if (!engine) {
        return ANDROID_TTS_FAILURE;
    }

    return (jint) engine->funcs->stop(engine);
}

static jint
com_android_tts_compat_SynthProxy_stopSync(JNIEnv *env, jobject thiz, jlong jniData)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return ANDROID_TTS_FAILURE;
    }

    // perform a regular stop
    int result = com_android_tts_compat_SynthProxy_stop(env, thiz, jniData);
    // but wait on the engine having released the engine mutex which protects
    // the synthesizer resources.
    engineMutex.lock();
    engineMutex.unlock();

    return (jint) result;
}

static jobjectArray
com_android_tts_compat_SynthProxy_getLanguage(JNIEnv *env, jobject thiz, jlong jniData)
{
    SynthProxyJniStorage* pSynthData = getSynthData(jniData);
    if (pSynthData == NULL) {
        return NULL;
    }

    if (pSynthData->mEngine) {
        size_t bufSize = 100;
        char lang[bufSize];
        char country[bufSize];
        char variant[bufSize];
        memset(lang, 0, bufSize);
        memset(country, 0, bufSize);
        memset(variant, 0, bufSize);
        jobjectArray retLocale = (jobjectArray)env->NewObjectArray(3,
                env->FindClass("java/lang/String"), env->NewStringUTF(""));

        android_tts_engine_t *engine = pSynthData->mEngine;
        engine->funcs->getLanguage(engine, lang, country, variant);
        env->SetObjectArrayElement(retLocale, 0, env->NewStringUTF(lang));
        env->SetObjectArrayElement(retLocale, 1, env->NewStringUTF(country));
        env->SetObjectArrayElement(retLocale, 2, env->NewStringUTF(variant));
        return retLocale;
    } else {
        return NULL;
    }
}


// Dalvik VM type signatures
static JNINativeMethod gMethods[] = {
    {   "native_stop",
        "(J)I",
        (void*)com_android_tts_compat_SynthProxy_stop
    },
    {   "native_stopSync",
        "(J)I",
        (void*)com_android_tts_compat_SynthProxy_stopSync
    },
    {   "native_speak",
        "(JLjava/lang/String;Landroid/speech/tts/SynthesisCallback;)I",
        (void*)com_android_tts_compat_SynthProxy_speak
    },
    {   "native_isLanguageAvailable",
        "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
        (void*)com_android_tts_compat_SynthProxy_isLanguageAvailable
    },
    {   "native_setLanguage",
        "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
        (void*)com_android_tts_compat_SynthProxy_setLanguage
    },
    {   "native_loadLanguage",
        "(JLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)I",
        (void*)com_android_tts_compat_SynthProxy_loadLanguage
    },
    {   "native_setProperty",
        "(JLjava/lang/String;Ljava/lang/String;)I",
        (void*)com_android_tts_compat_SynthProxy_setProperty
    },
    {   "native_getLanguage",
        "(J)[Ljava/lang/String;",
        (void*)com_android_tts_compat_SynthProxy_getLanguage
    },
    {   "native_shutdown",
        "(J)V",
        (void*)com_android_tts_compat_SynthProxy_shutdown
    },
    {   "native_setup",
        "(Ljava/lang/String;Ljava/lang/String;)J",
        (void*)com_android_tts_compat_SynthProxy_native_setup
    },
    {   "native_setLowShelf",
        "(ZFFFF)I",
        (void*)com_android_tts_compat_SynthProxy_setLowShelf
    },
    {   "native_finalize",
        "(J)V",
        (void*)com_android_tts_compat_SynthProxy_native_finalize
    }
};

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env = NULL;

    if (vm->GetEnv((void**) &env, JNI_VERSION_1_4) != JNI_OK) {
        ALOGE("ERROR: GetEnv failed\n");
        return -1;
    }
    assert(env != NULL);

    jclass classSynthesisRequest = env->FindClass(
            "android/speech/tts/SynthesisCallback");
    if (classSynthesisRequest == NULL) {
        return -1;
    }

    synthesisRequest_start = env->GetMethodID(classSynthesisRequest,
            "start", "(III)I");
    if (synthesisRequest_start == NULL) {
        return -1;
    }

    synthesisRequest_audioAvailable = env->GetMethodID(classSynthesisRequest,
            "audioAvailable", "([BII)I");
    if (synthesisRequest_audioAvailable == NULL) {
        return -1;
    }

    synthesisRequest_done = env->GetMethodID(classSynthesisRequest,
            "done", "()I");
    if (synthesisRequest_done == NULL) {
        return -1;
    }

    if (jniRegisterNativeMethods(
            env, "com/android/tts/compat/SynthProxy", gMethods, NELEM(gMethods)) < 0) {
        return -1;
    }

    /* success -- return valid version number */
    return JNI_VERSION_1_4;
}
