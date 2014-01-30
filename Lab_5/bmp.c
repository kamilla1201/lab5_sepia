#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmp.h"

void BMP_Save(const char* output_name, int width, int height, unsigned char* RGB)
{
	
	
	FILE* f;
	BMP_Header bmp_head;
	int x, y, padded_bytes;
	padded_bytes = 4 - (width * 3) % 4;
	padded_bytes = padded_bytes % 4;

	
	memset(&bmp_head, 0, sizeof(bmp_head));
	bmp_head.type[0]='B';
	bmp_head.type[1]='M';
	bmp_head.file_size = (width*height*3) + (height*padded_bytes) + sizeof(bmp_head);
	bmp_head.offset = sizeof(BMP_Header);
	bmp_head.header_size = 40;
	bmp_head.planes = 1;
	bmp_head.width = width;
	bmp_head.height = height;
	bmp_head.bit_count = 24;

	
	
	fopen_s(&f, output_name, "wb");
	if (!f) {
		printf("Error opening the output file.");
		getchar();
		return;
	}
	fwrite(&bmp_head, sizeof(bmp_head), 1, f);
	for (y=height-1; y>=0; y--)
	{
		for (x=0; x<width; x++)
		{
			int i = (x + (width)*y) * 3;
			unsigned int rgbpix = (RGB[i]<<16)|(RGB[i+1]<<8)|(RGB[i+2]<<0);
			fwrite(&rgbpix, 3, 1, f);
		}
		if (padded_bytes>0)
		{
			unsigned char pad = 0;
			fwrite(&pad, padded_bytes, 1, f);
		}
	}
	fclose(f);
}