#include "ttsdlbased.h"
#include <tensorflow/lite/c/c_api.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "text_processor.h"

// Function declarations for TF Lite Flex delegate
extern "C" {
    TfLiteDelegate* TfLiteFlexDelegateCreate();
    void TfLiteFlexDelegateDelete(TfLiteDelegate* delegate);
}

// Implementation based on liteinference.py workflow
TTSContext* tts_initialize(const char* text2mel_model_path, 
                        const char* vocoder_model_path,
                        const char* processor_path, 
                        const char* lang) {
    TTSContext* ctx = nullptr;
    TfLiteInterpreterOptions* options = nullptr;
    TfLiteDelegate* flex_delegate = nullptr;
    TfLiteModel* text2mel_model = nullptr;
    TfLiteModel* vocoder_model = nullptr;

    // Create context
    ctx = (TTSContext*)malloc(sizeof(TTSContext));
    if (!ctx) return nullptr;
    
    // Store processor path
    ctx->processor_path = strdup(processor_path);
    if (!ctx->processor_path) {
        tts_cleanup(ctx);
        return nullptr;
    }

    // Initialize with default config
    if (strcmp(lang, "en-US") == 0 || strcmp(lang, "en-GB") == 0 ){
        ctx->config.energy_ratio = 1.0f;
        ctx->config.speaker_id = 0;
        ctx->config.f0_ratio = 1.0f;
        ctx->config.speed_ratio = 1.0f;
    }else if(strcmp(lang, "ger") == 0){
        ctx->config.speaker_id = 0;
    }

    // Create interpreter options
    options = TfLiteInterpreterOptionsCreate();
    if (!options) {
        tts_cleanup(ctx);
        return nullptr;
    }

        
    // Load text2mel model
    text2mel_model = TfLiteModelCreateFromFile(text2mel_model_path);
    if (!text2mel_model) {
        if (flex_delegate) TfLiteFlexDelegateDelete(flex_delegate);
        TfLiteInterpreterOptionsDelete(options);
        tts_cleanup(ctx);
        return nullptr;
    }

    ctx->text2mel_interpreter = TfLiteInterpreterCreate(text2mel_model, options);
    TfLiteModelDelete(text2mel_model);
    
    if (!ctx->text2mel_interpreter) {
        if (flex_delegate) TfLiteFlexDelegateDelete(flex_delegate);
        TfLiteInterpreterOptionsDelete(options);
        tts_cleanup(ctx);
        return nullptr;
    }
    
    // Load vocoder model
    vocoder_model = TfLiteModelCreateFromFile(vocoder_model_path);
    if (!vocoder_model) {
        if (flex_delegate) TfLiteFlexDelegateDelete(flex_delegate);
        TfLiteInterpreterOptionsDelete(options);
        tts_cleanup(ctx);
        return nullptr;
    }
    
    ctx->vocoder_interpreter = TfLiteInterpreterCreate(vocoder_model, options);
    TfLiteModelDelete(vocoder_model);
    
    if (!ctx->vocoder_interpreter) {
        if (flex_delegate) TfLiteFlexDelegateDelete(flex_delegate);
        TfLiteInterpreterOptionsDelete(options);
        tts_cleanup(ctx);
        return nullptr;
    }
    
    // Cleanup options and delegate
    if (flex_delegate) TfLiteFlexDelegateDelete(flex_delegate);
    TfLiteInterpreterOptionsDelete(options);
    
    // Allocate tensors for both models
    if (TfLiteInterpreterAllocateTensors(ctx->text2mel_interpreter) != kTfLiteOk ||
        TfLiteInterpreterAllocateTensors(ctx->vocoder_interpreter) != kTfLiteOk) {
        tts_cleanup(ctx);
        return nullptr;
    }

    return ctx;
}

