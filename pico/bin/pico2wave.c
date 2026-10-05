/* pico2wave.c

 * Copyright (C) 2009 Mathieu Parent <math.parent@gmail.com>
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
 *   Convert text to .wav using svox text-to-speech system.
 *
 */


#include <popt.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <picoapi.h>
#include <picoapid.h>
#include <picoos.h>
#ifdef ENABLE_TENSORFLOW
#include <ttsdlbased.h>
#endif

/* adaptation layer defines */
#define PICO_MEM_SIZE       2500000
#define DummyLen 100000000

/* Parameters for writing on disk*/
#define SAMPLE_RATE 22050  // MelGAN output sample rate
#define WAV_HEADER_SIZE 44

/* string constants */
#define MAX_OUTBUF_SIZE     128
#ifdef picolangdir
const char * PICO_LINGWARE_PATH             = picolangdir "/";
#else
const char * PICO_LINGWARE_PATH             = "./lang/";
#endif
const char * PICO_VOICE_NAME                = "PicoVoice";

/* supported voices
   Pico does not seperately specify the voice and locale.   */
const char * picoSupportedLangIso3[]        = { "eng",              "eng",              "deu",              "spa",              "fra",              "ita" };
const char * picoSupportedCountryIso3[]     = { "USA",              "GBR",              "DEU",              "ESP",              "FRA",              "ITA" };
const char * picoSupportedLang[]            = { "en-US",            "en-GB",            "de-DE",            "es-ES",            "fr-FR",            "it-IT",    "ger"};
const char * picoInternalLang[]             = { "en-US",            "en-GB",            "de-DE",            "es-ES",            "fr-FR",            "it-IT" };
const char * picoInternalTaLingware[]       = { "en-US_ta.bin",     "en-GB_ta.bin",     "de-DE_ta.bin",     "es-ES_ta.bin",     "fr-FR_ta.bin",     "it-IT_ta.bin" };
const char * picoInternalSgLingware[]       = { "en-US_lh0_sg.bin", "en-GB_kh0_sg.bin", "de-DE_gl0_sg.bin", "es-ES_zl0_sg.bin", "fr-FR_nk0_sg.bin", "it-IT_cm0_sg.bin" };
const char * picoInternalUtppLingware[]     = { "en-US_utpp.bin",   "en-GB_utpp.bin",   "de-DE_utpp.bin",   "es-ES_utpp.bin",   "fr-FR_utpp.bin",   "it-IT_utpp.bin" };
const int picoNumSupportedVocs              = 6;

/* adapation layer global variables */
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

#define CHUNK_SIZE 16384UL
/* buffered read from source until EOF; user should free the buffer */
size_t myread(FILE *source, char **buffer) {
    size_t count;
    size_t size = 0;
    size_t offset = 0;
    *buffer = NULL;
    do {
        // printf("allocate %ld chars\n", CHUNK_SIZE);
        *buffer = (char *)realloc(*buffer, size + CHUNK_SIZE);
        assert(*buffer);
        size += CHUNK_SIZE;
        // printf("size = %ld\n", size);
        count = fread(&((*buffer)[offset]), 1, CHUNK_SIZE, source);
        // printf("read chars = %ld\n", count);
        assert(!ferror(source));
        if (feof(source)) {
            // printf("feof = %ld\n", offset + count);
            (*buffer)[offset + count] = 0;
            return offset + count;
        }
        offset += CHUNK_SIZE;
        // printf("offset = %ld\n", offset);
    } while (1);
}

typedef struct WAVHeader {
    char     riff[4];        // "RIFF"
    uint32_t chunk_size;     
    char     wave[4];        // "WAVE"
    char     fmt[4];         // "fmt "
    uint32_t fmt_chunk_size; // 16 for PCM
    uint16_t audio_format;   // 1 = PCM
    uint16_t num_channels;   // 1 = mono
    uint32_t sample_rate;    // e.g., 22050
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char     data[4];        // "data"
    uint32_t data_chunk_size;
} WAVHeader;

