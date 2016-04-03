// Author: Jonathan Decker
// Usage:  WaveMark.exe strength input.png [output.png "message"]
// Description: Takes a 512x512 PNG and encodes or decodes a message

#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <string>

#include "schifra_galois_field.hpp"
#include "schifra_galois_field_polynomial.hpp"
#include "schifra_sequential_root_generator_polynomial_creator.hpp"
#include "schifra_reed_solomon_encoder.hpp"
#include "schifra_reed_solomon_decoder.hpp"
#include "schifra_reed_solomon_block.hpp"
#include "schifra_error_processes.hpp"

#ifdef _DEBUG
//#include <vld.h>
#endif

#define USE_LAB

extern "C"
{
	#define STBI_ONLY_PNG
	#define STB_IMAGE_IMPLEMENTATION
	#include "stb_image.h"
	#define STB_IMAGE_WRITE_IMPLEMENTATION
	#include "stb_image_write.h"
	#include "dwt.h"
}

typedef union
{
    struct
    {
        unsigned int r : 8;  // Red:     0/255 to 255/255
        unsigned int g : 8;  // Green:   0/255 to 255/255
        unsigned int b : 8;  // Blue:    0/255 to 255/255
        unsigned int a : 8;  // Alpha:   0/255 to 255/255
    };
    unsigned int c;
}rgbacol;

#ifdef _WIN32
static inline double round(double val)
{    
    return floor(val + 0.5);
}
#endif

static double clamp( double x, double min, double max )
{
     return x < min ? min : (x > max ? max : x);
}

static unsigned int nextPow2( unsigned int n )
{
	unsigned int temp = 1;
	while( n > temp ) temp <<= 1;
	return temp;
}

double xyzMat[] = { 0.4124, 0.3576, 0.1805,
                    0.2126, 0.7152, 0.0722,
					0.0193, 0.1192, 0.9505 };

double rgbMat[] = { 3.2406, -1.5372, -0.4986,
					-0.9689, 1.8758, 0.0415, 
					0.0557, -0.2040, 1.0570 };

#define CIEXYZ_D65_X 0.95047
#define CIEXYZ_D65_Y 1.00000
#define CIEXYZ_D65_Z 1.08883

void RGBtoXYZ( double src[3], double dst[3] )
{
    // convert to sRGB form
	if( src[0] > 0.04045 )
		src[0] = pow((src[0] + 0.055)/1.055, 2.4);
	else
		src[0] = src[0]/12.92;

	if( src[1] > 0.04045 )
		src[1] = pow((src[1] + 0.055)/1.055, 2.4);
	else
		src[1] = src[1]/12.92;

	if( src[2] > 0.04045 )
		src[2] = pow((src[2] + 0.055)/1.055, 2.4);
	else
		src[2] = src[2]/12.92;

	dst[0] = xyzMat[0] * src[0] + xyzMat[1] * src[1] + xyzMat[2] * src[2];
	dst[1] = xyzMat[3] * src[0] + xyzMat[4] * src[1] + xyzMat[5] * src[2];
	dst[2] = xyzMat[6] * src[0] + xyzMat[7] * src[1] + xyzMat[8] * src[2];
}

void XYZtoRGB( double src[3], double dst[3] )
{
	dst[0] = rgbMat[0] * src[0] + rgbMat[1] * src[1] + rgbMat[2] * src[2];
	dst[1] = rgbMat[3] * src[0] + rgbMat[4] * src[1] + rgbMat[5] * src[2];
	dst[2] = rgbMat[6] * src[0] + rgbMat[7] * src[1] + rgbMat[8] * src[2];

    for(int i=0; i<3; i++)
	{
		if( dst[i] > 0.0031308 )
			dst[i] = 1.055 * (pow(dst[i], 1.0/2.4) - 0.055);
		else
			dst[i] = 12.92*dst[i];
		dst[i] = clamp(dst[i],0.0,1.0);
	}
}

