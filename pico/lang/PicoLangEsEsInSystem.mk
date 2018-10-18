#
# Installation of es-ES for the Pico TTS engine in the system image
# 
# Include this file in a product makefile to include the language files for es-ES
#
# Note the destination path matches that used in external/svox/pico/tts/com_svox_picottsengine.cpp
# 

PRODUCT_COPY_FILES += \
	external/svox/pico/lang/es-ES_zl0_sg.bin:system/tts/lang_pico/es-ES_zl0_sg.bin \
	external/svox/pico/lang/es-ES_ta.bin:system/tts/lang_pico/es-ES_ta.bin

