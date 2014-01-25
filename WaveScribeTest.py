# Testing script for WaveScribe
# Checks a range of encoding strength variables 
# with conversion to JPEG images of a range of qualities.

import os
import sys

def cmdecho( cmdstr ):
    print cmdstr
    os.system( cmdstr )

if len(sys.argv) < 3:
    print "    usage:  WaveScribeTest.py <test image> <test string> [strength] [quality]"
    sys.exit(0)

testImage = sys.argv[1]
testString = sys.argv[2]

if len(sys.argv) > 3 :
    strength = [float(sys.argv[3])]
else:
    strength = [0.2,0.4,0.6,0.8]

if len(sys.argv) > 4 :
    quality = [int(sys.argv[4])]
else:
    quality = [80,90,100]
    
for s in strength:
    cmdecho("Release\\WaveScribe " + str(s) + " " + testImage + " tmp\\test.png " + '"' + testString + '"');
    cmdecho("Release\\WaveScribe " + str(s) + " tmp\\test.png");
    for q in quality:
        cmdecho("go run convertImage.go tmp\\test.png tmp\\test.jpg " + str(q))
        cmdecho("go run convertImage.go tmp\\test.jpg tmp\\test_jpg.png")
        cmdecho("Release\\WaveScribe " + str(s) + " tmp\\test_jpg.png");
        print "\n",
    

