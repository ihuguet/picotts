# Pico TTS

Text to speech voice sinthesizer from SVox, included in Android AOSP.

## Build and install in Linux

The source code is inside folder 'pico'

```
cd pico
```

Create autotools files:

```
./autogen.sh
```

Configure & build:

```
./configure
make
```

Install (this install files to /usr/bin, /usr/lib and /usr/share/pico):

```
make install
```

## Usage

```
pico2wave -l LANG -w OUT_WAV_FILE "text you want to sinthesize"
aplay OUT_WAV_FILE
rm OUT_WAV_FILE
```

Languages can be: en-EN, en-GB, es-ES, de-DE, fr-FR, it-IT

Output file must be .wav

## TensorFlow TTS Support

**Dependencies**: Before building with TensorFlow support, install:
- `libtensorflowlite-c`
- `libtensorflowlite-flex`

Build with TensorFlow support:
```
cd pico

./autogen.sh

./configure --enable-tensorflow

make

make install
```

Run TTS with deep learning models:
```
pico2wave -d true -t <text_to_mel.tflite> -g <vocoder.tflite> -p processor.json -s "Hello world" -w output.wav
```

## License

License Apache-2.0 (see pico_resources/NOTICE)
