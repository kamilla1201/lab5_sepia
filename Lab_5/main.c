#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "decode.h"
#include "bmp.h"

int main(int argc, char* argv[]) {
	int size;
	char *buf;
	FILE *f;

	if (argc < 2) {
		printf("Error reading command arguments. Please try again...", argv[0]);
		getchar();
		return 2;
	}
	fopen_s(&f, argv[1], "rb");
	if (!f) {
		printf("Error opening jpg. Please try again...");
		getchar();
		return 1;
	}
	fseek(f, 0, SEEK_END);
	size = (int) ftell(f);
	buf = (char*)malloc(size);
	fseek(f, 0, SEEK_SET);
	size = (int) fread(buf, 1, size, f);
	fclose(f);

	Start();
	if (Decode(buf, size)) {
		printf("Error decoding jpg. Please try again...");
		getchar();
		return 1;
	}

	BMP_Save("output.bmp", Width_Get(), Height_Get(), Image());
	Destroy();
	return 0;
}