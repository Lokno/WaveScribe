# WaveScribe : Wavelet-based Blind Message Encoder #

WaveScribe is command-line utility which encodes a 32-bit string into a 512x512 PNG

For the wavelet-based data encoding scheme uses the method described in the follow paper:

[A New Robust Blind Watermarking Method Based on Neural Networks in Wavelet Transform Domain] [1]

## Usage ##

> WaveScribe strength input.png [output.png "message"]

The message is encoded into input.png and the result is saved to output.png.
If no output image is provided, the application attempts to decode a message from input.png.
The strength value indicates how strongly the data will be encoded into the image. 
It is required for encoding and decoding the image.

## Process ##

- Input image undergoes a 3 level 2D wavelet transform
- The input message is encoded using into a Reed-Solomon error correcting block
- The binary block is arranged into a 2D grid using a zig-zap pattern similar to a QR code
- LH3 and HL3 of the transformed image are manipulated to encode the 2D binary pattern
- The wavelet transform is reversed and the image is saved to output

## Issues ##

- Can only handle image of size 512x512. 
- Encoding does not seem to survive JPEG compression.

## Dependencies ##

- dwt97.c (included) Can be found in [Axonlib] [2] source
- stb_image.c : single-header image reader [STB] [3]
- stb_image_write.c : single-header image writer [STB] [3] : 
- Schifra Version 0.0.1 : Reed-Solomon error correcting code library

## Extras ##

- hidden.go       : Go-lang script that hides a second image in the lower bits of another

> usage: hidden.go bits(0-7) input.png [imageToHide.png] output.png

- hidden.cpp      : C++ source that hides a second image in the lower bits of another

> usage: hidden.exe bits(0-7) input.png [imageToHide.png] output.png

- convertImage.go : Go-lang script that converts from PNG to JPEG

> usage: convertImage.go <input>[.png|.jpg] <output>[.png|.jpg] [quality]

  [1]: http://www.idosi.org/wasj/wasj22(11)13/8.pdf
  [2]: https://code.google.com/p/axonlib/source/browse/trunk/extern/dwt97.c?spec=svn19&r=19 "dwt97.c"
  [3]: https://github.com/nothings/stb "stb"