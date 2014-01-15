#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode.h"

#define W1 2841 // 2048*sqrt(2)*cos(1*pi/16)
#define W2 2676 // 2048*sqrt(2)*cos(2*pi/16)
#define W3 2408 // 2048*sqrt(2)*cos(3*pi/16)
#define W5 1609 // 2048*sqrt(2)*cos(5*pi/16)
#define W6 1108 // 2048*sqrt(2)*cos(6*pi/16)
#define W7 565  // 2048*sqrt(2)*cos(7*pi/16)

#define ERROR { jpg_data.error = error_stat; return; } 

//Ordering within a 8x8 block, in zig-zag
const char ZigZag[64] = 
{ 0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18,
11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34, 27, 20,
13, 6, 7, 14, 21, 28, 35,42, 49, 56, 57, 50, 43, 
36, 29, 22, 15, 23, 30, 37, 44, 51, 58, 59, 52, 45,
38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63 };

result_status Decode(const void* jpeg, const int size) {
	Destroy();
	jpg_data.file_point = (const unsigned char*) jpeg;
	jpg_data.size = size & 0x7FFFFFFF;
	if (jpg_data.size < 2) return error_stat;
	if ((jpg_data.file_point[0] != 0xFF) | (jpg_data.file_point[1] != 0xD8)) return error_stat;
	Skip(2);
	while (!jpg_data.error) {
		Skip(2);
		switch (jpg_data.file_point[-1]) {
			
		case 0xFE: Skip_marker(); break; 
		case 0xDB: DQT_decode();  break;
		case 0xC0: SOF_decode();  break;
		case 0xC4: DHT_decode();  break;
		case 0xDA: SOS_decode(); break;
		default:
			if ((jpg_data.file_point[-1] & 0xF0) == 0xE0) // APP0 - JFIF specification
				Skip_marker();
			else
				return error_stat;
		}
	}
	if (jpg_data.error != end_of_file) return jpg_data.error;
	jpg_data.error = success_stat;
	Convert();
	return jpg_data.error;
}

void Skip_marker() {
	length_Get();
	Skip(jpg_data.length);
}

void DQT_decode() {
	int i;
	unsigned char *t;
	length_Get();
	while (jpg_data.length >= 65) {
		i = jpg_data.file_point[0];
		if (i & 0xFC) ERROR;
		jpg_data.qt_available |= 1 << i;
		t = &jpg_data.q_table[i][0];
		for (i = 0;  i < 64;  ++i)
			t[i] = jpg_data.file_point[i + 1];
		Skip(65);
	}
	if (jpg_data.length) ERROR;
}

void SOF_decode() {
	int i, ssxmax = 0, ssymax = 0;
	jpg_comp* c;
	length_Get();
	if (jpg_data.length < 9) ERROR;
	if (jpg_data.file_point[0] != 8) ERROR;
	jpg_data.height = (jpg_data.file_point[1] * 256) | jpg_data.file_point[2];
	jpg_data.width = (jpg_data.file_point[3] * 256) | jpg_data.file_point[4];
	jpg_data.ncomp = jpg_data.file_point[5];
	Skip(6);

	if (jpg_data.length < (jpg_data.ncomp * 3)) ERROR;
	for (i = 0, c = jpg_data.comp;  i < jpg_data.ncomp;  ++i, ++c) {
		c->cid = jpg_data.file_point[0];
		if (!(c->ssx = jpg_data.file_point[1] >> 4)) ERROR;
		if (c->ssx & (c->ssx - 1)) ERROR;  // non-power of two
		if (!(c->ssy = jpg_data.file_point[1] & 15)) ERROR;
		if (c->ssy & (c->ssy - 1)) ERROR;  // non-power of two
		if ((c->qt_sel = jpg_data.file_point[2]) & 0xFC) ERROR;
		Skip(3);
		jpg_data.qt_used |= 1 << c->qt_sel;
		if (c->ssx > ssxmax) ssxmax = c->ssx;
		if (c->ssy > ssymax) ssymax = c->ssy;
	}
	jpg_data.m_block_sizex = ssxmax << 3;
	jpg_data.m_block_sizey = ssymax << 3;
	jpg_data.m_block_width = (jpg_data.width + jpg_data.m_block_sizex - 1) / jpg_data.m_block_sizex;
	jpg_data.m_block_height = (jpg_data.height + jpg_data.m_block_sizey - 1) / jpg_data.m_block_sizey;
	for (i = 0, c = jpg_data.comp;  i < jpg_data.ncomp;  ++i, ++c) {
		c->width = (jpg_data.width * c->ssx + ssxmax - 1) / ssxmax;
		c->step = (c->width + 7) & 0x7FFFFFF8;
		c->height = (jpg_data.height * c->ssy + ssymax - 1) / ssymax;
		c->step = jpg_data.m_block_width * jpg_data.m_block_sizex * c->ssx / ssxmax;
		if (((c->width < 3) && (c->ssx != ssxmax)) || ((c->height < 3) && (c->ssy != ssymax))) ERROR;
		if (!(c->pixels = (unsigned char*)malloc(c->step * (jpg_data.m_block_height * jpg_data.m_block_sizey * c->ssy / ssymax)))) ERROR;
	}
	if (jpg_data.ncomp == 3) {
		jpg_data.RGB_point = (unsigned char*)malloc(jpg_data.width * jpg_data.height * jpg_data.ncomp);
		if (!jpg_data.RGB_point) ERROR;
	}
	Skip(jpg_data.length);
}

