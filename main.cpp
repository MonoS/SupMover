#include <iostream>
#include <cstdio>
#include <math.h>
#include <algorithm>

//Documentation: http://blog.thescorpius.com/index.php/2017/07/15/presentation-graphic-stream-sup-files-bluray-subtitle-format/

//using namespace std;

double const PTS_MULT = 90.0f;
size_t const HEADER_SIZE = 13;

struct t_rect {
    uint16_t x;
    uint16_t y;
    uint16_t width;
    uint16_t height;
};

struct t_timestamp {
    uint32_t hh;
    uint32_t mm;
    uint32_t ss;
    uint32_t ms;
};

struct t_crop {
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
};

struct t_header{
	uint16_t header;
	uint32_t pts1;
	uint16_t dataLength;
    uint8_t  segmentType;
};

struct t_window {
    uint8_t  windowID;
    uint16_t WindowsHorPos;
    uint16_t WindowsVerPos;
    uint16_t WindowsWidth;
    uint16_t WindowsHeight;
};

struct t_WDS {
    uint8_t  numberOfWindows;
    t_window windows[256];
};

struct t_compositionObject {
    uint16_t objectID;
    uint8_t  windowID;
    uint8_t  objectCroppedFlag;
    uint16_t objectHorPos;
    uint16_t objectVerPos;
    uint16_t objCropHorPos;
    uint16_t objCropVerPos;
    uint16_t objCropWidth;
    uint16_t objCropHeight;
};
struct t_PCS {
    uint16_t width;
    uint16_t height;
    uint8_t  frameRate;
    uint16_t compositionNumber;
    uint8_t  compositionState;
    uint8_t  paletteUpdFlag;
    uint8_t  paletteID;
    uint8_t  numCompositionObject;
    t_compositionObject compositionObject[256];
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

t_header ReadHeader(uint8_t* buffer) {
    t_header header = {};

    header.header = swapEndianness(*(uint16_t*)&buffer[0]);
    header.pts1 = swapEndianness(*(uint32_t*)&buffer[2]);
    header.segmentType = *(uint8_t*)&buffer[10];
    header.dataLength = swapEndianness(*(uint16_t*)&buffer[11]);

    return header;
}

void WriteHeader(t_header header, uint8_t* buffer) {
    *((uint32_t*)(&buffer[2])) = swapEndianness(header.pts1);
}

t_WDS ReadWDS(uint8_t* buffer) {
    t_WDS wds;

    wds.numberOfWindows = *(uint8_t*)&buffer[0];
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;
        wds.windows[i].windowID = *(uint8_t*)&buffer[bufferStartIdx + 0];
        wds.windows[i].WindowsHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 1]);
        wds.windows[i].WindowsVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 3]);
        wds.windows[i].WindowsWidth = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 5]);
        wds.windows[i].WindowsHeight = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 7]);
    }

    return wds;
}

void WriteWDS(t_WDS wds, uint8_t* buffer) {
    *((uint8_t*)(&buffer[0])) = wds.numberOfWindows;
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;
        *((uint8_t*)(&buffer[bufferStartIdx + 0])) = wds.windows[i].windowID;
        *((uint16_t*)(&buffer[bufferStartIdx + 1])) = swapEndianness(wds.windows[i].WindowsHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 3])) = swapEndianness(wds.windows[i].WindowsVerPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 5])) = swapEndianness(wds.windows[i].WindowsWidth);
        *((uint16_t*)(&buffer[bufferStartIdx + 7])) = swapEndianness(wds.windows[i].WindowsHeight);
    }
}