double f( double t )
{
	if( t > 0.008856451679035631 ) // (6/29)^2
	{
		return pow(t,1.0/3.0);
	}
	else
	{
		return 0.3333333333*0.008856451679035631*t + 0.13793103448275862;
	}
}

double inversef( double t )
{
	if( t > 0.20689655172413793 ) // 6/29
	{
		return t*t*t;
	}
	else
	{
		return 3 * 0.008856451679035631 * ( t - 0.13793103448275862); // 3 * (6/29)^2 * (t - 4/29.0)
	}
}

void XYZtoLab( double src[3], double dst[3] )
{
	double nx = f(src[0]/CIEXYZ_D65_X);
	double ny = f(src[1]/CIEXYZ_D65_Y);
	dst[0] = 116.0 * ny - 16.0;
	dst[1] = 500.0 * (nx - ny);
	dst[2] = 200.0 * (ny - f(src[2]/CIEXYZ_D65_Z));
}

void LabtoXYZ( double src[3], double dst[3] )
{
	dst[1] = CIEXYZ_D65_Y * inversef( (src[0]+16.0)/116.0);
	dst[0] = CIEXYZ_D65_X * inversef( (src[0]+16.0)/116.0 + src[1]/500.0);
	dst[2] = CIEXYZ_D65_Z * inversef( (src[0]+16.0)/116.0 - src[2]/200.0);
}

void RGBtoYCbCr( double src[3], double dst[3] )
{
	dst[0] =     0 +  0.299    * src[0] +  0.587    * src[1] +  0.114    * src[2];
	dst[1] = 128.0 + -0.168736 * src[0] + -0.331264 * src[1] +  0.5      * src[2];
	dst[2] = 128.0 +  0.5      * src[0] + -0.418688 * src[1] + -0.081312 * src[2];
}

void YCbCrtoRGB( double src[3], double dst[3] )
{
	double cbTemp = src[1] - 128.0;
	double crTemp = src[2] - 128.0;

	dst[0] = src[0] +                      1.402   * crTemp;
	dst[1] = src[0] + -0.34414 * cbTemp + -0.71414 * crTemp;
	dst[2] = src[0] +  1.772   * cbTemp;
}

#define MAX(a,b) (a > b ? a : b)

// NOT USED
// Stores a 2D grid of doubles into an image with uint32 pixel depth
void writeFreqImage( unsigned int* src, unsigned int* dst, double *freqs, unsigned int n )
{
    unsigned int i;
	unsigned int *p1;
	double       *p2;

	rgbacol temp;

	double minFreq = DBL_MAX;
	double maxFreq = -DBL_MAX;

    for( i = 0, p2 = freqs; i < n; ++i, ++p2 )
    {
		if( *p2 < minFreq )
			minFreq = *p2;
		if( *p2 > maxFreq )
			maxFreq = *p2;
	}

    for( i = 0, p1 = dst, p2 = freqs; i < n; ++i, ++p1, ++p2 )
    {
		double value = (*p2-minFreq) * 255.0 / (maxFreq-minFreq);

		temp.c = 0;
		temp.r = (unsigned int)value;
		temp.g = (unsigned int)value;
		temp.b = (unsigned int)value;
		temp.a = 255;

		*p1 = temp.c;
	}
}

// NOT USED
// Converts an image with uint32 pixel depth into a binary string
void convertBinaryImage( unsigned int* src, unsigned char* dst, unsigned int width, unsigned int height )
{
    unsigned int i,j;
	unsigned int  *p1;
	unsigned char *p2;

	double tempColor1[3];
	double tempColor2[3];

	rgbacol temp;

    for( i = 0, p1 = src, p2 = dst; i < height; ++i )
    {
		for( j = 0; j < width; ++j, ++p1, ++p2 )
		{
    		temp.c = *p1;

			tempColor1[0] = (temp.r / 255.0);
			tempColor1[1] = (temp.g / 255.0);
			tempColor1[2] = (temp.b / 255.0);

			RGBtoXYZ(tempColor1,tempColor2);
			XYZtoLab(tempColor2,tempColor1);

    		if( tempColor1[0] < 50.0 )
				*p2 = 0;
			else
				*p2 = 1;
		}
    }
}

