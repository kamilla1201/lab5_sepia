
#define W1 2841 // 2048*sqrt(2)*cos(1*pi/16)
#define W2 2676 // 2048*sqrt(2)*cos(2*pi/16)
#define W3 2408 // 2048*sqrt(2)*cos(3*pi/16)
#define W5 1609 // 2048*sqrt(2)*cos(5*pi/16)
#define W6 1108 // 2048*sqrt(2)*cos(6*pi/16)
#define W7 565  // 2048*sqrt(2)*cos(7*pi/16)

void Rows_IDCT(int* block); // rows of the inverse discrete cosine transform

void Columns_IDCT(const int* block, unsigned char *out, int step); // columns of the inverse discrete cosine transform

unsigned char Clip(const int x); // convert to unsigned char