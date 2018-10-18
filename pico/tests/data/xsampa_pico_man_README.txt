ReadME for the XSAMPA Pico TTS tests

The following files contain a series of utterances to test the Pico TTS engine with regards to
the support of language-specific XSAMPA characters:

 xsampa_pico_man_de-DE.txt
 xsampa_pico_man_en-GB.txt
 xsampa_pico_man_en-US.txt
 xsampa_pico_man_es-ES.txt
 xsampa_pico_man_fr-FR.txt
 xsampa_pico_man_it-IT.txt

They implement the examples given in section A.4 of the SVOX Pico Manual ("SVOX Pico - Speech Output
Engine SDK").
They are formatted as utterances that can be sent to the Pico engine through the Android 1.6 API.
This is achieved by using the TextToSpeech.speak() method of the android.speech.tts package.
The test files adopt the following syntax:
- the lines used in the test are between the lines marked "BEGIN_TEST" and "END_TEST",
- lines that start with "#" are not spoken.
The application found in external/svox/pico/tests/apps/SpeechTester lets you load those files and
run the test, i.e. play all valid test lines in the test file. Copy the test files to the
Android-powered device, and enter the file name that contains the test to run, and click "Run test".