// NOT USED
// Converts a binary string into an image with uint32 pixel depth
void convertBinaryImageInverse( unsigned char* src, unsigned int* dst, unsigned int width, unsigned int height )
{
    unsigned int i,j;
	unsigned char *p1;
	unsigned int *p2;

	rgbacol temp;

    for( i = 0, p1 = src, p2 = dst; i < height; ++i )
    {
		for( j = 0; j < width; ++j, ++p1, ++p2 )
		{
			unsigned char v = *p1 ? 255 : 0;

			temp.r = v;
			temp.g = v;
			temp.b = v;
			temp.a = 255;

			*p2 = temp.c;
		}
    }
}

// encode byte into binary matrix using a zig-zagging pattern
//
//        upDir  !upDir
//  X--    7 6    1 0
//  |      5 4    3 2
//  |      3 2    5 4
//  |      1 0    7 6
void setByte( unsigned char* dst, const unsigned char c, int width, int height, bool upDir )
{
	int step;

	bool zag = true;

	if( upDir )
	{
		dst += width*3+1;
		step = -width + 1;
	}
	else
	{
		dst++;
		step = width + 1;
	}

	for( int i = 0 ; i < 8; ++i )
	{
		*dst = ((1 << (7-i)) & c) ? 1 : 0;

		dst += zag ? -1 : step;

		zag = !zag;
	}
}

// decode byte from binary matrix using a zig-zagging pattern

//        upDir  !upDir
//  X--    7 6    1 0
//  |      5 4    3 2
//  |      3 2    5 4
//  |      1 0    7 6
unsigned char getByte( unsigned char* src, int width, int height, bool upDir )
{
	int step;
	unsigned char c = 0;
	bool zag = true;

	if( upDir )
	{
		src += width*3+1;
		step = -width + 1;
	}
	else
	{
		src++;
		step = width + 1;
	}

	for( int i = 0 ; i < 8; ++i )
	{
		c |= *src ? (1 << (7-i)) : 0;

		src += zag ? -1 : step;

		zag = !zag;
	}

	return c;
}

// converts 255 byte block 2D binary matrix
// block is encoded in a zig-zagging pattern

void convertBufferToBinaryMatrix( const char* block, unsigned char* dst, unsigned int width, unsigned int height )
{
	if( width % 2 != 0 || height % 4 != 0 )
	{
		memset(dst,0,sizeof(unsigned char)*width*height);
		return;
	}

	int bw = width >> 1;
	int bh = height >> 2;
	int k = 0;
	bool upDir = true;

	memset(dst,0,sizeof(unsigned char)*width*height);

	for( int i = bw-1; i >= 0 ; --i )
	{
		for( int j = bh-1; j >= 0; --j, ++k )
		{
			setByte(dst + j*4*width+i*2, block[k], width, height, upDir );
		}
		upDir = !upDir;
	}
}

// 2D binary matrix into a 255 byte block 
// block is encoded in a zig-zagging pattern
// each bit in each quadriant is vote of the final value of that bit in the result

void convertBinaryMatrixToBuffer( char* block, unsigned char* src, unsigned int width, unsigned int height )
{
	if( width % 2 != 0 || height % 4 != 0 )
	{
		memset(src,0,sizeof(unsigned char)*width*height);
		return;
	}

	int bw = width >> 1;
	int bh = height >> 2;
	int k = 0;
	bool upDir = true;

	for( int i = bw-1; i >= 0 ; --i )
	{
		for( int j = bh-1; j >= 0; --j, ++k )
		{
			block[k] = (char)getByte(src + j*4*width+i*2, width, height, upDir );
		}
		upDir = !upDir;
	}
}

