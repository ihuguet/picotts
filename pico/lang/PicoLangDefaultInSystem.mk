#
# Installation of a default language for the Pico TTS engine
# 
# Include this file in a product makefile to include the language files for english US
#
# Note the destination path matches that used in external/svox/pico/tts/com_svox_picottsengine.cpp
# 

PRODUCT_COPY_FILES += \
	external/svox/pico/lang/en-US_lh0_sg.bin:system/tts/lang_pico/en-US_lh0_sg.bin \
	external/svox/pico/lang/en-US_ta.bin:system/tts/lang_pico/en-US_ta.bin

