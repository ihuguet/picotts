/* com_svox_picottsengine.cpp

 * Copyright (C) 2008-2009 SVOX AG, Baslerstr. 30, 8048 Zuerich, Switzerland
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *   This is the Manager layer.  It sits on top of the native Pico engine
 *   and provides the interface to the defined Google TTS engine API.
 *   The Google engine API is the boundary to allow a TTS engine to be swapped.
 *   The Manager layer also provide the SSML tag interpretation.
 *   The supported SSML tags are mapped to corresponding tags natively supported by Pico.
 *   Native Pico functions always begin with picoXXX.
 *
 *   In the Pico engine, the language cannot be changed indpendently of the voice.
 *   If either the voice or locale/language are changed, a new resource is loaded.
 *
 *   Only a subset of SSML 1.0 tags are supported.
 *   Some SSML tags involve significant complexity.
 *   If the language is changed through an SSML tag, there is a latency for the load.
 *
 */
//#define LOG_NDEBUG 0

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define LOG_TAG "SVOX Pico Engine"

#include <utils/Log.h>
#include <utils/String16.h>                     /* for strlen16 */
#include <android_runtime/AndroidRuntime.h>
#include <TtsEngine.h>

#include <cutils/jstring.h>
#include <picoapi.h>
#include <picodefs.h>

#include "svox_ssml_parser.h"

using namespace android;

/* adaptation layer defines */
#define PICO_MEM_SIZE       2500000
/* speaking rate    */
#define PICO_MIN_RATE        20
#define PICO_MAX_RATE       500
#define PICO_DEF_RATE       100
/* speaking pitch   */
#define PICO_MIN_PITCH       50
#define PICO_MAX_PITCH      200
#define PICO_DEF_PITCH      100
/* speaking volume  */
#define PICO_MIN_VOLUME       0
#define PICO_MAX_VOLUME     500
#define PICO_DEF_VOLUME     100

/* string constants */
#define MAX_OUTBUF_SIZE     128
const char * PICO_SYSTEM_LINGWARE_PATH      = "/system/tts/lang_pico/";
const char * PICO_LINGWARE_PATH             = "/sdcard/svox/";
const char * PICO_VOICE_NAME                = "PicoVoice";
const char * PICO_SPEED_OPEN_TAG            = "<speed level='%d'>";
const char * PICO_SPEED_CLOSE_TAG           = "</speed>";
const char * PICO_PITCH_OPEN_TAG            = "<pitch level='%d'>";
const char * PICO_PITCH_CLOSE_TAG           = "</pitch>";
const char * PICO_VOLUME_OPEN_TAG           = "<volume level='%d'>";
const char * PICO_VOLUME_CLOSE_TAG          = "</volume>";
const char * PICO_PHONEME_OPEN_TAG          = "<phoneme ph='";
const char * PICO_PHONEME_CLOSE_TAG         = "'/>";

/* supported voices
   Pico does not seperately specify the voice and locale.   */
const char * picoSupportedLangIso3[]        = { "eng",              "eng",              "deu",              "spa",              "fra",              "ita" };
const char * picoSupportedCountryIso3[]     = { "USA",              "GBR",              "DEU",              "ESP",              "FRA",              "ITA" };
const char * picoSupportedLang[]            = { "en-US",            "en-GB",            "de-DE",            "es-ES",            "fr-FR",            "it-IT" };
const char * picoInternalLang[]             = { "en-US",            "en-GB",            "de-DE",            "es-ES",            "fr-FR",            "it-IT" };
const char * picoInternalTaLingware[]       = { "en-US_ta.bin",     "en-GB_ta.bin",     "de-DE_ta.bin",     "es-ES_ta.bin",     "fr-FR_ta.bin",     "it-IT_ta.bin" };
const char * picoInternalSgLingware[]       = { "en-US_lh0_sg.bin", "en-GB_kh0_sg.bin", "de-DE_gl0_sg.bin", "es-ES_zl0_sg.bin", "fr-FR_nk0_sg.bin", "it-IT_cm0_sg.bin" };
const char * picoInternalUtppLingware[]     = { "en-US_utpp.bin",   "en-GB_utpp.bin",   "de-DE_utpp.bin",   "es-ES_utpp.bin",   "fr-FR_utpp.bin",   "it-IT_utpp.bin" };
const int picoNumSupportedVocs              = 6;

/* supported properties */
const char * picoSupportedProperties[]      = { "language", "rate", "pitch", "volume" };
const int    picoNumSupportedProperties     = 4;


/* adapation layer global variables */
synthDoneCB_t * picoSynthDoneCBPtr;
void *          picoMemArea         = NULL;
pico_System     picoSystem          = NULL;
pico_Resource   picoTaResource      = NULL;
pico_Resource   picoSgResource      = NULL;
pico_Resource   picoUtppResource    = NULL;
pico_Engine     picoEngine          = NULL;
pico_Char *     picoTaFileName      = NULL;
pico_Char *     picoSgFileName      = NULL;
pico_Char *     picoUtppFileName    = NULL;
pico_Char *     picoTaResourceName  = NULL;
pico_Char *     picoSgResourceName  = NULL;
pico_Char *     picoUtppResourceName = NULL;
int     picoSynthAbort = 0;
char *  picoProp_currLang   = NULL;                 /* current language */
int     picoProp_currRate   = PICO_DEF_RATE;        /* current rate     */
int     picoProp_currPitch  = PICO_DEF_PITCH;       /* current pitch    */
int     picoProp_currVolume = PICO_DEF_VOLUME;      /* current volume   */

int picoCurrentLangIndex = -1;

char * pico_alt_lingware_path = NULL;


/* internal helper functions */

/** checkForLocale
 *  Check whether the requested locale is among the supported locales.
 *  @locale -  the locale to check, either in xx or xx-YY format
 *  return index of the locale, or -1 if not supported.
*/
static int checkForLocale( const char * locale )
{
     int found = -1;                                         /* language not found   */
     int i;
     if (locale == NULL) {
        ALOGE("checkForLocale called with NULL language");
        return found;
     }

    /* Verify that the requested locale is a locale that we support.    */
    for (i = 0; i < picoNumSupportedVocs; i ++) {
        if (strcmp(locale, picoSupportedLang[i]) == 0) { /* in array */
            found = i;
            break;
        }
    };

    /* The exact locale was not found.    */
    if (found < 0) {
        /* We didn't find an exact match; it may have been specified with only the first 2 characters.
           This could overmatch ISO 639-3 language codes.%%                                   */

        /* check whether the current language matches the locale's language */
        if ((picoCurrentLangIndex > -1) &&
                (strncmp(locale, picoSupportedLang[picoCurrentLangIndex], 2) == 0)) {
            /* the current language matches the requested language, let's use it */
            found = picoCurrentLangIndex;
        } else {
            /* check whether we can find a match at least on the language */
            for (i = 0; i < picoNumSupportedVocs; i ++) {
                if (strncmp(locale, picoSupportedLang[i], 2) == 0) {
                    found = i;
                    break;
                }
            }
        }

        if (found < 0) {
            ALOGE("TtsEngine::set language called with unsupported locale %s", locale);
        }
    };
    return found;
}