void encodeStringIntoBinaryMatrix( const char* str, unsigned char* dst, unsigned int width, unsigned int height )
{

   /* Finite Field Parameters */
   const std::size_t field_descriptor                 =   8;
   const std::size_t generator_polynommial_index      = 120;
   const std::size_t generator_polynommial_root_count =  96;

   /* Reed Solomon Code Parameters */
   const std::size_t code_length = 128;
   const std::size_t fec_length  =  96;
   const std::size_t data_length = code_length - fec_length;

   /* Instantiate Finite Field and Generator Polynomials */
   schifra::galois::field field(field_descriptor,
                                schifra::galois::primitive_polynomial_size06,
                                schifra::galois::primitive_polynomial06);

   schifra::galois::field_polynomial generator_polynomial(field);

   schifra::sequential_root_generator_polynomial_creator(field,
                                                         generator_polynommial_index,
                                                         generator_polynommial_root_count,
                                                         generator_polynomial);

   /* Instantiate Encoder and Decoder (Codec) */
   schifra::reed_solomon::shortened_encoder<code_length,fec_length> encoder(field,generator_polynomial);

   std::string message = str;
               message = message + std::string(data_length - message.length(),static_cast<unsigned char>(0x00));

   /* Instantiate RS Block For Codec */
   schifra::reed_solomon::block<code_length,fec_length> block;

   /* Transform message into Reed-Solomon encoded codeword */
   if (!encoder.encode(message,block))
   {
      std::cout << "Error - Critical encoding failure!" << std::endl;
	  memset(dst,0,sizeof(unsigned char)*width*height);
      return;
   }
   
   std::string block_str;
   std::string data_str(data_length,static_cast<unsigned char>(0x00));
   std::string fec_str(fec_length,static_cast<unsigned char>(0x00));
   block.data_to_string(data_str);
   block.fec_to_string(fec_str);
   block_str = data_str + fec_str;

   convertBufferToBinaryMatrix(block_str.c_str(), dst, width, height);
}

void decodeBinaryMatrixAsString( unsigned char* src, char* dst, unsigned int width, unsigned int height )
{
   /* Finite Field Parameters */
   const std::size_t field_descriptor                 =   8;
   const std::size_t generator_polynommial_index      = 120;
   const std::size_t generator_polynommial_root_count =  96;

   /* Reed Solomon Code Parameters */
   const std::size_t code_length = 128;
   const std::size_t fec_length  =  96;
   const std::size_t data_length = code_length - fec_length;

   /* Instantiate Finite Field and Generator Polynomials */
   schifra::galois::field field(field_descriptor,
                                schifra::galois::primitive_polynomial_size06,
                                schifra::galois::primitive_polynomial06);

   schifra::galois::field_polynomial generator_polynomial(field);

   schifra::sequential_root_generator_polynomial_creator(field,
                                                         generator_polynommial_index,
                                                         generator_polynommial_root_count,
                                                         generator_polynomial);

   /* Instantiate Decoder (Codec) */
   schifra::reed_solomon::shortened_decoder<code_length,fec_length> decoder(field,generator_polynommial_index);

   char tempStr[128];

   convertBinaryMatrixToBuffer(tempStr, src, width, height);

   /* Instantiate RS Block For Codec */
   std::string data_str(32,0x00);
   std::string fec_str(96,0x00);

    for (std::size_t i = 0; i < 32; ++i)
	{
		data_str[i] = static_cast<char>(tempStr[i]);
	}

	for (std::size_t i = 0; i < 96; ++i)
	{
		fec_str[i] = static_cast<char>(tempStr[32+i]);
	}

   schifra::reed_solomon::block<code_length,fec_length> block(data_str,fec_str);

   if(!decoder.decode(block))
   {
      std::cout << "Error - Critical decoding failure!" << std::endl;
#ifdef _WIN32
	  sprintf_s(dst,32,"ERROR");
#else
	  sprintf(dst,"ERROR");
#endif
   }
   else
   {
	   block.data_to_string(data_str);

	   bool badString = false;

	   memcpy(dst, data_str.c_str(), 32*sizeof(char));

		for (std::size_t i = 0; i < 32; ++i)
		{
			unsigned char temp = static_cast<unsigned char>(dst[i]);
			if( temp < 32 || temp > 126 ) // Replaced unexpected character range with space
			{
				badString = true;
				dst[i] = (char)32U;
			}
		}
   }
}

