#include <iostream>
#include <cstdio>
#include <math.h>

using namespace std;

float const PTS_MULT = 90.0f;

struct t_section{
	uint16_t header;
	uint32_t pts1;
	uint16_t dataLength;
};

uint16_t swapEndianness(uint16_t input){
    uint16_t output;

    output = (input>>8) | (input<<8);

    return output;
}

uint32_t swapEndianness(uint32_t input){
    uint32_t output;

    output = ((input>>24) & 0xff)     |   // move byte 3 to byte 0
             ((input<<8)  & 0xff0000) |   // move byte 1 to byte 2
             ((input>>8)  & 0xff00)   |   // move byte 2 to byte 1
             ((input<<24) & 0xff000000 ); // byte 0 to byte 3;

    return output;
}

int main(int32_t argc, char** argv)
{
    size_t size;
    t_section sezione;

    if(argc != 4){
        printf("Usage: SupMover input.sup output.sup \"delay in ms\"");
        return 0;
    }

    uint32_t delay = (uint32_t)roundf(atof(argv[3]) * PTS_MULT);
    FILE* input  = fopen(argv[1], "rb");
    if(input == NULL){
        printf("Unable to open input file!");
        return 0;
    }
    FILE* output = fopen(argv[2], "wb");
    if(output == NULL){
        printf("Unable to open output file!");
        fclose(input);
        return 0;
    }

    fseek(input, 0, SEEK_END);
    size = ftell(input);
    if(size != 0){
        fseek(input, 0, SEEK_SET);

        uint8_t* buffer = (uint8_t*)calloc(1, size);

        fread(buffer, size, 1, input);
        if(delay != 0){
            size_t start = 0;

            for(start = 0; start < size; start = start + 13 + sezione.dataLength){
                sezione.header      = swapEndianness(*(uint16_t*)&buffer[start+0 ]);
                sezione.pts1        = swapEndianness(*(uint32_t*)&buffer[start+2 ]);
                sezione.dataLength  = swapEndianness(*(uint16_t*)&buffer[start+11]);

                if (sezione.header != 20551){
                    printf("Correct header not found at position %zd, abort!", start);
                    fclose(input);
                    fclose(output);
                    return -1;
                }

                sezione.pts1 = sezione.pts1 + delay;

                *((uint32_t*)(&buffer[start+2])) = swapEndianness(sezione.pts1);
            }
        }


        fwrite(buffer, size, 1, output);
    }

    fclose(input);
    fclose(output);

    return 0;
}