/** cleanResources
 *  Unloads any loaded Pico resources.
*/
static void cleanResources( void )
{
    if (picoEngine) {
        pico_disposeEngine( picoSystem, &picoEngine );
        pico_releaseVoiceDefinition( picoSystem, (pico_Char *) PICO_VOICE_NAME );
        picoEngine = NULL;
    }
    if (picoUtppResource) {
        pico_unloadResource( picoSystem, &picoUtppResource );
        picoUtppResource = NULL;
    }
    if (picoTaResource) {
        pico_unloadResource( picoSystem, &picoTaResource );
        picoTaResource = NULL;
    }
    if (picoSgResource) {
        pico_unloadResource( picoSystem, &picoSgResource );
        picoSgResource = NULL;
    }

    if (picoSystem) {
        pico_terminate(&picoSystem);
        picoSystem = NULL;
    }
    picoCurrentLangIndex = -1;
}


/** cleanFiles
 *  Frees any memory allocated for file and resource strings.
*/
static void cleanFiles( void )
{
    if (picoProp_currLang) {
        free( picoProp_currLang );
        picoProp_currLang = NULL;
    }

    if (picoTaFileName) {
        free( picoTaFileName );
        picoTaFileName = NULL;
    }

    if (picoSgFileName) {
        free( picoSgFileName );
        picoSgFileName = NULL;
    }

    if (picoUtppFileName) {
        free( picoUtppFileName );
        picoUtppFileName = NULL;
    }

    if (picoTaResourceName) {
        free( picoTaResourceName );
        picoTaResourceName = NULL;
    }

    if (picoSgResourceName) {
        free( picoSgResourceName );
        picoSgResourceName = NULL;
    }

    if (picoUtppResourceName) {
        free( picoUtppResourceName );
        picoUtppResourceName = NULL;
    }
}

/** hasResourcesForLanguage
 *  Check to see if the resources required to load the language at the specified index
 *  are properly installed
 *  @langIndex - the index of the language to check the resources for. The index is valid.
 *  return true if the required resources are installed, false otherwise
 */
static bool hasResourcesForLanguage(int langIndex) {
    FILE * pFile;
    char* fileName = (char*)malloc(PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE);

    /* check resources on system (under PICO_SYSTEM_LINGWARE_PATH). */
    strcpy((char*)fileName, PICO_SYSTEM_LINGWARE_PATH);
    strcat((char*)fileName, (const char*)picoInternalTaLingware[langIndex]);
    pFile = fopen(fileName, "r");
    if (pFile != NULL) {
        /* "ta" file found. */
        fclose (pFile);
        /* now look for "sg" file. */
        strcpy((char*)fileName, PICO_SYSTEM_LINGWARE_PATH);
        strcat((char*)fileName, (const char*)picoInternalSgLingware[langIndex]);
        pFile = fopen(fileName, "r");
        if (pFile != NULL) {
            /* "sg" file found, no need to continue checking, return success. */
            fclose(pFile);
            free(fileName);
            return true;
        }
    }

    /* resources not found on system, check resources on alternative location */
    /* (under pico_alt_lingware_path).                                            */
    strcpy((char*)fileName, pico_alt_lingware_path);
    strcat((char*)fileName, (const char*)picoInternalTaLingware[langIndex]);
    pFile = fopen(fileName, "r");
    if (pFile == NULL) {
        free(fileName);
        return false;
    } else {
        fclose (pFile);
    }

    strcpy((char*)fileName, pico_alt_lingware_path);
    strcat((char*)fileName, (const char*)picoInternalSgLingware[langIndex]);
    pFile = fopen(fileName, "r");
    if (pFile == NULL) {
        free(fileName);
        return false;
    } else {
        fclose(pFile);
        free(fileName);
        return true;
    }
}

/** doLanguageSwitchFromLangIndex
 *  Switch to the requested locale.
 *  If the locale is already loaded, it returns immediately.
 *  If another locale is already is loaded, it will first be unloaded and the new one then loaded.
 *  If no locale is loaded, the requested locale will be loaded.
 *  @langIndex -  the index of the locale/voice to load, which is guaranteed to be supported.
 *  return TTS_SUCCESS or TTS_FAILURE
 */
