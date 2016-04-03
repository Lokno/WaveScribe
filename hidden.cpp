// Author: Jonathan Decker
// Usage:  hidden.exe bits input.png [hidden.png] output.png
// Description: Places images into the lower bits of a image
// or normalizes the lower bit of an image to reveal an encoded image

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

extern "C"
{
	#define STBI_ONLY_PNG
	#define STB_IMAGE_IMPLEMENTATION
	#include "stb_image.h"
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#include "stb_image_write.h"
}

typedef union charbits
{
    struct
    {
        unsigned char hh : 2;
        unsigned char lh : 2;
        unsigned char hl : 2;
        unsigned char ll : 2;
    };
    unsigned char c;
}charbits;

void removeHiddenAny(unsigned int* pData, unsigned int width, unsigned int height, unsigned char bits)
{
	ccpng_color tempColor;

	if( bits == 0 ) bits = 1;
	if( bits > 7 ) bits = 7;

	unsigned char mask = 0xff >> (8-bits);

	float divisor = pow(2.0f,bits)-1.0f;

	unsigned int size = width*height;
	for(unsigned int i = 0; i < size; ++i)
	{
		tempColor.c = pData[i];

		tempColor.r = (unsigned char)(255.0f * (tempColor.r & mask) / divisor);
		tempColor.g = (unsigned char)(255.0f * (tempColor.g & mask) / divisor);
		tempColor.b = (unsigned char)(255.0f * (tempColor.b & mask) / divisor);

		pData[i] = tempColor.c;
	}
}

void addHiddenAny(unsigned int* pImage, unsigned int* pHidden, unsigned int width, unsigned int height, unsigned char bits)
{
	ccpng_color temp1Color;
	ccpng_color temp2Color;

	if( bits == 0 ) bits = 1;
	if( bits > 7 ) bits = 7;

	unsigned char mask1 = 0xff >> (8-bits);
	unsigned char mask2 = ~mask1;

	unsigned int size = width*height;
	for(unsigned int i = 0; i < size; ++i)
	{
		temp1Color.c = pImage[i];
		temp2Color.c = pHidden[i];

		temp1Color.r = (temp1Color.r & mask2) | (temp2Color.r >> (8-bits) & mask1);
		temp1Color.g = (temp1Color.g & mask2) | (temp2Color.g >> (8-bits) & mask1);
		temp1Color.b = (temp1Color.b & mask2) | (temp2Color.b >> (8-bits) & mask1);

		pImage[i] = temp1Color.c;
	}
}

void removeHidden(unsigned int* pData, unsigned int width, unsigned int height)
{
	ccpng_color tempColor;
	charbits  tempChannel;

	unsigned char v[4] = {0,85,170,255};

	unsigned int size = width*height;
	for(unsigned int i = 0; i < size; ++i)
	{
		tempColor.c = pData[i];

		tempChannel.c = (unsigned char)tempColor.r;
		tempColor.r = v[tempChannel.hh];

		tempChannel.c = (unsigned char)tempColor.g;
		tempColor.g = v[tempChannel.hh];

		tempChannel.c = (unsigned char)tempColor.b;
		tempColor.b = v[tempChannel.hh];

		pData[i] = tempColor.c;
	}
}

void addHidden(unsigned int* pImage, unsigned int* pHidden, unsigned int width, unsigned int height)
{
	ccpng_color temp1Color;
	ccpng_color temp2Color;
	charbits  tempChannel;

	unsigned int size = width*height;
	for(unsigned int i = 0; i < size; ++i)
	{
		temp1Color.c = pImage[i];
		temp2Color.c = pHidden[i];

		tempChannel.c = (unsigned char)temp1Color.r;
		tempChannel.hh = temp2Color.r >> 6;
		temp1Color.r = (unsigned int)tempChannel.c;

		tempChannel.c = (unsigned char)temp1Color.g;
		tempChannel.hh = temp2Color.g >> 6;
		temp1Color.g = (unsigned int)tempChannel.c;

		tempChannel.c = (unsigned char)temp1Color.b;
		tempChannel.hh = temp2Color.b >> 6;
		temp1Color.b = (unsigned int)tempChannel.c;

		pImage[i] = temp1Color.c;
	}
}

int main( int argc, char** argv )
{
	int width1;
	int height1;
	int width2;
	int height2;
	int channels1;
	int channels2;
	unsigned int* pData1;
	unsigned int* pData2;

	CCPNGInit();
	if( argc > 3 )
	{
		int bits = atoi(argv[1]);

        pData1 = (unsigned int*)stbi_load( argv[2], &width1, &height1, &channels1, 4 );
		//pData1 = CCPNGReadFile(argv[2], &width1, &height1);

		if( pData1 != NULL )
		{
			if( argc > 4 )
			{
				pData2 = (unsigned int*)stbi_load( argv[3], &width2, &height2, &channels2, 4 );
				//pData2 = CCPNGReadFile(argv[3], &width2, &height2);

				if( pData2 != NULL && width1 == width2 && height1 == height2 )
				{
					printf("Placing %s into the lower bits of %s and saving the result to %s\n", argv[2], argv[3], argv[4]);
					addHiddenAny(pData1,pData2,width1,height1,bits);

					stbi_write_png( argv[4], width1, height1, channels1, pData1, 4*width);
					//CCPNGWriteFile(argv[4], pData1, width1, height1, 0, 1);
					free(pData2);
				}
				else
				{
					printf("Error: Bad Hidden Image (needs to be the same size as source image)\n");
				}
			}
			else
			{
				printf("Normalizing the lower bits of %s and saving the result to %s\n", argv[2], argv[3]);
				removeHiddenAny(pData1,width1,height1,bits);

				stbi_write_png( argv[3], width1, height1, channels, pData1, 4*width);
				//CCPNGWriteFile(argv[3], pData1, width1, height1, 0, 1);
			}
			
			free(pData1);
		}
		else
		{
			printf("Error: Bad Source Image (needs to be the same size as source image)\n");
		}
	}
	else
	{
		printf("   usage: bits source.png [hidden.png] output.png\n");
	}

	//CCPNGDestroy();

	return 0;
}