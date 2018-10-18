#
# Installation of it-IT for the Pico TTS engine in the system image
# 
# Include this file in a product makefile to include the language files for it-IT
#
# Note the destination path matches that used in external/svox/pico/tts/com_svox_picottsengine.cpp
# 

PRODUCT_COPY_FILES += \
	external/svox/pico/lang/it-IT_cm0_sg.bin:system/tts/lang_pico/it-IT_cm0_sg.bin \
	external/svox/pico/lang/it-IT_ta.bin:system/tts/lang_pico/it-IT_ta.bin

