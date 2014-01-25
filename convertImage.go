// Converts an image between PNG and JPEG formats
// Optionally takes a quality setting for JPEG conversion
// usage convertImage.go <input>[.png|.jpg] <output>[.png|.jpg] [quality]

package main

import (
	"fmt"
	"image"
	"image/color"
	"image/jpeg"
	"image/png"
	"os"
	"strconv"
	"strings"
)

func main() {
	if len(os.Args) < 3 {
		fmt.Println("   usage: convertImage.go <input>[.png|.jpg] <output>[.png|.jpg] [quality]")
		os.Exit(1)
	}

	var quality int = 75

	if len(os.Args) >= 4 {
		quality, _ = strconv.Atoi(os.Args[3])
		if quality < 0 {
			quality = 0
		}
		if quality > 100 {
			quality = 100
		}
	}

	fromPNG := true
	toPNG := true

	infile, inerr := os.Open(os.Args[1])
	outfile, outerr := os.Create(os.Args[2])

	if !strings.Contains(os.Args[1], ".png") {
		fromPNG = false
	}

	if !strings.Contains(os.Args[2], ".png") {
		toPNG = false
	}

	if inerr == nil && outerr == nil {

		var img image.Image
		var imgerr error

		if fromPNG {
			img, imgerr = png.Decode(infile)
		} else {
			img, imgerr = jpeg.Decode(infile)
		}

		if imgerr == nil {
			if toPNG {

				bnds := img.Bounds()

				w := bnds.Max.X - bnds.Min.X
				h := bnds.Max.Y - bnds.Min.Y
				rgba := image.NewRGBA(image.Rect(0, 0, w, h))

				for x := 0; x < w; x++ {
					for y := 0; y < h; y++ {
						r, g, b, a := img.At(x, y).RGBA()

						rgba.SetRGBA(x, y, color.RGBA{uint8(r), uint8(g), uint8(b), uint8(a)})
					}
				}

				png.Encode(outfile, rgba)
			} else {
				jpeg.Encode(outfile, img, &jpeg.Options{quality})
			}
		} else {
			fmt.Println("Decode Error")
		}
	} else {
		fmt.Println("Error: Could not open files")
	}

	outfile.Close()
}