static tts_result doLanguageSwitchFromLangIndex( int langIndex )
{
    int ret;                                        /* function result code */

    if (langIndex>=0) {
        /* If we already have a loaded locale, check whether it is the same one as requested.   */
        if (picoProp_currLang && (strcmp(picoProp_currLang, picoSupportedLang[langIndex]) == 0)) {
            //ALOGI("Language already loaded (%s == %s)", picoProp_currLang,
            //        picoSupportedLang[langIndex]);
            return TTS_SUCCESS;
        }
    }

    /* It is not the same locale; unload the current one first. Also invalidates the system object*/
    cleanResources();

    /* Allocate memory for file and resource names.     */
    cleanFiles();

    if (picoSystem==NULL) {
        /*re-init system object*/
        ret = pico_initialize( picoMemArea, PICO_MEM_SIZE, &picoSystem );
        if (PICO_OK != ret) {
            ALOGE("Failed to initialize the pico system object\n");
            return TTS_FAILURE;
        }
    }

    picoProp_currLang   = (char *)      malloc( 10 );
    picoTaFileName      = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );
    picoSgFileName      = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );
    picoUtppFileName    = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );
    picoTaResourceName  = (pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );
    picoSgResourceName  = (pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );
    picoUtppResourceName =(pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );

    if (
        (picoProp_currLang==NULL) || (picoTaFileName==NULL) || (picoSgFileName==NULL) ||
        (picoUtppFileName==NULL) || (picoTaResourceName==NULL) || (picoSgResourceName==NULL) ||
        (picoUtppResourceName==NULL)
        ) {
        ALOGE("Failed to allocate memory for internal strings\n");
        cleanResources();
        return TTS_FAILURE;
    }

    /* Find where to load the resource files from: system or alternative location              */
    /* based on availability of the Ta file. Try the alternative location first, this is where */
    /* more recent language file updates would be installed (under pico_alt_lingware_path).        */
    bool bUseSystemPath = true;
    FILE * pFile;
    char* tmpFileName = (char*)malloc(PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE);
    strcpy((char*)tmpFileName, pico_alt_lingware_path);
    strcat((char*)tmpFileName, (const char*)picoInternalTaLingware[langIndex]);
    pFile = fopen(tmpFileName, "r");
    if (pFile != NULL) {
        /* "ta" file found under pico_alt_lingware_path, don't use the system path. */
        fclose (pFile);
        bUseSystemPath = false;
    }
    free(tmpFileName);

    /* Set the path and file names for resource files.  */
    if (bUseSystemPath) {
        strcpy((char *) picoTaFileName,   PICO_SYSTEM_LINGWARE_PATH);
        strcpy((char *) picoSgFileName,   PICO_SYSTEM_LINGWARE_PATH);
        strcpy((char *) picoUtppFileName, PICO_SYSTEM_LINGWARE_PATH);
    } else {
        strcpy((char *) picoTaFileName,   pico_alt_lingware_path);
        strcpy((char *) picoSgFileName,   pico_alt_lingware_path);
        strcpy((char *) picoUtppFileName, pico_alt_lingware_path);
    }
    strcat((char *) picoTaFileName,   (const char *) picoInternalTaLingware[langIndex]);
    strcat((char *) picoSgFileName,   (const char *) picoInternalSgLingware[langIndex]);
    strcat((char *) picoUtppFileName, (const char *) picoInternalUtppLingware[langIndex]);

    /* Load the text analysis Lingware resource file.   */
    ret = pico_loadResource( picoSystem, picoTaFileName, &picoTaResource );
    if (PICO_OK != ret) {
        ALOGE("Failed to load textana resource for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Load the signal generation Lingware resource file.   */
    ret = pico_loadResource( picoSystem, picoSgFileName, &picoSgResource );
    if (PICO_OK != ret) {
        ALOGE("Failed to load siggen resource for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Load the utpp Lingware resource file if exists - NOTE: this file is optional
       and is currently not used. Loading is only attempted for future compatibility.
       If this file is not present the loading will still succeed.                      */
    ret = pico_loadResource( picoSystem, picoUtppFileName, &picoUtppResource );
    if ((PICO_OK != ret) && (ret != PICO_EXC_CANT_OPEN_FILE)) {
        ALOGE("Failed to load utpp resource for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Get the text analysis resource name.     */
    ret = pico_getResourceName( picoSystem, picoTaResource, (char *) picoTaResourceName );
    if (PICO_OK != ret) {
        ALOGE("Failed to get textana resource name for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Get the signal generation resource name. */
    ret = pico_getResourceName( picoSystem, picoSgResource, (char *) picoSgResourceName );
    if ((PICO_OK == ret) && (picoUtppResource != NULL)) {
        /* Get utpp resource name - optional: see note above.   */
        ret = pico_getResourceName( picoSystem, picoUtppResource, (char *) picoUtppResourceName );
        if (PICO_OK != ret)  {
            ALOGE("Failed to get utpp resource name for %s [%d]", picoSupportedLang[langIndex], ret);
            cleanResources();
            cleanFiles();
            return TTS_FAILURE;
        }
    }
    if (PICO_OK != ret) {
        ALOGE("Failed to get siggen resource name for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Create a voice definition.   */
    ret = pico_createVoiceDefinition( picoSystem, (const pico_Char *) PICO_VOICE_NAME );
    if (PICO_OK != ret) {
        ALOGE("Failed to create voice for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Add the text analysis resource to the voice. */
    ret = pico_addResourceToVoiceDefinition( picoSystem, (const pico_Char *) PICO_VOICE_NAME, picoTaResourceName );
    if (PICO_OK != ret) {
        ALOGE("Failed to add textana resource to voice for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Add the signal generation resource to the voice. */
    ret = pico_addResourceToVoiceDefinition( picoSystem, (const pico_Char *) PICO_VOICE_NAME, picoSgResourceName );
    if ((PICO_OK == ret) && (picoUtppResource != NULL)) {
        /* Add utpp resource to voice - optional: see note above.   */
        ret = pico_addResourceToVoiceDefinition( picoSystem, (const pico_Char *) PICO_VOICE_NAME, picoUtppResourceName );
        if (PICO_OK != ret) {
            ALOGE("Failed to add utpp resource to voice for %s [%d]", picoSupportedLang[langIndex], ret);
            cleanResources();
            cleanFiles();
            return TTS_FAILURE;
        }
    }

    if (PICO_OK != ret) {
        ALOGE("Failed to add siggen resource to voice for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    ret = pico_newEngine( picoSystem, (const pico_Char *) PICO_VOICE_NAME, &picoEngine );
    if (PICO_OK != ret) {
        ALOGE("Failed to create engine for %s [%d]", picoSupportedLang[langIndex], ret);
        cleanResources();
        cleanFiles();
        return TTS_FAILURE;
    }

    /* Set the current locale/voice.    */
    strcpy( picoProp_currLang, picoSupportedLang[langIndex] );
    picoCurrentLangIndex = langIndex;
    ALOGI("loaded %s successfully", picoProp_currLang);
    return TTS_SUCCESS;
}


/** doLanguageSwitch
 *  Switch to the requested locale.
 *  If this locale is already loaded, it returns immediately.
 *  If another locale is already loaded, this will first be unloaded
 *  and the new one then loaded.
 *  If no locale is loaded, the requested will be loaded.
 *  @locale -  the locale to check, either in xx or xx-YY format (i.e "en" or "en-US")
 *  return TTS_SUCCESS or TTS_FAILURE
*/
static tts_result doLanguageSwitch( const char * locale )
{
    int loclIndex;                              /* locale index */

    /* Load the new locale. */
    loclIndex = checkForLocale( locale );
    if (loclIndex < 0)  {
        ALOGE("Tried to swith to non-supported locale %s", locale);
        return TTS_FAILURE;
    }
    //ALOGI("Found supported locale %s", picoSupportedLang[loclIndex]);
    return doLanguageSwitchFromLangIndex( loclIndex );
}


/** doAddProperties
 *  Add <speed>, <pitch> and <volume> tags to the text,
 *  if the properties have been set to non-default values, and return the new string.
 *  The calling function is responsible for freeing the returned string.
 *  @str - text to apply tags to
 *  return new string with tags applied
*/
static char * doAddProperties( const char * str )
{
    char *  data = NULL;
    int     haspitch, hasspeed, hasvol;                 /* parameters           */
    int     textlen;                                    /* property string length   */
    haspitch = 0; hasspeed = 0; hasvol = 0;
    textlen = strlen(str) + 1;
    if (picoProp_currPitch != PICO_DEF_PITCH) {          /* non-default pitch    */
        textlen += strlen(PICO_PITCH_OPEN_TAG) + 5;
        textlen += strlen(PICO_PITCH_CLOSE_TAG);
        haspitch = 1;
    }
    if (picoProp_currRate != PICO_DEF_RATE) {            /* non-default rate     */
        textlen += strlen(PICO_SPEED_OPEN_TAG) + 5;
        textlen += strlen(PICO_SPEED_CLOSE_TAG);
        hasspeed = 1;
    }

    if (picoProp_currVolume != PICO_DEF_VOLUME) {        /* non-default volume   */
        textlen += strlen(PICO_VOLUME_OPEN_TAG) + 5;
        textlen += strlen(PICO_VOLUME_CLOSE_TAG);
        hasvol = 1;
    }

    /* Compose the property strings.    */
    data = (char *) malloc( textlen );                  /* allocate string      */
    if (!data) {
        return NULL;
    }
    memset(data, 0, textlen);                           /* clear it             */
    if (haspitch) {
        char* tmp = (char*)malloc(strlen(PICO_PITCH_OPEN_TAG) + strlen(PICO_PITCH_CLOSE_TAG) + 5);
        sprintf(tmp, PICO_PITCH_OPEN_TAG, picoProp_currPitch);
        strcat(data, tmp);
        free(tmp);
    }

    if (hasspeed) {
        char* tmp = (char*)malloc(strlen(PICO_SPEED_OPEN_TAG) + strlen(PICO_SPEED_CLOSE_TAG) + 5);
        sprintf(tmp, PICO_SPEED_OPEN_TAG, picoProp_currRate);
        strcat(data, tmp);
        free(tmp);
    }

    if (hasvol) {
        char* tmp = (char*)malloc(strlen(PICO_VOLUME_OPEN_TAG) + strlen(PICO_VOLUME_CLOSE_TAG) + 5);
        sprintf(tmp, PICO_VOLUME_OPEN_TAG, picoProp_currVolume);
        strcat(data, tmp);
        free(tmp);
    }

    strcat(data, str);
    if (hasvol) {
        strcat(data, PICO_VOLUME_CLOSE_TAG);
    }

    if (hasspeed) {
        strcat(data, PICO_SPEED_CLOSE_TAG);
    }

    if (haspitch) {
        strcat(data, PICO_PITCH_CLOSE_TAG);
    }
    return data;
}


/** get_tok
 *  Searches for tokens in a string
 *  @str - text to be processed
 *  @pos - position of first character to be searched in str
 *  @textlen - postion of last character to be searched
 *  @tokstart - address of a variable to receive the start of the token found
 *  @tokstart - address of a variable to receive the length of the token found
 *  return : 1=token found, 0=token not found
 *  notes : the token separator set could be enlarged adding characters in "seps"
*/
static int  get_tok(const char * str , int pos, int textlen, int *tokstart, int *toklen)
{
    const char * seps = " ";

    /*look for start*/
    while ((pos<textlen) && (strchr(seps,str[pos]) != NULL)) {
        pos++;
    }
    if (pos == textlen) {
        /*no characters != seps found whithin string*/
        return 0;
    }
    *tokstart = pos;
    /*look for end*/
    while ((pos<textlen) && (strchr(seps,str[pos]) == NULL)) {
        pos++;
    }
    *toklen = pos - *tokstart;
    return 1;
}/*get_tok*/


/** get_sub_tok
 *  Searches for subtokens in a token having a compound structure with camel case like "xxxYyyy"
 *  @str - text to be processed
 *  @pos - position of first character to be searched in str
 *  @textlen - postion of last character to be searched in str
 *  @tokstart - address of a variable to receive the start of the sub token found
 *  @tokstart - address of a variable to receive the length of the sub token found
 *  return : 1=sub token found, 0=sub token not found
 *  notes : the sub token separator set could be enlarged adding characters in "seps"
*/
static int  get_sub_tok(const char * str , int pos, int textlen, int *tokstart, int *toklen) {

    const char * seps = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    if (pos == textlen) {
        return 0;
    }

    /*first char != space*/
    *tokstart = pos;
    /*finding first non separator*/
    while ((pos < textlen) && (strchr(seps, str[pos]) != NULL)) {
        pos++;
    }
    if (pos == textlen) {
        /*characters all in seps found whithin string : return full token*/
        *toklen = pos - *tokstart;
        return 1;
    }
    /*pos should be pointing to first non seps and more chars are there*/
    /*finding first separator*/
    while ((pos < textlen) && (strchr(seps, str[pos]) == NULL)) {
        pos++;
    }
    if (pos == textlen) {
        /*transition non seps->seps not found : return full token*/
        *toklen = pos - *tokstart;
        return 1;
    }
    *toklen = pos - *tokstart;
    return 1;
}/*get_sub_tok*/


/** doCamelCase
 *  Searches for tokens having a compound structure with camel case and transforms them as follows :
 *        "XxxxYyyy" -->> "Xxxx Yyyy",
 *        "xxxYyyy"  -->> "xxx Yyyy",
 *        "XXXYyyy"  -->> "XXXYyyy"
 *        etc....
 *  The calling function is responsible for freeing the returned string.
 *  @str - text to be processed
 *  return new string with text processed
*/
static char * doCamelCase( const char * str )
{
    int     textlen;             /* input string length   */
    int     totlen;              /* output string length   */
    int     tlen_2, nsubtok;     /* nuber of subtokens   */
    int     toklen, tokstart;    /*legnth and start of generic token*/
    int     stoklen, stokstart;  /*legnth and start of generic sub-token*/
    int     pos, tokpos, outpos; /*postion of current char in input string and token and output*/
    char    *data;               /*pointer of the returned string*/

    pos = 0;
    tokpos = 0;
    toklen = 0;
    stoklen = 0;
    tlen_2 = 0;
    totlen = 0;

    textlen = strlen(str) + 1;

    /*counting characters after sub token splitting including spaces*/
    //while ((pos<textlen) && (str[pos]!=0)) {
    while (get_tok(str, pos, textlen, &tokstart, &toklen)) {
        tokpos = tokstart;
        tlen_2 = 0;
        nsubtok = 0;
        while (get_sub_tok(str, tokpos, tokstart+toklen, &stokstart, &stoklen)) {
            totlen += stoklen;
            tlen_2 += stoklen;
            tokpos = stokstart + stoklen;
            nsubtok += 1;
        }
        totlen += nsubtok;    /*add spaces between subtokens*/
        pos = tokstart + tlen_2;
    }
    //}
    /* Allocate the return string */

    data = (char *) malloc( totlen );                  /* allocate string      */
    if (!data) {
        return NULL;
    }
    memset(data, 0, totlen);                           /* clear it             */
    outpos = 0;
    pos = 0;
    /*copying characters*/
    //while ((pos<textlen) && (str[pos]!=0)) {
    while (get_tok  (str, pos, textlen, &tokstart, &toklen)) {
        tokpos = tokstart;
        tlen_2 = 0;
        nsubtok = 0;
        while (get_sub_tok(str, tokpos, tokstart+toklen, &stokstart, &stoklen)) {
            strncpy(&(data[outpos]), &(str[stokstart]), stoklen);
            outpos += stoklen;
            strncpy(&(data[outpos]), " ", 1);
            tlen_2 += stoklen;
            outpos += 1;
            tokpos = stokstart + stoklen;
        }
        pos=tokstart+tlen_2;
    }
    //}
    if (outpos == 0) {
        outpos = 1;
    }
    data[outpos-1] = 0;
    return data;
}/*doCamelCase*/


/** createPhonemeString
 *  Wrap all individual words in <phoneme> tags.
 *  The Pico <phoneme> tag only supports one word in each tag,
 *  therefore they must be individually wrapped!
 *  @xsampa - text to convert to Pico phomene string
 *  @length - length of the input string
 *  return new string with tags applied
*/
extern char * createPhonemeString( const char * xsampa, int length )
{
    char *  convstring = NULL;
    int     origStrLen = strlen(xsampa);
    int     numWords   = 1;
    int     start, totalLength, i, j;

    for (i = 0; i < origStrLen; i ++) {
        if ((xsampa[i] == ' ') || (xsampa[i] == '#')) {
            numWords ++;
        }
    }

    if (numWords == 1) {
        convstring = new char[origStrLen + 17];
        convstring[0] = '\0';
        strcat(convstring, PICO_PHONEME_OPEN_TAG);
        strcat(convstring, xsampa);
        strcat(convstring, PICO_PHONEME_CLOSE_TAG);
    } else {
        char * words[numWords];
        start = 0; totalLength = 0; i = 0; j = 0;
        for (i=0, j=0; i < origStrLen; i++) {
            if ((xsampa[i] == ' ') || (xsampa[i] == '#')) {
                words[j]    = new char[i+1-start+17];
                words[j][0] = '\0';
                strcat( words[j], PICO_PHONEME_OPEN_TAG);
                strncat(words[j], xsampa+start, i-start);
                strcat( words[j], PICO_PHONEME_CLOSE_TAG);
                start = i + 1;
                j++;
                totalLength += strlen(words[j-1]);
            }
        }
        words[j]    = new char[i+1-start+17];
        words[j][0] = '\0';
        strcat(words[j], PICO_PHONEME_OPEN_TAG);
        strcat(words[j], xsampa+start);
        strcat(words[j], PICO_PHONEME_CLOSE_TAG);
        totalLength += strlen(words[j]);
        convstring = new char[totalLength + 1];
        convstring[0] = '\0';
        for (i=0 ; i < numWords ; i++) {
            strcat(convstring, words[i]);
            delete [] words[i];
        }
    }

    return convstring;
}

/* The XSAMPA uses as many as 5 characters to represent a single IPA code.  */
typedef struct tagPhnArr
{
    char16_t    strIPA;             /* IPA Unicode symbol       */
    char        strXSAMPA[6];       /* SAMPA sequence           */
} PArr;

#define phn_cnt (134+7)

PArr    PhnAry[phn_cnt] = {

    /* XSAMPA conversion table
	   This maps a single IPA symbol to a sequence representing XSAMPA.
       This relies upon a direct one-to-one correspondance
       including diphthongs and affricates.						      */

    /* Vowels (23) complete     */
    {0x025B,        "E"},
    {0x0251,        "A"},
    {0x0254,        "O"},
    {0x00F8,        "2"},
    {0x0153,        "9"},
    {0x0276,        "&"},
    {0x0252,        "Q"},
    {0x028C,        "V"},
    {0x0264,        "7"},
    {0x026F,        "M"},
    {0x0268,        "1"},
    {0x0289,        "}"},
    {0x026A,        "I"},
    {0x028F,        "Y"},
    {0x028A,        "U"},
    {0x0259,        "@"},
    {0x0275,        "8"},
    {0x0250,        "6"},
    {0x00E6,        "{"},
    {0x025C,        "3"},
    {0x025A,        "@`"},
    {0x025E,        "3\\\\"},
    {0x0258,        "@\\\\"},

    /* Consonants (60) complete */
    {0x0288,        "t`"},
    {0x0256,        "d`"},
    {0x025F,        "J\\\\"},
    {0x0261,        "g"},
    {0x0262,        "G\\\\"},
    {0x0294,        "?"},
    {0x0271,        "F"},
    {0x0273,        "n`"},
    {0x0272,        "J"},
    {0x014B,        "N"},
    {0x0274,        "N\\\\"},
    {0x0299,        "B\\\\"},
    {0x0280,        "R\\\\"},
    {0x027E,        "4"},
    {0x027D,        "r`"},
    {0x0278,        "p\\\\"},
    {0x03B2,        "B"},
    {0x03B8,        "T"},
    {0x00F0,        "D"},
    {0x0283,        "S"},
    {0x0292,        "Z"},
    {0x0282,        "s`"},
    {0x0290,        "z`"},
    {0x00E7,        "C"},
    {0x029D,        "j\\\\"},
    {0x0263,        "G"},
    {0x03C7,        "X"},
    {0x0281,        "R"},
    {0x0127,        "X\\\\"},
    {0x0295,        "?\\\\"},
    {0x0266,        "h\\\\"},
    {0x026C,        "K"},
    {0x026E,        "K\\\\"},
    {0x028B,        "P"},
    {0x0279,        "r\\\\"},
    {0x027B,        "r\\\\'"},
    {0x0270,        "M\\\\"},
    {0x026D,        "l`"},
    {0x028E,        "L"},
    {0x029F,        "L\\\\"},
    {0x0253,        "b_<"},
    {0x0257,        "d_<"},
    {0x0284,        "J\\_<"},
    {0x0260,        "g_<"},
    {0x029B,        "G\\_<"},
    {0x028D,        "W"},
    {0x0265,        "H"},
    {0x029C,        "H\\\\"},
    {0x02A1,        ">\\\\"},
    {0x02A2,        "<\\\\"},
    {0x0267,        "x\\\\"},		/* hooktop heng	*/
    {0x0298,        "O\\\\"},
    {0x01C0,        "|\\\\"},
    {0x01C3,        "!\\\\"},
    {0x01C2,        "=\\"},
    {0x01C1,        "|\\|\\"},
    {0x027A,        "l\\\\"},
    {0x0255,        "s\\\\"},
    {0x0291,        "z\\\\"},
    {0x026B,        "l_G"},


    /* Diacritics (37) complete */
    {0x02BC,        "_>"},
    {0x0325,        "_0"},
    {0x030A,        "_0"},
    {0x032C,        "_v"},
    {0x02B0,        "_h"},
    {0x0324,        "_t"},
    {0x0330,        "_k"},
    {0x033C,        "_N"},
    {0x032A,        "_d"},
    {0x033A,        "_a"},
    {0x033B,        "_m"},
    {0x0339,        "_O"},
    {0x031C,        "_c"},
    {0x031F,        "_+"},
    {0x0320,        "_-"},
    {0x0308,        "_\""},     /* centralized		*/
    {0x033D,        "_x"},
    {0x0318,        "_A"},
    {0x0319,        "_q"},
    {0x02DE,        "`"},
    {0x02B7,        "_w"},
    {0x02B2,        "_j"},
    {0x02E0,        "_G"},
    {0x02E4,        "_?\\\\"},	/* pharyngealized	*/
    {0x0303,        "~"},		/* nasalized		*/
    {0x207F,        "_n"},
    {0x02E1,        "_l"},
    {0x031A,        "_}"},
    {0x0334,        "_e"},
    {0x031D,        "_r"},		/* raised  equivalent to 02D4 */
    {0x02D4,        "_r"},		/* raised  equivalent to 031D */
    {0x031E,        "_o"},		/* lowered equivalent to 02D5 */
    {0x02D5,        "_o"},		/* lowered equivalent to 031E */
    {0x0329,        "="},		/* sylabic			*/
    {0x032F,        "_^"},		/* non-sylabic		*/
    {0x0361,        "_"},		/* top tie bar		*/
    {0x035C,        "_"},

    /* Suprasegmental (15) incomplete */
    {0x02C8,        "\""},		/* primary   stress	*/
    {0x02CC,        "%"},		/* secondary stress	*/
    {0x02D0,        ":"},		/* long				*/
    {0x02D1,        ":\\\\"},	/* half-long		*/
    {0x0306,        "_X"},		/* extra short		*/

    {0x2016,        "||"},		/* major group		*/
    {0x203F,        "-\\\\"},	/* bottom tie bar	*/
    {0x2197,        "<R>"},		/* global rise		*/
    {0x2198,        "<F>"},		/* global fall		*/
    {0x2193,        "<D>"},		/* downstep			*/
    {0x2191,        "<U>"},		/* upstep			*/
    {0x02E5,        "<T>"},		/* extra high level	*/
    {0x02E7,        "<M>"},		/* mid level		*/
    {0x02E9,        "<B>"},		/* extra low level	*/

    {0x025D,        "3`:"},		/* non-IPA	%%		*/

    /* Affricates (6) complete  */
    {0x02A3,        "d_z"},
    {0x02A4,        "d_Z"},
    {0x02A5,        "d_z\\\\"},
    {0x02A6,        "t_s"},
    {0x02A7,        "t_S"},
    {0x02A8,        "t_s\\\\"}
    };


void CnvIPAPnt( const char16_t IPnt, char * XPnt )
{
    char16_t        ThisPnt = IPnt;                     /* local copy of single IPA codepoint   */
    int             idx;                                /* index into table         */

    /* Convert an individual IPA codepoint.
       A single IPA code could map to a string.
       Search the table.  If it is not found, use the same character.
       Since most codepoints can be contained within 16 bits,
       they are represented as wide chars.              */
    XPnt[0] = 0;                                        /* clear the result string  */

    /* Search the table for the conversion. */
    for (idx = 0; idx < phn_cnt; idx ++) {               /* for each item in table   */
        if (IPnt == PhnAry[idx].strIPA) {                /* matches IPA code         */
            strcat( XPnt, (const char *)&(PhnAry[idx].strXSAMPA) ); /* copy the XSAMPA string   */
            return;
        }
    }
    strcat(XPnt, (const char *)&ThisPnt);               /* just copy it             */
}


/** cnvIpaToXsampa
 *  Convert an IPA character string to an XSAMPA character string.
 *  @ipaString - input IPA string to convert
 *  @outXsampaString - converted XSAMPA string is passed back in this parameter
 *  return size of the new string
*/

int cnvIpaToXsampa( const char16_t * ipaString, size_t ipaStringSize, char ** outXsampaString )
{
    size_t xsize;                                  /* size of result               */
    size_t ipidx;                                  /* index into IPA string        */
    char * XPnt;                                   /* short XSAMPA char sequence   */

    /* Convert an IPA string to an XSAMPA string and store the xsampa string in *outXsampaString.
       It is the responsibility of the caller to free the allocated string.
       Increment through the string.  For each base & combination convert it to the XSAMP equivalent.
       Because of the XSAMPA limitations, not all IPA characters will be covered.       */
    XPnt = (char *) malloc(6);
    xsize   = (4 * ipaStringSize) + 8;          /* assume more than double size */
    *outXsampaString = (char *) malloc( xsize );/* allocate return string   */
    *outXsampaString[0] = 0;
    xsize = 0;                                  /* clear final              */

    for (ipidx = 0; ipidx < ipaStringSize; ipidx ++) { /* for each IPA code        */
        CnvIPAPnt( ipaString[ipidx], XPnt );           /* get converted character  */
        strcat((char *)*outXsampaString, XPnt );       /* concatenate XSAMPA       */
    }
    free(XPnt);
    xsize = strlen(*outXsampaString);                  /* get the final length     */
    return xsize;
}


/* Google Engine API function implementations */

/** init
 *  Allocates Pico memory block and initializes the Pico system.
 *  synthDoneCBPtr - Pointer to callback function which will receive generated samples
 *  config - the engine configuration parameters, here only contains the non-system path
 *      for the lingware location
 *  return tts_result
*/
tts_result TtsEngine::init( synthDoneCB_t synthDoneCBPtr, const char *config )
{
    if (synthDoneCBPtr == NULL) {
        ALOGE("Callback pointer is NULL");
        return TTS_FAILURE;
    }

    picoMemArea = malloc( PICO_MEM_SIZE );
    if (!picoMemArea) {
        ALOGE("Failed to allocate memory for Pico system");
        return TTS_FAILURE;
    }

    pico_Status ret = pico_initialize( picoMemArea, PICO_MEM_SIZE, &picoSystem );
    if (PICO_OK != ret) {
        ALOGE("Failed to initialize Pico system");
        free( picoMemArea );
        picoMemArea = NULL;
        return TTS_FAILURE;
    }

    picoSynthDoneCBPtr = synthDoneCBPtr;

    picoCurrentLangIndex = -1;

    // was the initialization given an alternative path for the lingware location?
    if ((config != NULL) && (strlen(config) > 0)) {
        pico_alt_lingware_path = (char*)malloc(strlen(config));
        strcpy((char*)pico_alt_lingware_path, config);
        ALOGV("Alternative lingware path %s", pico_alt_lingware_path);
    } else {
        pico_alt_lingware_path = (char*)malloc(strlen(PICO_LINGWARE_PATH) + 1);
        strcpy((char*)pico_alt_lingware_path, PICO_LINGWARE_PATH);
        ALOGV("Using predefined lingware path %s", pico_alt_lingware_path);
    }

    return TTS_SUCCESS;
}


/** shutdown
 *  Unloads all Pico resources; terminates Pico system and frees Pico memory block.
 *  return tts_result
*/
tts_result TtsEngine::shutdown( void )
{
    cleanResources();

    if (picoSystem) {
        pico_terminate(&picoSystem);
        picoSystem = NULL;
    }
    if (picoMemArea) {
        free(picoMemArea);
        picoMemArea = NULL;
    }

    cleanFiles();
    return TTS_SUCCESS;
}


/** loadLanguage
 *  Load a new language.
 *  @lang - string with ISO 3 letter language code.
 *  @country - string with ISO 3 letter country code .
 *  @variant - string with language variant for that language and country pair.
 *  return tts_result
*/
tts_result TtsEngine::loadLanguage(const char *lang, const char *country, const char *variant)
{
    return TTS_FAILURE;
    //return setProperty("language", value, size);
}


/** setLanguage
 *  Load a new language (locale).  Use the ISO 639-3 language codes.
 *  @lang - string with ISO 639-3 language code.
 *  @country - string with ISO 3 letter country code.
 *  @variant - string with language variant for that language and country pair.
 *  return tts_result
 */
tts_result TtsEngine::setLanguage( const char * lang, const char * country, const char * variant )
{
    //ALOGI("TtsEngine::setLanguage %s %s %s", lang, country, variant);
    int langIndex;
    int countryIndex;
    int i;

    if (lang == NULL)
        {
        ALOGE("TtsEngine::setLanguage called with NULL language");
        return TTS_FAILURE;
        }

    /* We look for a match on the language first
       then we look for a match on the country.
       If no match on the language:
             return an error.
       If match on the language, but no match on the country:
             load the language found for the language match.
       If match on the language, and match on the country:
             load the language found for the country match.     */

    /* Find a match on the language.    */
    langIndex = -1;                                     /* no match */
    for (i = 0; i < picoNumSupportedVocs; i ++)
        {
        if (strcmp(lang, picoSupportedLangIso3[i]) == 0)
            {
            langIndex = i;
            break;
            }
        }
    if (langIndex < 0)
        {
        /* The language isn't supported.    */
        ALOGE("TtsEngine::setLanguage called with unsupported language");
        return TTS_FAILURE;
        }

    /* Find a match on the country, if there is one.    */
    if (country != NULL)
        {
        countryIndex = -1;
        for (i = langIndex; i < picoNumSupportedVocs; i ++)
            {
            if (   (strcmp(lang,    picoSupportedLangIso3[i])    == 0)
                && (strcmp(country, picoSupportedCountryIso3[i]) == 0))
                {
                countryIndex = i;
                break;
                }
            }

        if (countryIndex < 0)
            {
            /* We didn't find a match on the country, but we had a match on the language.
               Use that language.                                                       */
            ALOGI("TtsEngine::setLanguage found matching language(%s) but not matching country(%s).",
                    lang, country);
            }
        else
            {
            /* We have a match on both the language and the country.    */
            langIndex = countryIndex;
            }
        }

    return doLanguageSwitchFromLangIndex( langIndex );      /* switch the language  */
}


/** isLanguageAvailable
 *  Returns the level of support for a language.
 *  @lang - string with ISO 3 letter language code.
 *  @country - string with ISO 3 letter country code .
 *  @variant - string with language variant for that language and country pair.
 *  return tts_support_result
*/
tts_support_result TtsEngine::isLanguageAvailable(const char *lang, const char *country,
            const char *variant) {
    int langIndex = -1;
    int countryIndex = -1;
    //-------------------------
    // language matching
    // if no language specified
    if (lang == NULL)  {
        ALOGE("TtsEngine::isLanguageAvailable called with no language");
        return TTS_LANG_NOT_SUPPORTED;
    }

    // find a match on the language
    for (int i = 0; i < picoNumSupportedVocs; i++)
    {
        if (strcmp(lang, picoSupportedLangIso3[i]) == 0) {
            langIndex = i;
            break;
        }
    }
    if (langIndex < 0) {
        // language isn't supported
        ALOGV("TtsEngine::isLanguageAvailable called with unsupported language");
        return TTS_LANG_NOT_SUPPORTED;
    }

    //-------------------------
    // country matching
    // if no country specified
    if ((country == NULL) || (strlen(country) == 0)) {
        // check installation of matched language
        return (hasResourcesForLanguage(langIndex) ? TTS_LANG_AVAILABLE : TTS_LANG_MISSING_DATA);
    }

    // find a match on the country
    for (int i = langIndex; i < picoNumSupportedVocs; i++) {
        if ((strcmp(lang, picoSupportedLangIso3[i]) == 0)
                && (strcmp(country, picoSupportedCountryIso3[i]) == 0)) {
            countryIndex = i;
            break;
        }
    }
    if (countryIndex < 0)  {
        // we didn't find a match on the country, but we had a match on the language
        // check installation of matched language
        return (hasResourcesForLanguage(langIndex) ? TTS_LANG_AVAILABLE : TTS_LANG_MISSING_DATA);
    } else {
        // we have a match on the language and the country
        langIndex = countryIndex;
        // check installation of matched language + country
        return (hasResourcesForLanguage(langIndex) ? TTS_LANG_COUNTRY_AVAILABLE : TTS_LANG_MISSING_DATA);
    }

    // no variants supported in this library, TTS_LANG_COUNTRY_VAR_AVAILABLE cannot be returned.
}


/** getLanguage
 *  Get the currently loaded language - if any.
 *  @lang - string with current ISO 3 letter language code, empty string if no loaded language.
 *  @country - string with current ISO 3 letter country code, empty string if no loaded language.
 *  @variant - string with current language variant, empty string if no loaded language.
 *  return tts_result
*/
tts_result TtsEngine::getLanguage(char *language, char *country, char *variant)
{
    if (picoCurrentLangIndex == -1) {
        strcpy(language, "\0");
        strcpy(country, "\0");
        strcpy(variant, "\0");
    } else {
        strcpy(language, picoSupportedLangIso3[picoCurrentLangIndex]);
        strcpy(country, picoSupportedCountryIso3[picoCurrentLangIndex]);
        // no variant in this implementation
        strcpy(variant, "\0");
    }
    return TTS_SUCCESS;
}


/** setAudioFormat
 * sets the audio format to use for synthesis, returns what is actually used.
 * @encoding - reference to encoding format
 * @rate - reference to sample rate
 * @channels - reference to number of channels
 * return tts_result
 * */
tts_result TtsEngine::setAudioFormat(tts_audio_format& encoding, uint32_t& rate,
            int& channels)
{
    // ignore the input parameters, the enforced audio parameters are fixed here
    encoding = TTS_AUDIO_FORMAT_PCM_16_BIT;
    rate = 16000;
    channels = 1;
    return TTS_SUCCESS;
}


/** setProperty
 *  Set property. The supported properties are:  language, rate, pitch and volume.
 *  @property - name of property to set
 *  @value - value to set
 *  @size - size of value
 *  return tts_result
*/
tts_result TtsEngine::setProperty( const char * property, const char * value, const size_t size )
{
    int rate;
    int pitch;
    int volume;

    /* Set a specific property for the engine.
       Supported properties include: language (locale), rate, pitch, volume.    */
    /* Sanity check */
    if (property == NULL) {
        ALOGE("setProperty called with property NULL");
        return TTS_PROPERTY_UNSUPPORTED;
    }

    if (value == NULL) {
        ALOGE("setProperty called with value NULL");
        return TTS_VALUE_INVALID;
    }

    if (strncmp(property, "language", 8) == 0) {
        /* Verify it's in correct format.   */
        if (strlen(value) != 2 && strlen(value) != 6) {
            ALOGE("change language called with incorrect format");
            return TTS_VALUE_INVALID;
        }

        /* Try to switch to specified language. */
        if (doLanguageSwitch(value) == TTS_FAILURE) {
            ALOGE("failed to load language");
            return TTS_FAILURE;
        } else {
            return TTS_SUCCESS;
        }
    } else if (strncmp(property, "rate", 4) == 0) {
        rate = atoi(value);
        if (rate < PICO_MIN_RATE) {
            rate = PICO_MIN_RATE;
        }
        if (rate > PICO_MAX_RATE) {
            rate = PICO_MAX_RATE;
        }
        picoProp_currRate = rate;
        return TTS_SUCCESS;
    } else if (strncmp(property, "pitch", 5) == 0) {
        pitch = atoi(value);
        if (pitch < PICO_MIN_PITCH) {
            pitch = PICO_MIN_PITCH;
        }
        if (pitch > PICO_MAX_PITCH) {
            pitch = PICO_MAX_PITCH;
        }
        picoProp_currPitch = pitch;
        return TTS_SUCCESS;
    } else if (strncmp(property, "volume", 6) == 0) {
        volume = atoi(value);
        if (volume < PICO_MIN_VOLUME) {
            volume = PICO_MIN_VOLUME;
        }
        if (volume > PICO_MAX_VOLUME) {
            volume = PICO_MAX_VOLUME;
        }
        picoProp_currVolume = volume;
        return TTS_SUCCESS;
    }

    return TTS_PROPERTY_UNSUPPORTED;
}


/** getProperty
 *  Get the property.  Supported properties are:  language, rate, pitch and volume.
 *  @property - name of property to get
 *  @value    - buffer which will receive value of property
 *  @iosize   - size of value - if size is too small on return this will contain actual size needed
 *  return tts_result
*/
tts_result TtsEngine::getProperty( const char * property, char * value, size_t * iosize )
{
    /* Get the property for the engine.
       This property was previously set by setProperty or by default.       */
    /* sanity check */
    if (property == NULL) {
        ALOGE("getProperty called with property NULL");
        return TTS_PROPERTY_UNSUPPORTED;
    }

    if (value == NULL) {
        ALOGE("getProperty called with value NULL");
        return TTS_VALUE_INVALID;
    }

    if (strncmp(property, "language", 8) == 0) {
        if (picoProp_currLang == NULL) {
            strcpy(value, "");
        } else {
            if (*iosize < strlen(picoProp_currLang)+1)  {
                *iosize = strlen(picoProp_currLang) + 1;
                return TTS_PROPERTY_SIZE_TOO_SMALL;
            }
            strcpy(value, picoProp_currLang);
        }
        return TTS_SUCCESS;
    } else if (strncmp(property, "rate", 4) == 0) {
        char tmprate[4];
        sprintf(tmprate, "%d", picoProp_currRate);
        if (*iosize < strlen(tmprate)+1) {
            *iosize = strlen(tmprate) + 1;
            return TTS_PROPERTY_SIZE_TOO_SMALL;
        }
        strcpy(value, tmprate);
        return TTS_SUCCESS;
    } else if (strncmp(property, "pitch", 5) == 0) {
        char tmppitch[4];
        sprintf(tmppitch, "%d", picoProp_currPitch);
        if (*iosize < strlen(tmppitch)+1) {
            *iosize = strlen(tmppitch) + 1;
            return TTS_PROPERTY_SIZE_TOO_SMALL;
        }
        strcpy(value, tmppitch);
        return TTS_SUCCESS;
    } else if (strncmp(property, "volume", 6) == 0) {
        char tmpvol[4];
        sprintf(tmpvol, "%d", picoProp_currVolume);
        if (*iosize < strlen(tmpvol)+1) {
            *iosize = strlen(tmpvol) + 1;
            return TTS_PROPERTY_SIZE_TOO_SMALL;
        }
        strcpy(value, tmpvol);
        return TTS_SUCCESS;
    }

    /* Unknown property */
    ALOGE("Unsupported property");
    return TTS_PROPERTY_UNSUPPORTED;
}


/** synthesizeText
 *  Synthesizes a text string.
 *  The text string could be annotated with SSML tags.
 *  @text     - text to synthesize
 *  @buffer   - buffer which will receive generated samples
 *  @bufferSize - size of buffer
 *  @userdata - pointer to user data which will be passed back to callback function
 *  return tts_result
*/
tts_result TtsEngine::synthesizeText( const char * text, int8_t * buffer, size_t bufferSize, void * userdata )
{
    int         err;
    int         cbret;
    pico_Char * inp = NULL;
    char *      expanded_text = NULL;
    pico_Char * local_text = NULL;
    short       outbuf[MAX_OUTBUF_SIZE/2];
    pico_Int16  bytes_sent, bytes_recv, text_remaining, out_data_type;
    pico_Status ret;
    SvoxSsmlParser * parser = NULL;

    picoSynthAbort = 0;
    if (text == NULL) {
        ALOGE("synthesizeText called with NULL string");
        return TTS_FAILURE;
    }

    if (strlen(text) == 0) {
        return TTS_SUCCESS;
    }

    if (buffer == NULL) {
        ALOGE("synthesizeText called with NULL buffer");
        return TTS_FAILURE;
    }

    if ( (strncmp(text, "<speak", 6) == 0) || (strncmp(text, "<?xml", 5) == 0) ) {
        /* SSML input */
        parser = new SvoxSsmlParser();
        if (parser && parser->initSuccessful()) {
            err = parser->parseDocument(text, 1);
            if (err == XML_STATUS_ERROR) {
                /* Note: for some reason expat always thinks the input document has an error
                   at the end, even when the XML document is perfectly formed */
                ALOGI("Warning: SSML document parsed with errors");
            }
            char * parsed_text = parser->getParsedDocument();
            if (parsed_text) {
                /* Add property tags to the string - if any.    */
                local_text = (pico_Char *) doAddProperties( parsed_text );
                if (!local_text) {
                    ALOGE("Failed to allocate memory for text string");
                    delete parser;
                    return TTS_FAILURE;
                }
                char * lang = parser->getParsedDocumentLanguage();
                if (lang != NULL) {
                    if (doLanguageSwitch(lang) == TTS_FAILURE) {
                        ALOGE("Failed to switch to language (%s) specified in SSML document.", lang);
                        delete parser;
                        return TTS_FAILURE;
                    }
                } else {
                    // lang is NULL, pick a language so the synthesis can be performed
                    if (picoCurrentLangIndex == -1) {
                        // no current language loaded, pick the first one and load it
                        if (doLanguageSwitchFromLangIndex(0) == TTS_FAILURE) {
                            ALOGE("Failed to switch to default language.");
                            delete parser;
                            return TTS_FAILURE;
                        }
                    }
                    //ALOGI("No language in SSML, using current language (%s).", picoProp_currLang);
                }
                delete parser;
            } else {
                ALOGE("Failed to parse SSML document");
                delete parser;
                return TTS_FAILURE;
            }
        } else {
            ALOGE("Failed to create SSML parser");
            if (parser) {
                delete parser;
            }
            return TTS_FAILURE;
        }
    } else {
        /* camelCase pre-processing */
        expanded_text = doCamelCase(text);
        /* Add property tags to the string - if any.    */
        local_text = (pico_Char *) doAddProperties( expanded_text );
        if (expanded_text) {
            free( expanded_text );
        }
        if (!local_text) {
            ALOGE("Failed to allocate memory for text string");
            return TTS_FAILURE;
        }
    }

    text_remaining = strlen((const char *) local_text) + 1;

    inp = (pico_Char *) local_text;

    size_t bufused = 0;

    /* synthesis loop   */
    while (text_remaining) {
        if (picoSynthAbort) {
            ret = pico_resetEngine( picoEngine, PICO_RESET_SOFT );
            break;
        }

        /* Feed the text into the engine.   */
        ret = pico_putTextUtf8( picoEngine, inp, text_remaining, &bytes_sent );
        if (ret != PICO_OK) {
            ALOGE("Error synthesizing string '%s': [%d]", text, ret);
            if (local_text) {
                free( local_text );
            }
            return TTS_FAILURE;
        }

        text_remaining -= bytes_sent;
        inp += bytes_sent;
        do {
            if (picoSynthAbort) {
                ret = pico_resetEngine( picoEngine, PICO_RESET_SOFT );
                break;
            }
            /* Retrieve the samples and add them to the buffer. */
            ret = pico_getData( picoEngine, (void *) outbuf, MAX_OUTBUF_SIZE, &bytes_recv,
                    &out_data_type );
            if (bytes_recv) {
                if ((bufused + bytes_recv) <= bufferSize) {
                    memcpy(buffer+bufused, (int8_t *) outbuf, bytes_recv);
                    bufused += bytes_recv;
                } else {
                    /* The buffer filled; pass this on to the callback function.    */
                    cbret = picoSynthDoneCBPtr(userdata, 16000, TTS_AUDIO_FORMAT_PCM_16_BIT, 1, buffer,
                            bufused, TTS_SYNTH_PENDING);
                    if (cbret == TTS_CALLBACK_HALT) {
                        ALOGI("Halt requested by caller. Halting.");
                        picoSynthAbort = 1;
                        ret = pico_resetEngine( picoEngine, PICO_RESET_SOFT );
                        break;
                    }
                    bufused = 0;
                    memcpy(buffer, (int8_t *) outbuf, bytes_recv);
                    bufused += bytes_recv;
                }
            }
        } while (PICO_STEP_BUSY == ret);

        /* This chunk of synthesis is finished; pass the remaining samples.
           Use 16 KHz, 16-bit samples.                                              */
        if (!picoSynthAbort) {
            picoSynthDoneCBPtr( userdata, 16000, TTS_AUDIO_FORMAT_PCM_16_BIT, 1, buffer, bufused,
                    TTS_SYNTH_PENDING);
        }
        picoSynthAbort = 0;

        if (ret != PICO_STEP_IDLE) {
            if (ret != 0){
                ALOGE("Error occurred during synthesis [%d]", ret);
            }
            if (local_text) {
                free(local_text);
            }
            ALOGV("Synth loop: sending TTS_SYNTH_DONE after error");
            picoSynthDoneCBPtr( userdata, 16000, TTS_AUDIO_FORMAT_PCM_16_BIT, 1, buffer, bufused,
                    TTS_SYNTH_DONE);
            pico_resetEngine( picoEngine, PICO_RESET_SOFT );
            return TTS_FAILURE;
        }
    }

    /* Synthesis is done; notify the caller */
    ALOGV("Synth loop: sending TTS_SYNTH_DONE after all done, or was asked to stop");
    picoSynthDoneCBPtr( userdata, 16000, TTS_AUDIO_FORMAT_PCM_16_BIT, 1, buffer, bufused,
            TTS_SYNTH_DONE);

    if (local_text) {
        free( local_text );
    }
    return TTS_SUCCESS;
}



/** stop
 *  Aborts the running synthesis.
 *  return tts_result
*/
tts_result TtsEngine::stop( void )
{
    picoSynthAbort = 1;
    return TTS_SUCCESS;
}


#ifdef __cplusplus
extern "C" {
#endif

TtsEngine * getTtsEngine( void )
{
    return new TtsEngine();
}

#ifdef __cplusplus
}
#endif