// accending order
void sortVec4( double* c, unsigned int* i )
{
	double dTemp;
	unsigned int iTemp;

	i[0] = 0; i[1] = 1; i[2] = 2; i[3] = 3;

	// sort left set
	if( c[0] > c[1] ) 
	{
		dTemp = c[0];
		c[0] = c[1];
		c[1] = dTemp;

		iTemp = i[0];
		i[0] = i[1];
		i[1] = iTemp;
	}

	// sort right set
	if( c[2] > c[3] ) 
	{
		dTemp = c[2];
		c[2] = c[3];
		c[3] = dTemp;

		iTemp = i[2];
		i[2] = i[3];
		i[3] = iTemp;
	}

	// sort lowest to front
	if( c[0] > c[2] )
	{
		dTemp = c[0];
		c[0] = c[2];
		c[2] = dTemp;

		iTemp = i[0];
		i[0] = i[2];
		i[2] = iTemp;
	}

	// sort highest to back
	if( c[1] > c[3] )
	{
		dTemp = c[3];
		c[3] = c[1];
		c[1] = dTemp;

		iTemp = i[3];
		i[3] = i[1];
		i[1] = iTemp;
	}

	// sort middle values
	if( c[1] > c[2] )
	{
		dTemp = c[1];
		c[1] = c[2];
		c[2] = dTemp;

		iTemp = i[1];
		i[1] = i[2];
		i[2] = iTemp;
	}
}

void encodeBit( double c[4], unsigned int i[4], unsigned char b, double markStrength )
{
	double delta,distance,newDistance,change;

	sortVec4(c,i);

	delta = (c[3] - c[0]) * 0.5 * markStrength;

	distance = 0;
	if( delta != 0 )
		distance = (c[2] - c[1]) / delta;

	newDistance = floor(distance);
	if( ((int)newDistance) % 2 == 0 )
	{
		if( b == 1 ) 
			newDistance += 1.0;
	}
	else
	{
		if( b == 0 ) 
			newDistance += 1.0;
	}

	change = (newDistance - distance) * 0.5 * delta;

	c[1] = c[1] - change;
	c[2] = c[2] + change;
}

double getDistance( double c[4], double markStrength )
{
	double delta;

	unsigned int idx[4];

	sortVec4(c,idx);

	delta = (c[3] - c[0]) * 0.5 * markStrength;
	return delta != 0 ? (c[2] - c[1]) / delta : 0;
}

