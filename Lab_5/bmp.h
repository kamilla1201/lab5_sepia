#pragma pack(1)
	typedef struct BMP_Head // Bitmap header and Bitmap info header
	{
		// Bitmap header - 14 bytes

		char         type[2];     // 2 bytes - 'B' and 'M'
		unsigned int file_size;     // 4 bytes
		short int    reserved1;     // 2 bytes
		short int    reserved2;     // 2 bytes
		unsigned int offset;   // 4 bytes
		
		// Bitmap info header - 40 bytes

		unsigned int header_size;    // 4 bytes - 40
		unsigned int width;         // 4 bytes
		unsigned int height;        // 4 bytes
		short int    planes;        // 2 bytes
		short int    bit_count;      // 2 bytes
		unsigned int compression;    // 4 bytes
		unsigned int image_size;     // 4 bytes
		unsigned int x_pix; // 4 bytes
		unsigned int y_pix; // 4 bytes
		unsigned int clr_used;       // 4 bytes
		unsigned int clr_important;  // 4 bytes
	}BMP_Header;

	#pragma pack()

	
	void BMP_Save(const char* output_name, int width, int height, unsigned char* RGB);