void DHT_decode() {
	int codelen, currcnt, remain, spread, i, j;
	vlc_code *vlc;
	static unsigned char counts[16];
	length_Get();
	while (jpg_data.length >= 17) {
		i = jpg_data.file_point[0];
		if (i & 0xEC) ERROR;
		if (i & 0x02) ERROR;
		i = (i | (i >> 3)) & 3;  // combined AC/DC and tableid value
		for (codelen = 1;  codelen <= 16;  ++codelen)
			counts[codelen - 1] = jpg_data.file_point[codelen];
		Skip(17);
		vlc = &jpg_data.vlc_table[i][0];
		remain = spread = 65536;
		for (codelen = 1;  codelen <= 16;  ++codelen) {
			spread >>= 1;
			currcnt = counts[codelen - 1];
			if (!currcnt) continue;
			if (jpg_data.length < currcnt) ERROR;
			remain -= currcnt << (16 - codelen);
			if (remain < 0) ERROR;
			for (i = 0;  i < currcnt;  ++i) {
				register unsigned char code = jpg_data.file_point[i];
				for (j = spread;  j;  --j) {
					vlc->bits = (unsigned char) codelen;
					vlc->code = code;
					++vlc;
				}
			}
			Skip(currcnt);
		}
		while (remain--) {
			vlc->bits = 0;
			++vlc;
		}
	}
	if (jpg_data.length) ERROR;
}

void SOS_decode() {
	int i, m_block_x, m_block_y, sbx, sby;
	int restart_count = jpg_data.restart_interval, next_restart = 0;
	jpg_comp* c;
	length_Get();
	if (jpg_data.length < (4 + 2 * jpg_data.ncomp)) ERROR;
	if (jpg_data.file_point[0] != jpg_data.ncomp) ERROR;
	Skip(1);
	for (i = 0, c = jpg_data.comp;  i < jpg_data.ncomp;  ++i, ++c) {
		if (jpg_data.file_point[0] != c->cid) ERROR;
		if (jpg_data.file_point[1] & 0xEE) ERROR;
		c->dc_table = jpg_data.file_point[1] >> 4;
		c->ac_table = (jpg_data.file_point[1] & 1) | 2;
		Skip(2);
	}
	if (jpg_data.file_point[0] || (jpg_data.file_point[1] != 63) || jpg_data.file_point[2]) ERROR;
	Skip(jpg_data.length);
	for (m_block_x = m_block_y = 0;;) {
		for (i = 0, c = jpg_data.comp;  i < jpg_data.ncomp;  ++i, ++c)
			for (sby = 0;  sby < c->ssy;  ++sby)
				for (sbx = 0;  sbx < c->ssx;  ++sbx) {
					Block_Decode(c, &c->pixels[((m_block_y * c->ssy + sby) * c->step + m_block_x * c->ssx + sbx) << 3]);
					if (jpg_data.error)
						return;
				}
				if (++m_block_x >= jpg_data.m_block_width) {
					m_block_x = 0;
					if (++m_block_y >= jpg_data.m_block_height) break;
				}
				if (jpg_data.restart_interval && !(--restart_count)) {
					jpg_data.bits_buf &= 0xF8;
					i = bits_Get(16);
					if (((i & 0xFFF8) != 0xFFD0) || ((i & 7) != next_restart)) ERROR;
					next_restart = (next_restart + 1) & 7;
					restart_count = jpg_data.restart_interval;
					for (i = 0;  i < 3;  ++i)
						jpg_data.comp[i].dc_pred = 0;
				}
	}
	jpg_data.error = end_of_file;
}