void encodeMark( double* freqs, unsigned char* mark, unsigned int width, unsigned int height, unsigned int markSize, double markStrength = 0.5 )
{
	unsigned int vecInLine = markSize/2;
	unsigned int levelSize = markSize*2;
	unsigned int hl3Offset = levelSize;
	unsigned int lh3Offset = width*levelSize;

	unsigned int i,j,k;
	double *p1;
	unsigned char *p2;

	double v[4];
	unsigned idx[4];

	unsigned int size = 4*sizeof(double);

	// LH3 
    for( i = 0, p1 = freqs+lh3Offset, p2 = mark; i < levelSize; ++i )
    {
		for( j = 0; j < vecInLine; ++j, p1+=4, ++p2 )
		{
			memcpy(v,p1,size);

			encodeBit(v,idx,*p2,markStrength);

			// place coefficents back into matrix in their original order
			for( k = 0; k < 4; ++k )
				p1[idx[k]] = v[k];
		}

		// skip to the next row of the lh3 cell
		p1+=width-levelSize;
	}

	// HL3 
    for( i = 0, p1 = freqs+hl3Offset, p2 = mark; i < levelSize; ++i )
    {
		for( j = 0; j < vecInLine; ++j, ++p2 )
		{
			for( k = 0; k < 4; p1+=width, ++k )
				v[k] = *p1;

			encodeBit(v,idx,*p2,markStrength);

			// place coefficents back into matrix in their original order
			p1 -= 4*width;
			for( k = 0; k < 4; ++k )
				*(p1+width*idx[k]) = v[k];
			p1 += 4*width;
		}

		// move to beginning of the next column
		p1=freqs+hl3Offset+i+1;
	}
}

void decodeMark( double* freqs, unsigned char* mark, double* buffer1, double* buffer2, unsigned int width, unsigned int height, unsigned int markSize, double markStrength = 0.5 )
{
	unsigned int vecInLine = markSize/2;
	unsigned int levelSize = markSize*2;
	unsigned int hl3Offset = levelSize;
	unsigned int lh3Offset = width*levelSize;

	unsigned int i,j,k;
	double *p1;
	double *p2;

	unsigned char *p3;

	double v[4];

	unsigned int markLength = markSize*markSize;

	unsigned int size = 4*sizeof(double);

	// LH3 
    for( i = 0, p1 = freqs+lh3Offset, p2 = buffer1; i < levelSize; ++i )
    {
		// row
		for( j = 0; j < vecInLine; ++j, p1+=4, ++p2 )
		{
			memcpy(v,p1,size);
			*p2 = getDistance(v,markStrength);
		}

		// skip to the next row of the lh3 cell
		p1+=width-levelSize;
	}

	// HL3 
    for( i = 0, p1 = freqs+hl3Offset, p2 = buffer2; i < levelSize; ++i )
    {
		// column
		for( j = 0; j < vecInLine; ++j, ++p2 )
		{
			for( k = 0; k < 4; p1+=width, ++k )
				v[k] = *p1;

			*p2 = getDistance(v,markStrength);
		}

		// move to beginning of the next column
		p1=freqs+hl3Offset+i+1;
	}

	// fuzzy mean
	for( i = 0, p1 = buffer1, p2 = buffer2, p3 = mark; i < markLength; ++i, ++p1, ++p2, ++p3 )
	{
		double belief1 = 1 - 2 * fabs(*p1 - round(*p1));
		double belief2 = 1 - 2 * fabs(*p2 - round(*p2));

		double vote1 = ((int)round(*p1)) % 2 == 0 ? -1 : 1;
		double vote2 = ((int)round(*p2)) % 2 == 0 ? -1 : 1;

		double div = belief1 * vote1 + belief2 * vote2;

		*p3 = div < 0 ? 0 : 1;
	}
}
void decomposeImage( double* data, double* columnBuffer, unsigned int levels, unsigned int width, unsigned int height)
{
	unsigned int i,j,k;
	double       *p1;
	double       *p2;

	for( k = 0; k < 3; ++k )
	{
		// decompose rows
		for( i = 0; i < height>>k; ++i )
		{
			fwt97(data+i*width, width>>k);
		}

		// decompose columns
		for( j = 0; j < width>>k; ++j )
		{
			for( i = 0, p1 = data+j, p2 = columnBuffer; i < height>>k; ++i, p1+=width, ++p2 )
				*p2 = *p1;

			fwt97(columnBuffer, height>>k);

			for( i = 0, p1 = data+j, p2 = columnBuffer; i < height>>k; ++i, p1+=width, ++p2 )
				*p1 = *p2;
		}
	}
}


