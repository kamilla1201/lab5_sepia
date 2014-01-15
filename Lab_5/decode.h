
typedef enum status {
    success_stat = 0, // no error, decoding successful
	error_stat, // error decoding
	end_of_file // finish decoding
} result_status;

typedef struct var_length_code {
	unsigned char bits, code;
}vlc_code;

typedef struct component {
	int cid;
	int ssx, ssy;
	int width, height;
	int step;
	int qt_sel;
	int ac_table, dc_table;
	int dc_pred;
	unsigned char *pixels;
}jpg_comp;

typedef struct context {
	result_status error;
	const unsigned char *file_point;
	int size;
	int length;
	int width, height;
	int m_block_width, m_block_height;
	int m_block_sizex, m_block_sizey;
	int ncomp;
	jpg_comp comp[3];
	int qt_used, qt_available;
	unsigned char q_table[4][64];
	vlc_code vlc_table[4][65536];
	int buf, bits_buf;
	int block[64];
	int restart_interval;
	unsigned char *RGB_point;
}jpg_cont;

static jpg_cont jpg_data;

unsigned char Clip(const int x); // convert to unsigned char

void Row_IDCT(int* block); // rows of the inverse discrete cosine transform

void Col_IDCT(const int* block, unsigned char *out, int step); // columns of the inverse discrete cosine transform

int bits_Show(int bits); // show bits

void bits_Skip(int bits); // skip bits

int bits_Get(int bits); // get bits

void Skip(int count); // skip bytes

void length_Get(); // get length of segment

void Skip_marker(); // comments segment

void SOF_decode(); // frame segment 

void DHT_decode(); // Huffman table segment 

void DQT_decode(); // quantization table segment DQT

int VLC_Get(vlc_code* vlc, unsigned char* code); // variable length decode

void Block_Decode(jpg_comp* c, unsigned char* out); // decode block 8x8

void SOS_decode(); //  scan segment 

void Upsample(jpg_comp* c); // upsample image

unsigned char * Sepia(unsigned char rgb[]); // convert RGB to Sepia

void Convert(); // convert YCbCr to RGB

void Start(); // set to zero

void Destroy(); // free memory

result_status Decode(const void* jpeg, const int size); // decode a jpg image

int width_Get(); // return width

int height_Get(); // return height

unsigned char* Image(); // return decoded image data

int size_Get(); // return size