void Row_IDCT(int* block) {
	int x0, x1, x2, x3, x4, x5, x6, x7, x8;
	if (!((x1 = block[4] << 11) | (x2 = block[6]) | (x3 = block[2]) | (x4 = block[1])
		| (x5 = block[7]) | (x6 = block[5])	| (x7 = block[3])))
	{
		block[0] = block[1] = block[2] = block[3] = block[4] = block[5] = block[6] = block[7] = block[0] << 3;
		return;
	}
	x0 = (block[0] << 11) + 128;
	x8 = W7 * (x4 + x5);
	x4 = x8 + (W1 - W7) * x4;
	x5 = x8 - (W1 + W7) * x5;
	x8 = W3 * (x6 + x7);
	x6 = x8 - (W3 - W5) * x6;
	x7 = x8 - (W3 + W5) * x7;
	x8 = x0 + x1;
	x0 -= x1;
	x1 = W6 * (x3 + x2);
	x2 = x1 - (W2 + W6) * x2;
	x3 = x1 + (W2 - W6) * x3;
	x1 = x4 + x6;
	x4 -= x6;
	x6 = x5 + x7;
	x5 -= x7;
	x7 = x8 + x3;
	x8 -= x3;
	x3 = x0 + x2;
	x0 -= x2;
	x2 = (181 * (x4 + x5) + 128) >> 8;
	x4 = (181 * (x4 - x5) + 128) >> 8;
	block[0] = (x7 + x1) >> 8;
	block[1] = (x3 + x2) >> 8;
	block[2] = (x0 + x4) >> 8;
	block[3] = (x8 + x6) >> 8;
	block[4] = (x8 - x6) >> 8;
	block[5] = (x0 - x4) >> 8;
	block[6] = (x3 - x2) >> 8;
	block[7] = (x7 - x1) >> 8;
}

void Col_IDCT(const int* block, unsigned char *out, int step) {
	int x0, x1, x2, x3, x4, x5, x6, x7, x8;
	if (!((x1 = block[8*4] << 8) | (x2 = block[8*6]) | (x3 = block[8*2]) | (x4 = block[8*1])
		| (x5 = block[8*7]) | (x6 = block[8*5]) | (x7 = block[8*3])))
	{
		x1 = Clip(((block[0] + 32) >> 6) + 128);
		for (x0 = 8;  x0;  --x0) {
			*out = (unsigned char) x1;
			out += step;
		}
		return;
	}
	x0 = (block[0] << 8) + 8192;
	x8 = W7 * (x4 + x5) + 4;
	x4 = (x8 + (W1 - W7) * x4) >> 3;
	x5 = (x8 - (W1 + W7) * x5) >> 3;
	x8 = W3 * (x6 + x7) + 4;
	x6 = (x8 - (W3 - W5) * x6) >> 3;
	x7 = (x8 - (W3 + W5) * x7) >> 3;
	x8 = x0 + x1;
	x0 -= x1;
	x1 = W6 * (x3 + x2) + 4;
	x2 = (x1 - (W2 + W6) * x2) >> 3;
	x3 = (x1 + (W2 - W6) * x3) >> 3;
	x1 = x4 + x6;
	x4 -= x6;
	x6 = x5 + x7;
	x5 -= x7;
	x7 = x8 + x3;
	x8 -= x3;
	x3 = x0 + x2;
	x0 -= x2;
	x2 = (181 * (x4 + x5) + 128) >> 8;
	x4 = (181 * (x4 - x5) + 128) >> 8;
	*out = Clip(((x7 + x1) >> 14) + 128);  out += step;
	*out = Clip(((x3 + x2) >> 14) + 128);  out += step;
	*out = Clip(((x0 + x4) >> 14) + 128);  out += step;
	*out = Clip(((x8 + x6) >> 14) + 128);  out += step;
	*out = Clip(((x8 - x6) >> 14) + 128);  out += step;
	*out = Clip(((x0 - x4) >> 14) + 128);  out += step;
	*out = Clip(((x3 - x2) >> 14) + 128);  out += step;
	*out = Clip(((x7 - x1) >> 14) + 128);
}