void reconstructImage( double* data, double* columnBuffer, unsigned int levels, unsigned int width, unsigned int height)
{
	unsigned int i,j,k;
	double       *p1;
	double       *p2;

	for( k = levels; k > 0; --k )
	{
		// decompose rows
		for( i = 0; i < height>>(k-1); ++i )
		{
			iwt97(data+i*width, width>>(k-1));
		}

		// decompose columns
		for( j = 0; j < width>>(k-1); ++j )
		{
			for( i = 0, p1 = data+j, p2 = columnBuffer; i < height>>(k-1); ++i, p1+=width, ++p2 )
				*p2 = *p1;

			iwt97(columnBuffer, height>>(k-1));

			for( i = 0, p1 = data+j, p2 = columnBuffer; i < height>>(k-1); ++i, p1+=width, ++p2 )
				*p1 = *p2;
		}
	}
}

// if mark is NULL, attempts to remove watermark from LH3 and HL3 and store the recontruction in dst
// otherwise it inserts the mark into the image stores the new image in dst
void insertWatermark( unsigned int* src, unsigned int** dst, unsigned char* mark, int *width, int *height, bool isForward = true, double markStrength = 0.5 )
{
	unsigned int size = *width**height;
	unsigned int newSize;
	unsigned int n;
    int i,j;
	unsigned int *p1;
	double       *p2;

	double tempColor1[3];
	double tempColor2[3];

	int newWidth = nextPow2(*width);
	int newHeight = nextPow2(*height);

	newSize = newWidth*newHeight;

	n = newSize;

	rgbacol temp;

	unsigned int markSize = 32;

	// Only handles 512x512 image currently
	if( newHeight != 512 && newWidth != 512 )
	{
		*dst = NULL;
		fprintf(stderr,"Error: Expecting 512x512 source image");
		return;
	}

	double * markBuffer1 = NULL;
	double * markBuffer2 = NULL;

	double *freqs = (double*)malloc(sizeof(double)*n);
	double *freqTempColumn = (double*)malloc(sizeof(double)*newHeight);

	if( !isForward )
	{
		markBuffer1 = (double*)malloc(sizeof(double)*markSize*markSize);
		markBuffer2 = (double*)malloc(sizeof(double)*markSize*markSize);
	}

	// convert RGB to luminance
    for( i = 0, p1 = src, p2 = freqs; i < *height; ++i )
    {
		for( j = 0; j < *width; ++j, ++p1, ++p2 )
		{
    		temp.c = *p1;

#ifdef USE_LAB
			tempColor1[0] = (double)temp.r / 255.0;
			tempColor1[1] = (double)temp.g / 255.0;
			tempColor1[2] = (double)temp.b / 255.0;

			RGBtoXYZ(tempColor1,tempColor2);
			XYZtoLab(tempColor2,tempColor1);

			*p2 = tempColor1[0];
#else
			tempColor1[0] = (double)temp.r;
			tempColor1[1] = (double)temp.g;
			tempColor1[2] = (double)temp.b;

			RGBtoYCbCr(tempColor1,tempColor2);

			*p2 = tempColor2[0];
#endif

		}
		for( ; j < newWidth; ++j, ++p2 )
			*p2 = 0;
    }
    for( ; i < newHeight; ++i )
    {
		for( j = 0; j < newWidth; ++j, ++p2 )
		{
			*p2 = 0;
		}
	}

	decomposeImage(freqs,freqTempColumn,3,newWidth,newHeight);

	if( isForward )
	{
		// encode watermark boolean bits into coefficients
		encodeMark(freqs, mark, newWidth, newHeight, markSize, markStrength);

		// prepare to return the source image
		*dst = src;

		reconstructImage(freqs,freqTempColumn,3,newWidth,newHeight);

		// replace luminance in image
		for( i = 0, p1 = *dst, p2 = freqs; i < *height; ++i )
		{
			for( j = 0; j < *width; ++j, ++p1, ++p2 )
			{
    			temp.c = *p1;

#ifdef USE_LAB // Use Lab Color Space
				tempColor1[0] = (double)temp.r / 255.0;
				tempColor1[1] = (double)temp.g / 255.0;
				tempColor1[2] = (double)temp.b / 255.0;

				RGBtoXYZ(tempColor1,tempColor2);
				XYZtoLab(tempColor2,tempColor1);

    			tempColor1[0] = *p2;

				LabtoXYZ(tempColor1,tempColor2);
				XYZtoRGB(tempColor2,tempColor1);

				temp.r = (unsigned char)(tempColor1[0] * 255.0);
				temp.g = (unsigned char)(tempColor1[1] * 255.0);
				temp.b = (unsigned char)(tempColor1[2] * 255.0);
#else // Use YCbCr Color Space
				tempColor1[0] = (double)temp.r;
				tempColor1[1] = (double)temp.g;
				tempColor1[2] = (double)temp.b;

				RGBtoYCbCr(tempColor1,tempColor2);

				tempColor2[0] = *p2;

				YCbCrtoRGB(tempColor2,tempColor1);

				temp.r = (unsigned char)tempColor1[0];
				temp.g = (unsigned char)tempColor1[1];
				temp.b = (unsigned char)tempColor1[2];
#endif

				*p1 = temp.c;
			}
		}
	}
	else
	{
		decodeMark(freqs, mark, markBuffer1, markBuffer2, *width, *height, markSize, markStrength);

		*dst = NULL;
	}

	if( !isForward )
	{
		free(markBuffer1);
		free(markBuffer2);
	}

	free(freqs);
	free(freqTempColumn);
}

