#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode.h"
#include "idct.h"


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
			
		case 0xFE: Skip_Marker(); break; 
		case 0xDB: DQT_Decode();  break;
		case 0xC0: SOF_Decode();  break;
		case 0xC4: DHT_Decode();  break;
		case 0xDA: SOS_Decode(); break;
		default:
			if ((jpg_data.file_point[-1] & 0xF0) == 0xE0) // APP0 - JFIF specification
				Skip_Marker();
			else
				return error_stat;
		}
	}
	if (jpg_data.error != end_of_file) return jpg_data.error;
	jpg_data.error = success_stat;
	Convert();
	return jpg_data.error;
}

void Skip_Marker() {
	Length_Get();
	Skip(jpg_data.length);
}

void DQT_Decode() {
	int i;
	unsigned char *t;
	Length_Get();
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

void SOF_Decode() {
	int i, ssxmax = 0, ssymax = 0;
	jpg_comp* c;
	Length_Get();
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

void DHT_Decode() {
	int codelen, currcnt, remain, spread, i, j;
	vlc_code *vlc;
	static unsigned char counts[16];
	Length_Get();
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

void SOS_Decode() {
	int i, m_block_x, m_block_y, sbx, sby;
	int restart_count = jpg_data.restart_interval, next_restart = 0;
	jpg_comp* c;
	Length_Get();
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
					i = Bits_Get(16);
					if (((i & 0xFFF8) != 0xFFD0) || ((i & 7) != next_restart)) ERROR;
					next_restart = (next_restart + 1) & 7;
					restart_count = jpg_data.restart_interval;
					for (i = 0;  i < 3;  ++i)
						jpg_data.comp[i].dc_pred = 0;
				}
	}
	jpg_data.error = end_of_file;
}


int Bits_Show(int bits) {
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

void Bits_Skip(int bits) {
	if (jpg_data.bits_buf < bits)
		(void) Bits_Show(bits);
	jpg_data.bits_buf -= bits;
}

int Bits_Get(int bits) {
	int res = Bits_Show(bits);
	Bits_Skip(bits);
	return res;
}

int VLC_Get(vlc_code* vlc, unsigned char* code) {
	int value = Bits_Show(16);
	int bits = vlc[value].bits;
	if (!bits) { jpg_data.error = error_stat; return 0; }
	Bits_Skip(bits);
	value = vlc[value].code;
	if (code) *code = (unsigned char) value;
	bits = value & 15;
	if (!bits) return 0;
	value = Bits_Get(bits);
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
		Rows_IDCT(&jpg_data.block[coef]);
	for (coef = 0;  coef < 8;  ++coef)
		Columns_IDCT(&jpg_data.block[coef], &out[coef], c->step);
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


void Length_Get() {
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

int Height_Get() {
	return jpg_data.height;
}

int Width_Get() {
	return jpg_data.width;
}

unsigned char* Image() {
	if (jpg_data.ncomp == 1)
		return  jpg_data.comp[0].pixels;
	else return jpg_data.RGB_point;
}

int Size_Get() {
	return jpg_data.width * jpg_data.height * jpg_data.ncomp;
}
