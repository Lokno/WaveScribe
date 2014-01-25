// Author: Jonathan Decker
// Usage:  hidden.go bits input.png [hidden.png] output.png
// Description: Places images into the lower bits of a image
// or normalizes the lower bit of an image to reveal an encoded image

package main

import (
	"fmt"
	"image"
	"image/color"
	"image/png"
	"math"
	"os"
	"runtime"
	"strconv"
	"time"
)

func removeHidden(dst *image.RGBA, src image.Image, x0, y0, w, h, bits int, done chan bool) {

	if bits == 0 {
		bits = 1
	}

	if bits > 7 {
		bits = 7
	}

	mask := uint8(0xff) >> uint8(8-bits)

	divisor := math.Pow(2.0, float64(bits)) - 1.0

	// set the color
	for x := x0; x < x0+w; x++ {
		for y := y0; y < y0+h; y++ {
			c := color.NRGBAModel.Convert(src.At(x, y)).(color.NRGBA)

			c.R = uint8(255.0 * float64(c.R&mask) / divisor)
			c.G = uint8(255.0 * float64(c.G&mask) / divisor)
			c.B = uint8(255.0 * float64(c.B&mask) / divisor)

			dst.SetRGBA(x, y, color.RGBA{c.R, c.G, c.B, uint8(255)})
		}
	}

	done <- true
}

func addHidden(dst *image.RGBA, src image.Image, hidden image.Image, x0, y0, w, h, bits int, done chan bool) {

	if bits == 0 {
		bits = 1
	}

	if bits > 7 {
		bits = 7
	}

	mask1 := uint8(0xff) >> uint8(8-bits)
	mask2 := ^mask1

	// set the color
	for x := x0; x < x0+w; x++ {
		for y := y0; y < y0+h; y++ {
			c := color.NRGBAModel.Convert(src.At(x, y)).(color.NRGBA)
			cH := color.NRGBAModel.Convert(hidden.At(x, y)).(color.NRGBA)

			c.R = (c.R & mask2) | (cH.R >> uint8(8-bits) & mask1)
			c.G = (c.G & mask2) | (cH.G >> uint8(8-bits) & mask1)
			c.B = (c.B & mask2) | (cH.B >> uint8(8-bits) & mask1)

			dst.SetRGBA(x, y, color.RGBA{c.R, c.G, c.B, uint8(255)})
		}
	}

	done <- true
}

const BSIZE int = 16

func main() {
	if len(os.Args) < 4 {
		fmt.Println("   usage image.go <bits> <input> [<hidden>] <output>")
		os.Exit(1)
	}

	bits, _ := strconv.Atoi(os.Args[1])

	isForward := len(os.Args) == 5

	infile1, inerr := os.Open(os.Args[2])

	var infile2 *os.File
	var inerr2 error

	var outfile *os.File
	var outerr error

	inerr2 = nil

	if isForward {
		infile2, inerr2 = os.Open(os.Args[3])
		outfile, outerr = os.Create(os.Args[4])
	} else {
		outfile, outerr = os.Create(os.Args[3])
	}

	runtime.GOMAXPROCS(8)

	if inerr == nil && outerr == nil && inerr2 == nil {
		img, pngerr := png.Decode(infile1)
		infile1.Close()

		var img2 image.Image
		var pngerr2 error

		pngerr2 = nil

		if isForward {
			img2, pngerr2 = png.Decode(infile2)
			infile2.Close()
		}

		if inerr == nil && outerr == nil && pngerr == nil && pngerr2 == nil {
			bnds := img.Bounds()

			w := bnds.Max.X - bnds.Min.X
			h := bnds.Max.Y - bnds.Min.Y
			rgba := image.NewRGBA(image.Rect(0, 0, w, h))

			xBlocks := w / BSIZE
			yBlocks := h / BSIZE

			if w%BSIZE != 0 {
				xBlocks++
			}

			if h%BSIZE != 0 {
				yBlocks++
			}

			runTime := 0.0

			done := make(chan bool)

			start := time.Now()

			for x := 0; x < xBlocks; x++ {
				for y := 0; y < yBlocks; y++ {
					wBlockSize := BSIZE
					hBlockSize := BSIZE

					if x == xBlocks-1 {
						wBlockSize = w - x*BSIZE
					}

					if y == yBlocks-1 {
						hBlockSize = h - y*BSIZE
					}

					if isForward {
						go addHidden(rgba, img, img2, x*BSIZE, y*BSIZE, wBlockSize, hBlockSize, bits, done)
					} else {
						go removeHidden(rgba, img, x*BSIZE, y*BSIZE, wBlockSize, hBlockSize, bits, done)
					}

				}
			}

			for x := 0; x < xBlocks; x++ {
				for y := 0; y < yBlocks; y++ {
					<-done
				}
			}

			runTime += time.Since(start).Seconds()

			fmt.Printf("%fms\n", runTime*1000)
			png.Encode(outfile, rgba)

		} else {
			fmt.Println("Decode Error")
		}

		outfile.Close()

	} else {
		fmt.Println("Open File Error")
	}
}