void write_wav_header(FILE* fp, size_t num_samples, int sample_rate) {
    WAVHeader header;

    // RIFF chunk
    header.riff[0]='R'; header.riff[1]='I'; header.riff[2]='F'; header.riff[3]='F';
    header.chunk_size = 36 + num_samples * 2; // int16 = 2 bytes
    header.wave[0]='W'; header.wave[1]='A'; header.wave[2]='V'; header.wave[3]='E';
    
    // fmt subchunk
    header.fmt[0]='f'; header.fmt[1]='m'; header.fmt[2]='t'; header.fmt[3]=' ';
    header.fmt_chunk_size = 16;
    header.audio_format = 1; // PCM
    header.num_channels = 1;
    header.sample_rate = sample_rate;
    header.bits_per_sample = 16;
    header.byte_rate = sample_rate * header.num_channels * header.bits_per_sample / 8;
    header.block_align = header.num_channels * header.bits_per_sample / 8;
    
    // data subchunk
    header.data[0]='d'; header.data[1]='a'; header.data[2]='t'; header.data[3]='a';
    header.data_chunk_size = num_samples * 2;

    fwrite(&header, sizeof(WAVHeader), 1, fp);
}

void write_float_wav(const char* filename, float* audio_buffer, size_t audio_size) {
    FILE* fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "Failed to open output file: %s\n", filename);
        return;
    }

    int sample_rate = 22050; // your desired rate
    write_wav_header(fp, audio_size, sample_rate);

    // Convert float [-1,1] to int16
    for (size_t i = 0; i < audio_size; ++i) {
        float sample = audio_buffer[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        int16_t s16 = (int16_t)(sample * 32767.0f);
        fwrite(&s16, sizeof(int16_t), 1, fp);
    }

    fclose(fp);
}