int bits_Show(int bits) {
	unsigned char newbyte;
	if (!bits) return 0;
	while (jpg_data.bits_buf < bits) {
		if (jpg_data.size <= 0) {
			jpg_data.buf = (jpg_data.buf << 8) | 0xFF;
			jpg_data.bits_buf += 8;
			continue;
		}
		newbyte = *jpg_data.file_point++;
		jpg_data.size--;
		jpg_data.bits_buf += 8;
		jpg_data.buf = (jpg_data.buf << 8) | newbyte;
		if (newbyte == 0xFF) {
			if (jpg_data.size) {
				unsigned char marker = *jpg_data.file_point++;
				jpg_data.size--;
				switch (marker) {
				case 0:    break;
				case 0xD9: jpg_data.size = 0; break;
				default:
					if ((marker & 0xF8) != 0xD0)
						jpg_data.error = error_stat;
					else {
						jpg_data.buf = (jpg_data.buf << 8) | marker;
						jpg_data.bits_buf += 8;
					}
				}
			} else
				jpg_data.error = error_stat;
		}
	}
	return (jpg_data.buf >> (jpg_data.bits_buf - bits)) & ((1 << bits) - 1);
}

void bits_Skip(int bits) {
	if (jpg_data.bits_buf < bits)
		(void) bits_Show(bits);
	jpg_data.bits_buf -= bits;
}

int bits_Get(int bits) {
	int res = bits_Show(bits);
	bits_Skip(bits);
	return res;
}

int VLC_Get(vlc_code* vlc, unsigned char* code) {
	int value = bits_Show(16);
	int bits = vlc[value].bits;
	if (!bits) { jpg_data.error = error_stat; return 0; }
	bits_Skip(bits);
	value = vlc[value].code;
	if (code) *code = (unsigned char) value;
	bits = value & 15;
	if (!bits) return 0;
	value = bits_Get(bits);
	if (value < (1 << (bits - 1)))
		value += ((-1) << bits) + 1;
	return value;
}

void Block_Decode(jpg_comp* c, unsigned char* out) {
	unsigned char code = 0;
	int value, coef = 0;
	//Inverse quantization
	//First value in block (0: top left) uses a predictor.
	memset(jpg_data.block, 0, sizeof(jpg_data.block));
	c->dc_pred += VLC_Get(&jpg_data.vlc_table[c->dc_table][0], NULL);
	jpg_data.block[0] = (c->dc_pred) * jpg_data.q_table[c->qt_sel][0];
	do {
		value = VLC_Get(&jpg_data.vlc_table[c->ac_table][0], &code);
		if (!code) break;  // EOB
		if (!(code & 0x0F) && (code != 0xF0)) ERROR;
		coef += (code >> 4) + 1;
		if (coef > 63) ERROR;
		jpg_data.block[(int) ZigZag[coef]] = value * jpg_data.q_table[c->qt_sel][coef];
	} while (coef < 63);
	for (coef = 0;  coef < 64;  coef += 8)
		Row_IDCT(&jpg_data.block[coef]);
	for (coef = 0;  coef < 8;  ++coef)
		Col_IDCT(&jpg_data.block[coef], &out[coef], c->step);
}