t_PCS ReadPCS(uint8_t* buffer) {
    t_PCS pcs;

    pcs.width = swapEndianness(*(uint16_t*)&buffer[0]);
    pcs.height = swapEndianness(*(uint16_t*)&buffer[2]);
    pcs.frameRate = *(uint8_t*)&buffer[4];
    pcs.compositionNumber = swapEndianness(*(uint16_t*)&buffer[5]);
    pcs.compositionState = *(uint8_t*)&buffer[7];
    pcs.paletteUpdFlag = *(uint8_t*)&buffer[8];
    pcs.paletteID = *(uint8_t*)&buffer[9];
    pcs.numCompositionObject = *(uint8_t*)&buffer[10];

    for (int i = 0; i < pcs.numCompositionObject; i++) {
        size_t bufferStartIdx = 11 + (size_t)i * 16;

        pcs.compositionObject[i].objectID = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 0]);
        pcs.compositionObject[i].windowID = *(uint8_t*)&buffer[bufferStartIdx + 2];
        pcs.compositionObject[i].objectCroppedFlag = *(uint8_t*)&buffer[bufferStartIdx + 3];
        pcs.compositionObject[i].objectHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 4]);
        pcs.compositionObject[i].objectVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 6]);
        pcs.compositionObject[i].objCropHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 8]);
        pcs.compositionObject[i].objCropVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 10]);
        pcs.compositionObject[i].objCropWidth = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 12]);
        pcs.compositionObject[i].objCropHeight = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 14]);
    }

    return pcs;
}

void WritePCS(t_PCS pcs, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0])) = swapEndianness(pcs.width);
    *((uint16_t*)(&buffer[2])) = swapEndianness(pcs.height);
    *((uint8_t*)(&buffer[4])) = pcs.frameRate;
    *((uint16_t*)(&buffer[5])) = swapEndianness(pcs.compositionNumber);
    *((uint8_t*)(&buffer[7])) = pcs.compositionState;
    *((uint8_t*)(&buffer[8])) = pcs.paletteUpdFlag;
    *((uint8_t*)(&buffer[9])) = pcs.paletteID;
    *((uint8_t*)(&buffer[10])) = pcs.numCompositionObject;

    for (int i = 0; i < pcs.numCompositionObject; i++) {
        size_t bufferStartIdx = 11 + (size_t)i * 16;

        *((uint16_t*)(&buffer[bufferStartIdx + 0])) = swapEndianness(pcs.compositionObject[i].objectID);
        *((uint8_t*)(&buffer[bufferStartIdx + 2])) = pcs.compositionObject[i].windowID;
        *((uint8_t*)(&buffer[bufferStartIdx + 3])) = pcs.compositionObject[i].objectCroppedFlag;
        *((uint16_t*)(&buffer[bufferStartIdx + 4])) = swapEndianness(pcs.compositionObject[i].objectHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 6])) = swapEndianness(pcs.compositionObject[i].objectVerPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 8])) = swapEndianness(pcs.compositionObject[i].objCropHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 10])) = swapEndianness(pcs.compositionObject[i].objCropVerPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 12])) = swapEndianness(pcs.compositionObject[i].objCropWidth);
        *((uint16_t*)(&buffer[bufferStartIdx + 14])) = swapEndianness(pcs.compositionObject[i].objCropHeight);
    }
}

bool rectIsContained(t_rect container, t_rect window) {
    if ((window.x + window.width) < (container.x + container.width)
        && (window.x) > (container.x)
        && (window.y) > (container.y)
        && (window.y + window.height) < (container.y + container.height)
        )
    {
        return true;
    }
    else
    {
        return false;
    }
}

t_timestamp PTStoTimestamp(uint32_t pts){
    t_timestamp res;

    res.ms = ((int)floor((float)pts / PTS_MULT));
    res.ss = res.ms / 1000;
    res.mm = res.ss / 60;
    res.hh = res.mm / 60;

    res.ms %= 1000;
    res.ss %= 60;
    res.mm %= 60;

    return res;
}

