#include <stdint.h>
#include <stdlib.h>
#include <boolean.h>
#include <string.h>
#include <formats/mpng.h>

#ifndef HAVE_ZLIB
# define MINIZ_HEADER_FILE_ONLY
# include "miniz.c"
static void tinfl_deinit(tinfl_decompressor* r) {}

#else

# include <zlib.h>
typedef z_stream tinfl_decompressor;
typedef int tinfl_status;

static uint32_t mz_crc32(uint32_t crc, const uint8_t* buf, size_t len) { return crc32(crc, buf, len); }

enum {
	TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
	TINFL_FLAG_HAS_MORE_INPUT = 2,
	TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
	TINFL_FLAG_COMPUTE_ADLER32 = 8,
	
	TINFL_STATUS_DONE  = Z_STREAM_END
};

static void tinfl_init(tinfl_decompressor* r)
{
	memset(r, 0, sizeof(*r));
	inflateInit(r);
}

static tinfl_status tinfl_decompress(tinfl_decompressor* r, const uint8_t * pIn_buf_next, size_t* pIn_buf_size,
                                     uint8_t * pOut_buf_start, uint8_t * pOut_buf_next, size_t* pOut_buf_size,
                                     uint32_t decomp_flags)
{
	r->next_in = (Bytef*)pIn_buf_next;
	r->avail_in = *pIn_buf_size;
	r->next_out = pOut_buf_next;
	r->avail_out = *pOut_buf_size;
	return inflate(r, (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? Z_NO_FLUSH : Z_SYNC_FLUSH);
}

static void tinfl_deinit(tinfl_decompressor* r)
{
	inflateEnd(r);
}
#endif

uint32_t read8r(const uint8_t* source) { return *source; }
uint32_t read24r(const uint8_t* source)
{
	return ((source[0]<<16) | (source[1]<<8) | (source[2]<<0));
}
uint32_t read32r(const uint8_t* source)
{
	return ((source[0]<<24) | (source[1]<<16) | (source[2]<<8) | (source[3]<<0));
}

#define read8(target) do { target=read8r(chunkdata); chunkdata++; } while(0)
#define read24(target) do { target=read24r(chunkdata); chunkdata+=3; } while(0)
#define read32(target) do { target=read32r(chunkdata); chunkdata+=4; } while(0)

bool png_decode(const void * pngdata, size_t pnglen, struct mpng_image * img, enum video_format format)
{
   unsigned i;
   unsigned b, x, y;

	memset(img, 0, sizeof(struct mpng_image));
	if (format!=FMT_RGB888 && format!=FMT_XRGB8888 && format!=FMT_ARGB8888) return false;
	
	if (pnglen<8) return false;
	const uint8_t * data=(const uint8_t*)pngdata;
	if (memcmp(data, "\x89PNG\r\n\x1A\n", 8)) return false;
	const uint8_t * dataend=data+pnglen;
	data+=8;
	
	unsigned int width;
	unsigned int height;
	uint8_t * pixels=NULL;
	uint8_t * pixelsat;
	uint8_t * pixelsend;
	
	//chop off some warnings... these are all initialized in IHDR
	unsigned int bitsperchannel;
	unsigned int colortype;
	unsigned int compressiontype;
	unsigned int filtertype;
	unsigned int interlacetype;
	unsigned int bpl;
	
	unsigned int palette[256];
	memset(palette, 0, sizeof(palette));//not gonna catch palette overflows
	int palettelen=0;
	
	tinfl_decompressor inflator;
	tinfl_init(&inflator);
	
	while (true)
	{
		if (data+4+4>dataend) goto bad;
		unsigned int chunklen=read32r(data);
		unsigned int chunktype=read32r(data+4);
		if (chunklen>=0x80000000) goto bad;
		if (data+4+chunklen+4>dataend) goto bad;
		unsigned int chunkchecksum=mz_crc32(mz_crc32(0, NULL, 0), (uint8_t*)data+4, 4+chunklen);
		const uint8_t * chunkdata=data+4+4;
		unsigned int actualchunkchecksum=read32r(data+4+4+chunklen);
		if (actualchunkchecksum!=chunkchecksum) goto bad;
		
		data+=4+4+chunklen+4;
		switch (chunktype)
		{
			case 0x49484452: //IHDR
			{
				read32(width);
				read32(height);
				read8(bitsperchannel);
				read8(colortype);
				read8(compressiontype);
				read8(filtertype);
				read8(interlacetype);
				
				if (width>=0x80000000) goto bad;
				if (width==0) goto bad;
				if (height>=0x80000000) goto bad;
				if (height==0) goto bad;
				if (colortype!=2 && colortype!=3 && colortype!=6) goto bad;
//Greyscale 	0
//Truecolour 	2
//Indexed-colour 	3
//Greyscale with alpha 	4
//Truecolour with alpha 	6
				if (colortype==2 && bitsperchannel!=8) goto bad;//truecolor; can be 16bpp but I don't want that.
				if (colortype==3 && bitsperchannel!=1 && bitsperchannel!=2 && bitsperchannel!=4 && bitsperchannel!=8) goto bad;//paletted
				if (colortype==6 && bitsperchannel!=8) goto bad;//truecolor with alpha
				if (colortype==6 && format!=FMT_ARGB8888) goto bad;//can only decode alpha on ARGB formats
				if (compressiontype!=0) goto bad;
				if (filtertype!=0) goto bad;
				if (interlacetype!=0 && interlacetype!=1) goto bad;
				
				if (colortype==2) bpl=3*width;
				if (colortype==3) bpl=(width*bitsperchannel + bitsperchannel-1)/8;
				if (colortype==6) bpl=4*width;
				pixels=(uint8_t*)malloc((bpl+1)*height); if (!pixels) goto bad;
				pixelsat=pixels;
				pixelsend=pixels+(bpl+1)*height;
			}
			break;
			case 0x504c5445: //PLTE
			{
				if (pixels==NULL || palettelen!=0) goto bad;
				if (chunklen==0 || chunklen%3 || chunklen>3*256) goto bad;
				if (colortype!=3) break;//palette on rgb is allowed but rare, and it's just a recommendation anyways.
				palettelen=chunklen/3;
				for (i=0;i<palettelen;i++)
				{
					read24(palette[i]);
					palette[i]|=0xFF000000;
				}
			}
			break;
			case 0x74524E53: //tRNS
			{
				if (format!=FMT_ARGB8888 || pixels==NULL || pixels!=pixelsat) goto bad;
				if (colortype==2)
				{
					if (palettelen==0) goto bad;
goto bad;
				}
				else if (colortype==3)
				{
goto bad;
				}
				else goto bad;
			}
			break;
			case 0x49444154: //IDAT
			{
				if (pixels==NULL || (colortype==3 && palettelen==0)) goto bad;
				size_t chunklencopy=chunklen;
				size_t byteshere=(pixelsend-pixelsat)+1;
				tinfl_status status=tinfl_decompress(&inflator, (const uint8_t*)chunkdata, &chunklencopy, pixels, pixelsat, &byteshere,
															TINFL_FLAG_HAS_MORE_INPUT | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | TINFL_FLAG_PARSE_ZLIB_HEADER);
				pixelsat+=byteshere;
				if (status<0) goto bad;
			}
			break;
			case 0x49454e44: //IEND
			{
				if (data!=dataend) goto bad;
				if (chunklen) goto bad;
				size_t zero=0;
				size_t finalbytes=(pixelsend-pixelsat);
				tinfl_status status=tinfl_decompress(&inflator, (const uint8_t*)NULL, &zero, pixels, pixelsat, &finalbytes,
				                                     TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | TINFL_FLAG_PARSE_ZLIB_HEADER);
				pixelsat+=finalbytes;
				if (status!=TINFL_STATUS_DONE) goto bad;
				if (pixelsat!=pixelsend) goto bad;//too little data (can't be too much because we didn't give it that buffer size)
				uint8_t * out=(uint8_t*)malloc(videofmt_byte_per_pixel(format)*width*height);
				
				//TODO: deinterlace at random point
				
				//run filters
				unsigned int bpppacked=((colortype==2)?3:(colortype==6)?4:1);
				uint8_t * prevout=out+(4*width*1);
				if (height==1)
				{
					prevout=out;
					//this will blow up if a 1px high image is filtered with Paeth, but who the hell would do that?
				}
				memset(prevout, 0, 4*width*1);//not using bpp here because we only need a chunk of black anyways
				uint8_t * filteredline=pixels;
				
				for (y=0;y<height;y++)
				{
					uint8_t * thisout=out+(bpl*y);
					switch (*(filteredline++))
					{
					case 0:
						memcpy(thisout, filteredline, bpl);
						break;
					case 1:
						memcpy(thisout, filteredline, bpppacked);
						for (x=bpppacked;x<bpl;x++)
							thisout[x]=thisout[x-bpppacked]+filteredline[x];
						break;
					case 2:
						for (x=0;x<bpl;x++)
						{
							thisout[x]=prevout[x]+filteredline[x];
						}
						break;
					case 3:
						for (x=0;x<bpppacked;x++)
						{
							int a=0;
							int b=prevout[x];
							thisout[x]=(a+b)/2+filteredline[x];
						}
						for (x=bpppacked;x<bpl;x++)
						{
							int a=thisout[x-bpppacked];
							int b=prevout[x];
							thisout[x]=(a+b)/2+filteredline[x];
						}
						break;
					case 4:
						for (x=0;x<bpppacked;x++)
						{
							int prediction;
							
							int a=0;
							int b=prevout[x];
							int c=0;
							
							int p=a+b-c;
							int pa=abs(p-a);
							int pb=abs(p-b);
							int pc=abs(p-c);
							
							if (pa<=pb && pa<=pc) prediction=a;
							else if (pb<=pc) prediction=b;
							else prediction=c;
							
							thisout[x]=filteredline[x]+prediction;
						}
						for (x=bpppacked;x<bpl;x++)
						{
							int prediction;
							
							int a=thisout[x-bpppacked];
							int b=prevout[x];
							int c=prevout[x-bpppacked];
							
							int p=a+b-c;
							int pa=abs(p-a);
							int pb=abs(p-b);
							int pc=abs(p-c);
							
							if (pa<=pb && pa<=pc) prediction=a;
							else if (pb<=pc) prediction=b;
							else prediction=c;
							
							thisout[x]=filteredline[x]+prediction;
						}
						break;
					default: goto bad;
					}
					prevout=thisout;
					filteredline+=bpl;
				}
				
				//unpack paletted data
				//not sure if these aliasing tricks are valid, but the prerequisites for that bugging up are pretty much impossible to hit.
				if (colortype==3)
				{
					switch (bitsperchannel)
					{
					case 1:
					{
						int y=height;
						uint8_t * outp=out+3*width*height;
						do {
							uint8_t * inp=out+y*bpl;
							
							int x=(width+7)/8;
							do {
								x--;
								inp--;
								for (b=0;b<8;b++)
								{
									int rgb32=palette[((*inp)>>b)&1];
									*(--outp)=rgb32>>0;
									*(--outp)=rgb32>>8;
									*(--outp)=rgb32>>16;
								}
							} while(x);
							y--;
						} while(y);
					}
					break;
					case 2:
					{
						int y=height;
						uint8_t * outp=out+3*width*height;
						do {
							unsigned char * inp=out+y*bpl;
							
							int x=(width+3)/4;
							do {
								x--;
								inp--;
								for (b=0;b<8;b+=2)
								{
									int rgb32=palette[((*inp)>>b)&3];
									*(--outp)=rgb32>>0;
									*(--outp)=rgb32>>8;
									*(--outp)=rgb32>>16;
								}
							} while(x);
							y--;
						} while(y);
					}
					break;
					case 4:
					{
						int y=height;
						uint8_t * outp=out+3*width*height;
						do {
							unsigned char * inp=out+y*bpl;
							
							int x=(width+1)/2;
							do {
								x--;
								inp--;
								int rgb32=palette[*inp&15];
								*(--outp)=rgb32>>0;
								*(--outp)=rgb32>>8;
								*(--outp)=rgb32>>16;
								rgb32=palette[*inp>>4];
								*(--outp)=rgb32>>0;
								*(--outp)=rgb32>>8;
								*(--outp)=rgb32>>16;
							} while(x);
							y--;
						} while(y);
					}
					break;
					case 8:
					{
						uint8_t * inp=out+width*height;
						uint8_t * outp=out+3*width*height;
						int i=width*height;
						do {
							i--;
							inp-=1;
							int rgb32=palette[*inp];
							*(--outp)=rgb32>>0;
							*(--outp)=rgb32>>8;
							*(--outp)=rgb32>>16;
						} while(i);
					}
					break;
					}
				}
				
				//unpack to 32bpp if requested
				if (format!=FMT_RGB888 && colortype==2)
				{
					uint8_t * inp=out+width*height*3;
					uint32_t * outp=((uint32_t*)out)+width*height;
					int i=width*height;
					do {
						i--;
						inp-=3;
						outp--;
						*outp = read24r(inp) | 0xFF000000;
					} while(i);
				}
				
				img->width=width;
				img->height=height;
				img->pixels=out;
				img->pitch=videofmt_byte_per_pixel(format)*width;
				img->format=format;
				tinfl_deinit(&inflator);
				free(pixels);
				return true;
			}
			break;
			default:
				if (!(chunktype&0x20000000)) goto bad;//unknown critical
				//otherwise ignore
		}
	}
	
bad:
	tinfl_deinit(&inflator);
	free(pixels);
	memset(img, 0, sizeof(struct mpng_image));
	return false;
}