void Upsample(jpg_comp* c) {
	int x, y, xshift = 0, yshift = 0;
	unsigned char *out, *lin, *lout;
	while (c->width < jpg_data.width) { c->width <<= 1; ++xshift; }
	while (c->height < jpg_data.height) { c->height <<= 1; ++yshift; }
	out = (unsigned char*) malloc(c->width * c->height);
	if (!out) ERROR;
	lin = c->pixels;
	lout = out;
	for (y = 0;  y < c->height;  ++y) {
		lin = &c->pixels[(y >> yshift) * c->step];
		for (x = 0;  x < c->width;  ++x)
			lout[x] = lin[x >> xshift];
		lout += c->width;
	}
	c->step = c->width;
	free(c->pixels);
	c->pixels = out;
}

unsigned char * Sepia(unsigned char rgb[]){
	int koef = (int) (rgb[0]+rgb[1]+rgb[2])/3;
	rgb[0] = Clip(koef + 46);
	rgb[1] = Clip(koef);
	rgb[2] = Clip(koef - 46);
	if (rgb[0] > 255) rgb[0] = 255;
	if (rgb[1] > 255) rgb[1] = 255;
	if (rgb[2] > 255) rgb[2] = 255;
	return rgb;

}

void Convert() {
	int i;
	jpg_comp* c;
	for (i = 0, c = jpg_data.comp;  i < jpg_data.ncomp;  ++i, ++c) {

		if ((c->width < jpg_data.width) || (c->height < jpg_data.height))
			Upsample(c);
		if ((c->width < jpg_data.width) || (c->height < jpg_data.height)) ERROR;
	}
	if (jpg_data.ncomp == 3) {
		// convert to RGB_point
		int x, yy;
		unsigned char *pRGB_point = jpg_data.RGB_point;
		const unsigned char *py  = jpg_data.comp[0].pixels;
		const unsigned char *pcb = jpg_data.comp[1].pixels;
		const unsigned char *pcr = jpg_data.comp[2].pixels;
		for (yy = jpg_data.height;  yy;  --yy) {
			for (x = 0;  x < jpg_data.width;  ++x) {
				register int y = py[x] << 8;
				register int cb = pcb[x] - 128;
				register int cr = pcr[x] - 128;
				unsigned char rgb[3];
				rgb[0] = Clip((y            + 359 * cr + 128) >> 8);
				rgb[1] = Clip((y -  88 * cb - 183 * cr + 128) >> 8);
				rgb[2] = Clip((y + 454 * cb            + 128) >> 8);
				*rgb = * Sepia(rgb);

				*pRGB_point++ = rgb[0];
				*pRGB_point++ = rgb[1];
				*pRGB_point++ = rgb[2];
			}

			py += jpg_data.comp[0].step;
			pcb += jpg_data.comp[1].step;
			pcr += jpg_data.comp[2].step;
		}

	}
}

void Start() {
	memset(&jpg_data, 0, sizeof(jpg_cont));
}

void Destroy() {
	int i;
	for (i = 0;  i < 3;  ++i) 
		free((void*) jpg_data.comp[i].pixels);
	free((void*) jpg_data.RGB_point);
	Start();
}

unsigned char Clip(const int x) {
	if (x < 0)
		return 0;
	else if (x > 0xFF) 
		return 0xFF;
	else
		return (unsigned char)x;
}

void length_Get() {
	if (jpg_data.size < 2) ERROR;
	jpg_data.length = (jpg_data.file_point[0] * 256) | jpg_data.file_point[1];
	if (jpg_data.length > jpg_data.size) ERROR;
	Skip(2);
}

void Skip(int count) {
	jpg_data.file_point += count;
	jpg_data.size -= count;
	jpg_data.length -= count;
	if (jpg_data.size < 0) jpg_data.error = error_stat;
}

int height_Get() {
	return jpg_data.height;
}

int width_Get() {
	return jpg_data.width;
}

unsigned char* Image() {
	if (jpg_data.ncomp == 1)
		return  jpg_data.comp[0].pixels;
	else return jpg_data.RGB_point;
}

int size_Get() {
	return jpg_data.width * jpg_data.height * jpg_data.ncomp;
}
