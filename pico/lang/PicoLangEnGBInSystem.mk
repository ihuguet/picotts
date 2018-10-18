#
# Installation of en-GB for the Pico TTS engine in the system image
# 
# Include this file in a product makefile to include the language files for en-GB
#
# Note the destination path matches that used in external/svox/pico/tts/com_svox_picottsengine.cpp
# 

PRODUCT_COPY_FILES += \
	external/svox/pico/lang/en-GB_kh0_sg.bin:system/tts/lang_pico/en-GB_kh0_sg.bin \
	external/svox/pico/lang/en-GB_ta.bin:system/tts/lang_pico/en-GB_ta.bin