bool ParseCMD(int32_t argc, char** argv, int32_t& delay, t_crop& crop, double& resync) {
    delay = 0;
    crop = {};
    resync = 1;
    int i = 3;
    if (argc == 4) {
        //backward compatibility
        delay = (int32_t)round(atof(argv[3]) * PTS_MULT);
        return true;
    }

    while(i < argc) {
        std::string command = argv[i];
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);
        
        if (command == "delay") {
            delay = (int32_t)round(atof(argv[i+1]) * PTS_MULT);
            i += 2;
        }
        else if (command == "crop") {
            crop.left   = atoi(argv[i + 1]);
            crop.top    = atoi(argv[i + 2]);
            crop.right  = atoi(argv[i + 3]);
            crop.bottom = atoi(argv[i + 4]);
            i += 5;
        }
        else if (command == "resync") {
            std::string strFactor = argv[i + 1];
            size_t idx = strFactor.find("/");
            if (idx != SIZE_MAX) {
                double num = atof(strFactor.substr(0, idx).c_str());
                double den = atof(strFactor.substr(idx + 1, strFactor.length()).c_str());

                resync = num / den;
            }
            else {
                resync = atof(argv[i + 1]);
            }

            delay = (int32_t)round(((double)delay * resync));

            i += 2;
        }
        else {
            return false;
        }
    }

    return true;
}