int main(int argc, const char *argv[]) {
    char * wavefile = NULL;
    char * lang = "en-US";
    int langIndex = -1, langIndexTmp = -1;
    char * text = NULL;
    int8_t * buffer;
    size_t bufferSize = 256;

    char * dlbased = "false";
    char* tacotron_path = NULL;
    char* melgan_path = NULL;
    char* processor_path = NULL;

    /* Parsing options */
	poptContext optCon; /* context for parsing command-line options */
	int opt; /* used for argument parsing */

	struct poptOption optionsTable[] = {
		{ "wave", 'w', POPT_ARG_STRING, &wavefile, 0,
		  "Write output to this WAV file (extension SHOULD be .wav)", "filename.wav" },
		{ "lang", 'l', POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &lang, 0,
		  "Language", "lang" },
        { "tacotron", 't', POPT_ARG_STRING, &tacotron_path, 0,
            "path to Tacotron2 TFLite model", "PATH" },
        { "melgan", 'g', POPT_ARG_STRING, &melgan_path, 0,
            "path to MB-MelGAN TFLite model", "PATH" },
        { "processor", 'p', POPT_ARG_STRING, &processor_path, 0,
            "path to processor.json file", "PATH" },
        { "text", 's', POPT_ARG_STRING, &text, 0,
            "text to synthesize", "TEXT" },
        {"dlbased", 'd', POPT_ARG_STRING, &dlbased, 0,
            "Use deep learning based tts", "dl" },
		POPT_AUTOHELP
		POPT_TABLEEND
	};
	optCon = poptGetContext(NULL, argc, argv, optionsTable, POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(optCon, "<words>");

    /* Reporting about invalid extra options */
	while ((opt = poptGetNextOpt(optCon)) != -1) {
		switch (opt) {
		default:
			fprintf(stderr, "Invalid option %s: %s\n",
				poptBadOption(optCon, 0), poptStrerror(opt));
			poptPrintHelp(optCon, stderr, 0);
			exit(1);
		}
	}

    if (dlbased == "false"){
        /* Mandatory option: --wave */
        if(!wavefile) {
            fprintf(stderr, "Mandatory option: %s\n\n",
                "--wave=filename.wav");
            poptPrintHelp(optCon, stderr, 0);
            exit(1);
        }
        /* option: --lang */
        for(langIndexTmp =0; langIndexTmp<picoNumSupportedVocs; langIndexTmp++) {
            if(!strcmp(picoSupportedLang[langIndexTmp], lang)) {
                langIndex = langIndexTmp;
                break;
            }
        }
        if(langIndex == -1) {
            fprintf(stderr, "Unknown language: %s\nValid languages:\n",
                lang);
            for(langIndexTmp =0; langIndexTmp<picoNumSupportedVocs; langIndexTmp++) {
                fprintf(stderr, "%s\n", picoSupportedLang[langIndexTmp]);
            }
            lang = "en-US";
            fprintf(stderr, "\n");
            poptPrintHelp(optCon, stderr, 0);
            exit(1);
        }

        /* Remaining argument is <words> */
        const char **extra_argv;
        extra_argv = poptGetArgs(optCon);
        if(extra_argv) {
            text = (char *) &(*extra_argv)[0];
        } else {
            // read from stdin
            size_t len = myread(stdin, &text);
            // debug buffered read: make this command a pass-though cat-like command and compare input with output
            // printf("read %ld characters\n", len);
            // fwrite(text, 1, len, stdout);
            // exit(1);

            // TODO: break text in chunks of max size equal to pico_Int16 textSize,
            // /usr/include/picoapi.h:typedef short pico_Int16;
            // /usr/include/limits.h:#  define SHRT_MAX        32767
        }

        poptFreeContext(optCon);

        buffer = malloc( bufferSize );

        int ret, getstatus;
        pico_Char * inp = NULL;
        pico_Char * local_text = NULL;
        short       outbuf[MAX_OUTBUF_SIZE/2];
        pico_Int16  bytes_sent, bytes_recv, text_remaining, out_data_type;
        pico_Retstring outMessage;

        picoSynthAbort = 0;

        picoMemArea = malloc( PICO_MEM_SIZE );
        if((ret = pico_initialize( picoMemArea, PICO_MEM_SIZE, &picoSystem ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot initialize pico (%i): %s\n", ret, outMessage);
            goto terminate;
        }

        /* Load the text analysis Lingware resource file.   */
        picoTaFileName      = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );
        strcpy((char *) picoTaFileName,   PICO_LINGWARE_PATH);
        strcat((char *) picoTaFileName,   (const char *) picoInternalTaLingware[langIndex]);
        if((ret = pico_loadResource( picoSystem, picoTaFileName, &picoTaResource ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot load text analysis resource file (%i): %s\n", ret, outMessage);
            goto unloadTaResource;
        }

        /* Load the signal generation Lingware resource file.   */
        picoSgFileName      = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );
        strcpy((char *) picoSgFileName,   PICO_LINGWARE_PATH);
        strcat((char *) picoSgFileName,   (const char *) picoInternalSgLingware[langIndex]);
        if((ret = pico_loadResource( picoSystem, picoSgFileName, &picoSgResource ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot load signal generation Lingware resource file (%i): %s\n", ret, outMessage);
            goto unloadSgResource;
        }

        /* Load the utpp Lingware resource file if exists - NOTE: this file is optional
        and is currently not used. Loading is only attempted for future compatibility.
        If this file is not present the loading will still succeed.                      //
        picoUtppFileName      = (pico_Char *) malloc( PICO_MAX_DATAPATH_NAME_SIZE + PICO_MAX_FILE_NAME_SIZE );
        strcpy((char *) picoUtppFileName,   PICO_LINGWARE_PATH);
        strcat((char *) picoUtppFileName,   (const char *) picoInternalUtppLingware[langIndex]);
        ret = pico_loadResource( picoSystem, picoUtppFileName, &picoUtppResource );
        pico_getSystemStatusMessage(picoSystem, ret, outMessage);
        printf("pico_loadResource: %i: %s\n", ret, outMessage);
        */

        /* Get the text analysis resource name.     */
        picoTaResourceName  = (pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );
        if((ret = pico_getResourceName( picoSystem, picoTaResource, (char *) picoTaResourceName ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot get the text analysis resource name (%i): %s\n", ret, outMessage);
            goto unloadUtppResource;
        }

        /* Get the signal generation resource name. */
        picoSgResourceName  = (pico_Char *) malloc( PICO_MAX_RESOURCE_NAME_SIZE );
        if((ret = pico_getResourceName( picoSystem, picoSgResource, (char *) picoSgResourceName ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot get the signal generation resource name (%i): %s\n", ret, outMessage);
            goto unloadUtppResource;
        }


        /* Create a voice definition.   */
        if((ret = pico_createVoiceDefinition( picoSystem, (const pico_Char *) PICO_VOICE_NAME ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot create voice definition (%i): %s\n", ret, outMessage);
            goto unloadUtppResource;
        }

        /* Add the text analysis resource to the voice. */
        if((ret = pico_addResourceToVoiceDefinition( picoSystem, (const pico_Char *) PICO_VOICE_NAME, picoTaResourceName ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot add the text analysis resource to the voice (%i): %s\n", ret, outMessage);
            goto unloadUtppResource;
        }

        /* Add the signal generation resource to the voice. */
        if((ret = pico_addResourceToVoiceDefinition( picoSystem, (const pico_Char *) PICO_VOICE_NAME, picoSgResourceName ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot add the signal generation resource to the voice (%i): %s\n", ret, outMessage);
            goto unloadUtppResource;
        }

        /* Create a new Pico engine. */
        if((ret = pico_newEngine( picoSystem, (const pico_Char *) PICO_VOICE_NAME, &picoEngine ))) {
            pico_getSystemStatusMessage(picoSystem, ret, outMessage);
            fprintf(stderr, "Cannot create a new pico engine (%i): %s\n", ret, outMessage);
            goto disposeEngine;
        }

        local_text = (pico_Char *) text ;
        text_remaining = strlen((const char *) local_text) + 1;

        inp = (pico_Char *) local_text;

        size_t bufused = 0;

        picoos_Common common = (picoos_Common) pico_sysGetCommon(picoSystem);

        picoos_SDFile sdOutFile = NULL;

        picoos_bool done = TRUE;
        if(TRUE != (done = picoos_sdfOpenOut(common, &sdOutFile,
            (picoos_char *) wavefile, SAMPLE_FREQ_16KHZ, PICOOS_ENC_LIN)))
        {
            fprintf(stderr, "Cannot open output wave file\n");
            ret = 1;
            goto disposeEngine;
        }

        /* synthesis loop   */
        while (text_remaining) {
            /* Feed the text into the engine.   */
            if((ret = pico_putTextUtf8( picoEngine, inp, text_remaining, &bytes_sent ))) {
                pico_getSystemStatusMessage(picoSystem, ret, outMessage);
                fprintf(stderr, "Cannot put Text (%i): %s\n", ret, outMessage);
                goto disposeEngine;
            }

            text_remaining -= bytes_sent;
            inp += bytes_sent;

            do {
                if (picoSynthAbort) {
                    goto disposeEngine;
                }
                /* Retrieve the samples and add them to the buffer. */
                getstatus = pico_getData( picoEngine, (void *) outbuf,
                        MAX_OUTBUF_SIZE, &bytes_recv, &out_data_type );
                if((getstatus !=PICO_STEP_BUSY) && (getstatus !=PICO_STEP_IDLE)){
                    pico_getSystemStatusMessage(picoSystem, getstatus, outMessage);
                    fprintf(stderr, "Cannot get Data (%i): %s\n", getstatus, outMessage);
                    goto disposeEngine;
                }
                if (bytes_recv) {
                    if ((bufused + bytes_recv) <= bufferSize) {
                        memcpy(buffer+bufused, (int8_t *) outbuf, bytes_recv);
                        bufused += bytes_recv;
                    } else {
                        done = picoos_sdfPutSamples(
                                            sdOutFile,
                                            bufused / 2,
                                            (picoos_int16*) (buffer));
                        bufused = 0;
                        memcpy(buffer, (int8_t *) outbuf, bytes_recv);
                        bufused += bytes_recv;
                    }
                }
            } while (PICO_STEP_BUSY == getstatus);
            /* This chunk of synthesis is finished; pass the remaining samples. */
            if (!picoSynthAbort) {
                        done = picoos_sdfPutSamples(
                                            sdOutFile,
                                            bufused / 2,
                                            (picoos_int16*) (buffer));
            }
            picoSynthAbort = 0;
        }

        if(TRUE != (done = picoos_sdfCloseOut(common, &sdOutFile)))
        {
            fprintf(stderr, "Cannot close output wave file\n");
            ret = 1;
            goto disposeEngine;
        }

    disposeEngine:
        if (picoEngine) {
            pico_disposeEngine( picoSystem, &picoEngine );
            pico_releaseVoiceDefinition( picoSystem, (pico_Char *) PICO_VOICE_NAME );
            picoEngine = NULL;
        }
    unloadUtppResource:
        if (picoUtppResource) {
            pico_unloadResource( picoSystem, &picoUtppResource );
            picoUtppResource = NULL;
        }
    unloadSgResource:
        if (picoSgResource) {
            pico_unloadResource( picoSystem, &picoSgResource );
            picoSgResource = NULL;
        }
    unloadTaResource:
        if (picoTaResource) {
            pico_unloadResource( picoSystem, &picoTaResource );
            picoTaResource = NULL;
        }
    terminate:
        if (picoSystem) {
            pico_terminate(&picoSystem);
            picoSystem = NULL;
        }
        exit(ret);
    }else{
        if(!(strcmp(lang, "en-US") == 0 || strcmp(lang, "en-GB") == 0 || strcmp(lang, "ger") == 0 )){
            fprintf(stderr, "Language wasn't supported yet, please use an other method for this language.\n");
            poptPrintHelp(optCon, stderr, 0);
            poptFreeContext(optCon);
            return 1;
        }
        // Validate required arguments
        if (!tacotron_path || !melgan_path || !processor_path || !text || !wavefile) {
            fprintf(stderr, "Missing required arguments.\n");
            poptPrintHelp(optCon, stderr, 0);
            poptFreeContext(optCon);
            return 1;
        }
        poptFreeContext(optCon);

        #ifdef ENABLE_TENSORFLOW
            // Initialize TTS model
            TTSContext* tts = tts_initialize(tacotron_path, melgan_path,
                                            processor_path, lang);
            if (!tts) {
                fprintf(stderr, "Failed to initialize TTS model\n");
                return 1;
            }

            // Configure TTS parameters
            TTSConfig config = {
                .energy_ratio = 1.0f,
                .speaker_id = 0,
                .f0_ratio = 1.0f,
                .speed_ratio = 1.0f
            };
            // Initialize with default config
            if (strcmp(lang, "en-US") == 0 || strcmp(lang, "en-GB") == 0 ){
                TTSConfig config = {
                    .energy_ratio = 1.0f,
                    .speaker_id = 0,
                    .f0_ratio = 1.0f,
                    .speed_ratio = 1.0f
                };
            }else if(strcmp(lang, "ger") == 0){
                TTSConfig config = {
                    .speaker_id = 0,
                };
            }
            
            tts_configure(tts, &config);

            // Generate audio
            float* audio_buffer = NULL;
            size_t audio_size = 0;
            
            if (tts_generate_audio(tts, text, &audio_buffer, &audio_size, lang) != 0) {
                fprintf(stderr, "Failed to synthesize speech\n");
                tts_cleanup(tts);
                return 1;
            }


            FILE* fp = fopen(wavefile, "wb");
            if (!fp) {
                fprintf(stderr, "Failed to open output file: %s\n", wavefile);
                free(audio_buffer);
                tts_cleanup(tts);
                return 1;
            }

            write_float_wav(wavefile, audio_buffer, audio_size);


            free(audio_buffer);
            tts_cleanup(tts);
        #else
                fprintf(stderr, "TensorFlow support not enabled. Use --enable-tensorflow when configuring.\n");
                return 1;
        #endif
    
    }
}

