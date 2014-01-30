#include "idct.h"

void Rows_IDCT(int* block) {
	int x[9];
	if (!((x[1] = block[4] << 11) | (x[2] = block[6]) | (x[3] = block[2]) | (x[4] = block[1])
		| (x[5] = block[7]) | (x[6] = block[5])	| (x[7] = block[3])))
	{
		block[0] = block[1] = block[2] = block[3] = block[4] = block[5] = block[6] = block[7] = block[0] << 3;
		return;
	}
	x[0] = (block[0] << 11) + 128;
	x[8] = W7 * (x[4] + x[5]);
	x[4] = x[8] + (W1 - W7) * x[4];
	x[5] = x[8] - (W1 + W7) * x[5];
	x[8] = W3 * (x[6] + x[7]);
	x[6] = x[8] - (W3 - W5) * x[6];
	x[7] = x[8] - (W3 + W5) * x[7];
	x[8] = x[0] + x[1];
	x[0] -= x[1];
	x[1] = W6 * (x[3] + x[2]);
	x[2] = x[1] - (W2 + W6) * x[2];
	x[3] = x[1] + (W2 - W6) * x[3];
	x[1] = x[4] + x[6];
	x[4] -= x[6];
	x[6] = x[5] + x[7];
	x[5] -= x[7];
	x[7] = x[8] + x[3];
	x[8] -= x[3];
	x[3] = x[0] + x[2];
	x[0] -= x[2];
	x[2] = (181 * (x[4] + x[5]) + 128) >> 8;
	x[4] = (181 * (x[4] - x[5]) + 128) >> 8;
	block[0] = (x[7] + x[1]) >> 8;
	block[1] = (x[3] + x[2]) >> 8;
	block[2] = (x[0] + x[4]) >> 8;
	block[3] = (x[8] + x[6]) >> 8;
	block[4] = (x[8] - x[6]) >> 8;
	block[5] = (x[0] - x[4]) >> 8;
	block[6] = (x[3] - x[2]) >> 8;
	block[7] = (x[7] - x[1]) >> 8;
}

void Columns_IDCT(const int* block, unsigned char *out, int step) {
	int x[9];
	if (!((x[1] = block[8*4] << 8) | (x[2] = block[8*6]) | (x[3] = block[8*2]) | (x[4] = block[8*1])
		| (x[5] = block[8*7]) | (x[6] = block[8*5]) | (x[7] = block[8*3])))
	{
		x[1] = Clip(((block[0] + 32) >> 6) + 128);
		for (x[0] = 8;  x[0];  --x[0]) {
			*out = (unsigned char) x[1];
			out += step;
		}
		return;
	}
	x[0] = (block[0] << 8) + 8192;
	x[8] = W7 * (x[4] + x[5]) + 4;
	x[4] = (x[8] + (W1 - W7) * x[4]) >> 3;
	x[5] = (x[8] - (W1 + W7) * x[5]) >> 3;
	x[8] = W3 * (x[6] + x[7]) + 4;
	x[6] = (x[8] - (W3 - W5) * x[6]) >> 3;
	x[7] = (x[8] - (W3 + W5) * x[7]) >> 3;
	x[8] = x[0] + x[1];
	x[0] -= x[1];
	x[1] = W6 * (x[3] + x[2]) + 4;
	x[2] = (x[1] - (W2 + W6) * x[2]) >> 3;
	x[3] = (x[1] + (W2 - W6) * x[3]) >> 3;
	x[1] = x[4] + x[6];
	x[4] -= x[6];
	x[6] = x[5] + x[7];
	x[5] -= x[7];
	x[7] = x[8] + x[3];
	x[8] -= x[3];
	x[3] = x[0] + x[2];
	x[0] -= x[2];
	x[2] = (181 * (x[4] + x[5]) + 128) >> 8;
	x[4] = (181 * (x[4] - x[5]) + 128) >> 8;
	*out = Clip(((x[7] + x[1]) >> 14) + 128);  out += step;
	*out = Clip(((x[3] + x[2]) >> 14) + 128);  out += step;
	*out = Clip(((x[0] + x[4]) >> 14) + 128);  out += step;
	*out = Clip(((x[8] + x[6]) >> 14) + 128);  out += step;
	*out = Clip(((x[8] - x[6]) >> 14) + 128);  out += step;
	*out = Clip(((x[0] - x[4]) >> 14) + 128);  out += step;
	*out = Clip(((x[3] - x[2]) >> 14) + 128);  out += step;
	*out = Clip(((x[7] - x[1]) >> 14) + 128);
}


unsigned char Clip(const int x) {
	if (x < 0)
		return 0;
	else if (x > 0xFF) 
		return 0xFF;
	else
		return (unsigned char)x;
}