void tts_configure(TTSContext* ctx, const TTSConfig* config) {
    if (!ctx || !config) return;
    memcpy(&ctx->config, config, sizeof(TTSConfig));
}
int tts_generate_audio(TTSContext* ctx,
                    const char* text,
                    float** audio_out,
                    size_t* audio_length,
                    const char* lang) {
    if (!ctx || !text || !audio_out || !audio_length ) 
        return -1;

    // 1. Text to input IDs using processor
    size_t input_length = 0;
    int ret = init_text_processor(ctx->processor_path);
    if (ret != 0) {
        fprintf(stderr, "Failed to initialize text processor\n");
        return -1;
    }

    int* input_ids = text_to_sequence((char*)text, &input_length);
    if (!input_ids) {
        fprintf(stderr, "Failed to convert text to sequence\n");
        cleanup_text_processor();
        return -1;
    }

    int simple_array[1][input_length];  // VLA (C99)
    for (size_t i = 0; i < input_length; i++) {
        simple_array[0][i] = input_ids[i];
    }
    free(input_ids);

    // Declare all indices outside the conditional block
    int energy_index = -1, f0_index = -1, speed_index = -1;
    int text_index = -1, speaker_index = -1, length_index = -1;

    // 2. Assign indices based on language
    if (strcmp(lang, "en-US") == 0 || strcmp(lang, "en-GB") == 0) {
        energy_index  = 0;
        speaker_index = 1;
        f0_index      = 2;
        speed_index   = 3;
        text_index    = 4;
    } else if (strcmp(lang, "ger") == 0) {
        text_index    = 0;
        length_index  = 1;
        speaker_index = 2;
    } else {
        fprintf(stderr, "Unsupported language: %s\n", lang);
        cleanup_text_processor();
        return -1;
    }

    // 3. Resize and allocate text2mel input
    int dims[2] = {1, (int)input_length};
    TfLiteStatus status = TfLiteInterpreterResizeInputTensor(ctx->text2mel_interpreter, text_index, dims, 2);
    if (status != kTfLiteOk) {
        fprintf(stderr, "ResizeInputTensor failed for text input\n");
        cleanup_text_processor();
        return -1;
    }

    status = TfLiteInterpreterAllocateTensors(ctx->text2mel_interpreter);
    if (status != kTfLiteOk) {
        fprintf(stderr, "AllocateTensors failed\n");
        cleanup_text_processor();
        return -1;
    }

    // 4. Fill tensors
    if (strcmp(lang, "en-US") == 0 || strcmp(lang, "en-GB") == 0) {
        TfLiteTensor* energy_tensor  = TfLiteInterpreterGetInputTensor(ctx->text2mel_interpreter, energy_index);
        TfLiteTensor* f0_tensor      = TfLiteInterpreterGetInputTensor(ctx->text2mel_interpreter, f0_index);
        TfLiteTensor* speed_tensor   = TfLiteInterpreterGetInputTensor(ctx->text2mel_interpreter, speed_index);

        TfLiteTensorCopyFromBuffer(energy_tensor, &ctx->config.energy_ratio, sizeof(float));
        TfLiteTensorCopyFromBuffer(f0_tensor, &ctx->config.f0_ratio, sizeof(float));
        TfLiteTensorCopyFromBuffer(speed_tensor, &ctx->config.speed_ratio, sizeof(float));
    } else if (strcmp(lang, "ger") == 0) {
        TfLiteTensor* length_tensor = TfLiteInterpreterGetInputTensor(ctx->text2mel_interpreter, length_index);
        TfLiteTensorCopyFromBuffer(length_tensor, &input_length, sizeof(int32_t));
    }

    TfLiteTensor* speaker_tensor = TfLiteInterpreterGetInputTensor(ctx->text2mel_interpreter, speaker_index);
    TfLiteTensor* text_tensor    = TfLiteInterpreterGetInputTensor(ctx->text2mel_interpreter, text_index);

    TfLiteTensorCopyFromBuffer(speaker_tensor, &ctx->config.speaker_id, sizeof(int32_t));
    TfLiteTensorCopyFromBuffer(text_tensor, simple_array, input_length * sizeof(int32_t));

    // 5. Run text2mel
    if (TfLiteInterpreterInvoke(ctx->text2mel_interpreter) != kTfLiteOk) {
        cleanup_text_processor();
        return -1;
    }
    // 6. Get mel output
    const TfLiteTensor* mel_tensor = TfLiteInterpreterGetOutputTensor(ctx->text2mel_interpreter, 1);
    float* mel_outputs = (float*)TfLiteTensorData(mel_tensor);
    int mel_size = TfLiteTensorByteSize(mel_tensor) / sizeof(float);
    
    int mel_dims[3];
    mel_dims[0] = 1;
    for (int i = 1; i < 3; ++i)
        mel_dims[i] = TfLiteTensorDim(mel_tensor, i);
    
    TfLiteInterpreterResizeInputTensor(ctx->vocoder_interpreter, 0, mel_dims, 3);
    TfLiteInterpreterAllocateTensors(ctx->vocoder_interpreter);
    
    // 7. Run vocoder
    TfLiteTensor* vocoder_input = TfLiteInterpreterGetInputTensor(ctx->vocoder_interpreter, 0);
    TfLiteTensorCopyFromBuffer(vocoder_input, mel_outputs, mel_size * sizeof(float));
    
    if (TfLiteInterpreterInvoke(ctx->vocoder_interpreter) != kTfLiteOk) {
        cleanup_text_processor();
        return -1;
    }
    
    // 8. Extract audio
    const TfLiteTensor* audio_tensor = TfLiteInterpreterGetOutputTensor(ctx->vocoder_interpreter, 0);
    int batch = TfLiteTensorDim(audio_tensor, 0);
    int time  = TfLiteTensorDim(audio_tensor, 1);
    int ch    = TfLiteTensorDim(audio_tensor, 2);

    size_t audio_size = (size_t)batch * time * ch;
    *audio_out = (float*)malloc(audio_size * sizeof(float));
    if (!*audio_out) {
        cleanup_text_processor();
        return -1;
    }

    memcpy(*audio_out, TfLiteTensorData(audio_tensor), audio_size * sizeof(float));
    *audio_length = audio_size;

    cleanup_text_processor();
    return 0;
}



void tts_cleanup(TTSContext* ctx) {
    if (!ctx) return;
    
    if (ctx->text2mel_interpreter)
        TfLiteInterpreterDelete(ctx->text2mel_interpreter);
    
    if (ctx->vocoder_interpreter)
        TfLiteInterpreterDelete(ctx->vocoder_interpreter);
    
    free(ctx);
}