int main(int argc, char** argv)
{
	if( argc != 3 && argc != 5 )
	{
		printf("    usage: WaveMark strength input.png [output.png \"string\"]\n");
		exit(-1);
	}

	int width, height, channels;
	unsigned int markWidth, markHeight;

	markWidth = markHeight = 32;

	//CCPNGInit();

	//unsigned int* imageData = CCPNGReadFile(argv[2], &width, &height);
	unsigned int* imageData = (unsigned int*)stbi_load( argv[2], &width, &height, &channels, 4 );
	unsigned int* outputData = NULL;
	double strength = atof(argv[1]);
	unsigned char* boolMark = (unsigned char*)malloc(sizeof(unsigned char)*markWidth*markHeight);	

	// encode string from command line
	if( argc == 5 ) 
	{
		markWidth = 32;
		markHeight = 32;

		std::string message = argv[4];

		// remove quotes
		message.substr(1,message.length()-2);

		if( message.length() > 32 )
		{
			printf("Error: string too long\n");
		}

		else
		{
			// convert string to boolean matrix
			encodeStringIntoBinaryMatrix(message.c_str(),boolMark,markWidth,markHeight);
		}
	}

	if( imageData != NULL )
	{	
		if( boolMark != NULL || argc == 3 )
		{
			insertWatermark( imageData, &outputData, boolMark, &width, &height, argc == 5, strength );

			if( argc == 3 )
			{
				// convert boolean matrix into string result
				char str[33];
				str[32] = 0;
					
				decodeBinaryMatrixAsString(boolMark, str, markWidth, markHeight );
				width = markWidth;
				height = markHeight;

				printf("Message obtained from image %s : %s\n", argv[2], str);
			}

			if( outputData != NULL )
			{
				stbi_write_png( argv[3], width, height, 4, outputData, 4*width);
				//CCPNGWriteFile(argv[3], outputData, width, height, 0, 1);

				if( argc != 5 ) 
					free(outputData);
			}
		}

		free(imageData);
	}
	else
	{
		fprintf(stderr,"Error: could not open file %s\n", argv[2]);
	}

	free(boolMark);

	//CCPNGDestroy();
	dwtcleanup();

	return 0;
}