int main(int32_t argc, char** argv)
{
    size_t size;
    t_header header;


    if(argc < 4){
        printf("Usage: SupMover (<input.sup> <output.sup>) [delay (ms)] [crop (<left> <top> <right> <bottom>)] [resync (<num>/<den> | multFactor)]\r\n");
        printf("delay and resync command are executed in the order supplied\r\n");
        return 0;
    }
    int32_t delay = {};
    t_crop crop = {};
    double resync = 1;

    if (!ParseCMD(argc, argv, delay, crop, resync)) {
        printf("Error parsing input\r\n");
        return -1;
    }

    bool doDelay = delay != 0;
    bool doCrop = (crop.left + crop.top + crop.right + crop.bottom) > 0;
    bool doResync = resync != 1;

    FILE* input; fopen_s(&input, argv[1], "rb");
    if(input == NULL){
        printf("Unable to open input file!");
        return -1;
    }
    FILE* output; fopen_s(&output, argv[2], "wb");
    if(output == NULL){
        printf("Unable to open output file!");
        fclose(input);
        return -1;
    }

    fseek(input, 0, SEEK_END);
    size = ftell(input);
    if(size != 0){
        fseek(input, 0, SEEK_SET);

        uint8_t* buffer = (uint8_t*)calloc(1, size);
        if (buffer == NULL) {
            return -1;
        }

        fread(buffer, size, 1, input);
        if(doDelay || doCrop || doResync){
            size_t start = 0;

            t_rect screenRect = {};

            t_WDS wds = {};
            t_PCS pcs = {};

            size_t offesetCurrPCS = 0;
            bool fixPCS = false;

            for(start = 0; start < size; start = start + HEADER_SIZE + header.dataLength){
                header = ReadHeader(&buffer[start]);
                if (header.header != 0x5047){
                    printf("Correct header not found at position %zd, abort!", start);
                    fclose(input);
                    fclose(output);
                    return -1;
                }

                if (doResync) {
                    header.pts1 = (uint32_t)round((double)header.pts1 * resync);
                }
                if (doDelay) {
                    header.pts1 = header.pts1 + delay;
                }

                if (doResync || doDelay) {
                    WriteHeader(header, &buffer[start]);
                }

                switch (header.segmentType) {
                case 0x14:
                    //printf("PDS\r\n");
                    break;
                case 0x15:
                    //printf("ODS\r\n");
                    break;
                case 0x16:
                    //printf("PCS\r\n");
                    if (doCrop) {
                        pcs = ReadPCS(&buffer[start + HEADER_SIZE]);
                        offesetCurrPCS = start;

                        screenRect.x = 0 + crop.left;
                        screenRect.y = 0 + crop.top;
                        screenRect.width = pcs.width - (crop.left + crop.right);
                        screenRect.height = pcs.height - (crop.top + crop.bottom);

                        pcs.width = screenRect.width;
                        pcs.height = screenRect.height;

                        if (pcs.numCompositionObject > 1) {
                            t_timestamp timestamp = PTStoTimestamp(header.pts1);
                            printf("Multiple composition object at timestamp %lu:%02lu:%02lu.%03lu! Please Check!\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                        }

                        for (int i = 0; i < pcs.numCompositionObject; i++) {
                            if (pcs.compositionObject[i].objectCroppedFlag == 0x40) {
                                t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                printf("Object Cropped Flag set at timestamp %lu:%02lu:%02lu.%03lu! Implement it!\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                            }

                            if (crop.left > pcs.compositionObject[i].objectHorPos) {
                                pcs.compositionObject[i].objectHorPos = 0;
                            }
                            else {
                                pcs.compositionObject[i].objectHorPos -= crop.left;
                            }

                            if (crop.top > pcs.compositionObject[i].objectVerPos) {
                                pcs.compositionObject[i].objectVerPos = 0;
                            }
                            else {
                                pcs.compositionObject[i].objectVerPos -= crop.top;
                            }
                        }

                        WritePCS(pcs, &buffer[start + HEADER_SIZE]);
                    }
                    break;
                case 0x17:
                    //printf("WDS\r\n");
                    if (doCrop) {
                        wds = ReadWDS(&buffer[start+ HEADER_SIZE]);

                        if (wds.numberOfWindows > 1) {
                            t_timestamp timestamp = PTStoTimestamp(header.pts1);
                            printf("Multiple windows at timestamp %lu:%02lu:%02lu.%03lu! Please Check!\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                        }

                        for (int i = 0; i < wds.numberOfWindows; i++) {
                            t_rect wndRect;
                            uint16_t corrHor = 0;
                            uint16_t corrVer = 0;

                            wndRect.x = wds.windows[i].WindowsHorPos;
                            wndRect.y = wds.windows[i].WindowsVerPos;
                            wndRect.width = wds.windows[i].WindowsWidth;
                            wndRect.height = wds.windows[i].WindowsHeight;

                            if (!rectIsContained(screenRect, wndRect)) {
                                t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                printf("Window is outside new screen area at timestamp %lu:%02lu:%02lu.%03lu\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                            
                                uint16_t wndRightPoint    = wndRect.x + wndRect.width;
                                uint16_t screenRightPoint = screenRect.x + screenRect.width;
                                if (wndRightPoint > screenRightPoint) {
                                    corrHor = wndRightPoint - screenRightPoint;
                                }

                                uint16_t wndBottomPoint = wndRect.y + wndRect.height;
                                uint16_t screenBottomPoint = screenRect.y + screenRect.height;
                                if (wndBottomPoint > screenBottomPoint) {
                                    corrVer = wndBottomPoint - screenBottomPoint;
                                }

                                if (corrHor + corrVer != 0) {
                                    printf("Please check\r\n");
                                }
                            }

                            if (crop.left > wds.windows[i].WindowsHorPos) {
                                wds.windows[i].WindowsHorPos = 0;
                            }
                            else {
                                wds.windows[i].WindowsHorPos -= (crop.left + corrHor);
                                if (corrHor != 0) {
                                    pcs.compositionObject[i].objectHorPos -= corrHor;
                                    fixPCS = true;
                                }
                            }

                            if (crop.top > wds.windows[i].WindowsVerPos) {
                                wds.windows[i].WindowsVerPos = 0;
                            }
                            else {
                                wds.windows[i].WindowsVerPos -= (crop.top + corrVer);
                                if (corrVer != 0) {
                                    pcs.compositionObject[i].objectVerPos -= corrVer;
                                    fixPCS = true;
                                }
                            }
                        }

                        if (fixPCS) {
                            WritePCS(pcs, &buffer[offesetCurrPCS + HEADER_SIZE]);
                        }
                        WriteWDS(wds, &buffer[start + HEADER_SIZE]);

                    }
                    break;
                case 0x80:
                    //printf("END\r\n");
                    screenRect = {};
                    pcs = {};
                    wds = {};
                    break;
                }
            }
        }

        fwrite(buffer, size, 1, output);
    }

    fclose(input);
    fclose(output);

    return 0;
}
