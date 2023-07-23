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
    unsigned long hh;
    unsigned long mm;
    unsigned long ss;
    unsigned long ms;
};

struct t_crop {
    uint16_t left;
    uint16_t top;
    uint16_t right;
    uint16_t bottom;
};

struct t_header {
    uint16_t header;
    uint32_t pts1;
    uint32_t dts;
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

struct t_palette {
    uint8_t paletteEntryID;
    uint8_t paletteY;
    uint8_t paletteCr;
    uint8_t paletteCb;
    uint8_t paletteA;
};

struct t_PDS {
    uint8_t paletteID;
    uint8_t paletteVersionNumber;
    t_palette palette[255];
    uint8_t paletteNum; //not in format, only used internally
};

struct t_cmd {
    int32_t delay = 0;
    t_crop crop = {};
    double resync = 1;
    bool addZero = false;
    double tonemap = 1;
};

uint16_t swapEndianness(uint16_t input) {
    uint16_t output;

    output = (input >> 8) | (input << 8);

    return output;
}

uint32_t swapEndianness(uint32_t input) {
    uint32_t output;

    output = ((input >> 24) & 0xff) |   // move byte 3 to byte 0
        ((input << 8) & 0xff0000) |   // move byte 1 to byte 2
        ((input >> 8) & 0xff00) |   // move byte 2 to byte 1
        ((input << 24) & 0xff000000); // byte 0 to byte 3;

    return output;
}

t_header ReadHeader(uint8_t* buffer) {
    t_header header = {};

    header.header = swapEndianness(*(uint16_t*)&buffer[0]);
    header.pts1 = swapEndianness(*(uint32_t*)&buffer[2]);
    header.dts = swapEndianness(*(uint32_t*)&buffer[6]);
    header.segmentType = *(uint8_t*)&buffer[10];
    header.dataLength = swapEndianness(*(uint16_t*)&buffer[11]);

    return header;
}

void WriteHeader(t_header header, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0])) = swapEndianness(header.header);
    *((uint32_t*)(&buffer[2])) = swapEndianness(header.pts1);
    *((uint32_t*)(&buffer[6])) = swapEndianness(header.dts);
    *((uint8_t*)(&buffer[10])) = header.segmentType;
    *((uint16_t*)(&buffer[11])) = swapEndianness(header.dataLength);
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
        size_t bufferStartIdx = 11 + (size_t)i * 8;

        pcs.compositionObject[i].objectID = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 0]);
        pcs.compositionObject[i].windowID = *(uint8_t*)&buffer[bufferStartIdx + 2];
        pcs.compositionObject[i].objectCroppedFlag = *(uint8_t*)&buffer[bufferStartIdx + 3];
        pcs.compositionObject[i].objectHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 4]);
        pcs.compositionObject[i].objectVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 6]);
        /*
        pcs.compositionObject.objCropHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 8]);
        pcs.compositionObject.objCropVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 10]);
        pcs.compositionObject.objCropWidth = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 12]);
        pcs.compositionObject.objCropHeight = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 14]);
        */
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
        size_t bufferStartIdx = 11 + (size_t)i * 8;

        *((uint16_t*)(&buffer[bufferStartIdx + 0])) = swapEndianness(pcs.compositionObject[i].objectID);
        *((uint8_t*)(&buffer[bufferStartIdx + 2])) = pcs.compositionObject[i].windowID;
        *((uint8_t*)(&buffer[bufferStartIdx + 3])) = pcs.compositionObject[i].objectCroppedFlag;
        *((uint16_t*)(&buffer[bufferStartIdx + 4])) = swapEndianness(pcs.compositionObject[i].objectHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 6])) = swapEndianness(pcs.compositionObject[i].objectVerPos);
    }

    /*
    *((uint16_t*)(&buffer[bufferStartIdx + 8])) = swapEndianness(pcs.compositionObject.objCropHorPos);
    *((uint16_t*)(&buffer[bufferStartIdx + 10])) = swapEndianness(pcs.compositionObject.objCropVerPos);
    *((uint16_t*)(&buffer[bufferStartIdx + 12])) = swapEndianness(pcs.compositionObject.objCropWidth);
    *((uint16_t*)(&buffer[bufferStartIdx + 14])) = swapEndianness(pcs.compositionObject.objCropHeight);
    */
}

t_PDS ReadPDS(uint8_t* buffer, size_t segment_size) {
    t_PDS pds;

    pds.paletteID = *(uint8_t*)&buffer[0];
    pds.paletteVersionNumber = *(uint8_t*)&buffer[1];
    pds.paletteNum = (segment_size - 2) / 5;

    for (int i = 0; i < pds.paletteNum; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        pds.palette[i].paletteEntryID = *(uint8_t*)&buffer[bufferStartIdx + 0];
        pds.palette[i].paletteY = *(uint8_t*)&buffer[bufferStartIdx + 1];
        pds.palette[i].paletteCb = *(uint8_t*)&buffer[bufferStartIdx + 2];
        pds.palette[i].paletteCr = *(uint8_t*)&buffer[bufferStartIdx + 3];
        pds.palette[i].paletteA = *(uint8_t*)&buffer[bufferStartIdx + 4];
    }

    return pds;
}

void WritePDS(t_PDS pds, uint8_t* buffer) {

    *((uint8_t*)(&buffer[0])) = pds.paletteID;
    *((uint8_t*)(&buffer[1])) = pds.paletteVersionNumber;

    for (int i = 0; i < pds.paletteNum; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        *((uint8_t*)(&buffer[bufferStartIdx + 0])) = pds.palette[i].paletteEntryID;
        *((uint8_t*)(&buffer[bufferStartIdx + 1])) = pds.palette[i].paletteY;
        *((uint8_t*)(&buffer[bufferStartIdx + 2])) = pds.palette[i].paletteCb;
        *((uint8_t*)(&buffer[bufferStartIdx + 3])) = pds.palette[i].paletteCr;
        *((uint8_t*)(&buffer[bufferStartIdx + 4])) = pds.palette[i].paletteA;
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

t_timestamp PTStoTimestamp(uint32_t pts) {
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

bool ParseCMD(int32_t argc, char** argv, t_cmd& cmd) {
    cmd.delay = 0;
    cmd.crop = {};
    cmd.resync = 1;
    cmd.addZero = false;
    cmd.tonemap = 1;
    int i = 3;
    if (argc == 4) {
        //backward compatibility
        cmd.delay = (int32_t)round(atof(argv[3]) * PTS_MULT);
        if (cmd.delay != 0) {
            printf("Running in backwards-compatibility mode\n");
            return true;
        }
    }

    while (i < argc) {
        std::string command = argv[i];
        std::transform(command.begin(), command.end(), command.begin(), ::tolower);

        if (command == "delay") {
            cmd.delay = (int32_t)round(atof(argv[i + 1]) * PTS_MULT);
            i += 2;
        }
        else if (command == "crop") {
            cmd.crop.left = atoi(argv[i + 1]);
            cmd.crop.top = atoi(argv[i + 2]);
            cmd.crop.right = atoi(argv[i + 3]);
            cmd.crop.bottom = atoi(argv[i + 4]);
            i += 5;
        }
        else if (command == "resync") {
            std::string strFactor = argv[i + 1];
            size_t idx = strFactor.find("/");
            if (idx != SIZE_MAX) {
                double num = atof(strFactor.substr(0, idx).c_str());
                double den = atof(strFactor.substr(idx + 1, strFactor.length()).c_str());

                cmd.resync = num / den;
            }
            else {
                cmd.resync = atof(argv[i + 1]);
            }

            cmd.delay = (int32_t)round(((double)cmd.delay * cmd.resync));

            i += 2;
        }
        else if (command == "add_zero") {
            cmd.addZero = true;
            i += 1;
        }
        else if (command == "tonemap") {
            cmd.tonemap = atof(argv[i + 1]);
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


    if (argc < 4) {
        printf("Usage: SupMover (<input.sup> <output.sup>) [delay (ms)] [crop (<left> <top> <right> <bottom>)] [resync (<num>/<den> | multFactor)] [add_zero] [tonemap <perc>]\r\n");
        printf("delay and resync command are executed in the order supplied\r\n");
        return 0;
    }
    t_cmd cmd = {};

    if (!ParseCMD(argc, argv, cmd)) {
        printf("Error parsing input\r\n");
        return -1;
    }


    bool doDelay = cmd.delay != 0;
    bool doCrop = (cmd.crop.left + cmd.crop.top + cmd.crop.right + cmd.crop.bottom) > 0;
    bool doResync = cmd.resync != 1;
    bool doTonemap = cmd.tonemap != 1;

    bool doSomething = doDelay || doCrop || doResync || cmd.addZero || doTonemap;

    FILE* input = fopen(argv[1], "rb");
    if (input == NULL) {
        printf("Unable to open input file!");
        return -1;
    }
    FILE* output = fopen(argv[2], "wb");
    if (output == NULL) {
        printf("Unable to open output file!");
        fclose(input);
        return -1;
    }

    fseek(input, 0, SEEK_END);
    size = ftell(input);
    if (size != 0) {
        fseek(input, 0, SEEK_SET);

        uint8_t* buffer = (uint8_t*)calloc(1, size);
        if (buffer == NULL) {
            return -1;
        }

        fread(buffer, size, 1, input);
        if (doSomething) {
            size_t start = 0;

            t_rect screenRect = {};

            t_WDS wds = {};
            t_PCS pcs = {};
            t_PDS pds = {};

            size_t offesetCurrPCS = 0;
            bool fixPCS = false;

            for (start = 0; start < size; start = start + HEADER_SIZE + header.dataLength) {
                header = ReadHeader(&buffer[start]);
                if (header.header != 0x5047) {
                    printf("Correct header not found at position %zd, abort!", start);
                    fclose(input);
                    fclose(output);
                    return -1;
                }

                if (doResync) {
                    header.pts1 = (uint32_t)round((double)header.pts1 * cmd.resync);
                }
                if (doDelay) {
                    header.pts1 = header.pts1 + cmd.delay;
                }

                if (doResync || doDelay) {
                    WriteHeader(header, &buffer[start]);
                }

                switch (header.segmentType) {
                case 0x14:
                    //printf("PDS\r\n");
                    if (doTonemap) {
                        pds = ReadPDS(&buffer[start + HEADER_SIZE], header.dataLength);

                        for (int i = 0; i < pds.paletteNum; i++) {
                            //convert Y from TV level (16-235) to full range
                            double expandedY = ((((double)pds.palette[i].paletteY - 16.0) * (255.0 / (235.0 - 16.0))) / 255.0);
                            double tonemappedY = expandedY * cmd.tonemap;
                            double clampedY = std::min(1.0, std::max(tonemappedY, 0.0));
                            double newY = round((clampedY * (235.0 - 16.0)) + 16.0);
                            pds.palette[i].paletteY = (uint8_t)newY;
                        }

                        WritePDS(pds, &buffer[start + HEADER_SIZE]);
                    }
                    break;
                case 0x15:
                    //printf("ODS\r\n");
                    break;
                case 0x16:
                    //printf("PCS\r\n");
                    if (doCrop || cmd.addZero) {
                        pcs = ReadPCS(&buffer[start + HEADER_SIZE]);
                        offesetCurrPCS = start;

                        if (doCrop) {
                            screenRect.x = 0 + cmd.crop.left;
                            screenRect.y = 0 + cmd.crop.top;
                            screenRect.width = pcs.width - (cmd.crop.left + cmd.crop.right);
                            screenRect.height = pcs.height - (cmd.crop.top + cmd.crop.bottom);

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

                                if (cmd.crop.left > pcs.compositionObject[i].objectHorPos) {
                                    pcs.compositionObject[i].objectHorPos = 0;
                                }
                                else {
                                    pcs.compositionObject[i].objectHorPos -= cmd.crop.left;
                                }

                                if (cmd.crop.top > pcs.compositionObject[i].objectVerPos) {
                                    pcs.compositionObject[i].objectVerPos = 0;
                                }
                                else {
                                    pcs.compositionObject[i].objectVerPos -= cmd.crop.top;
                                }
                            }
                        }

                        if (cmd.addZero) {
                            if (pcs.compositionNumber == 0) {
                                uint8_t zeroBuffer[60];
                                uint8_t pos = 0;
                                t_header zeroHeader(header);
                                zeroHeader.pts1 = 0;
                                zeroHeader.dataLength = 11; //Length of upcoming PCS
                                WriteHeader(zeroHeader, &zeroBuffer[pos]);
                                pos += 13;
                                t_PCS zeroPcs(pcs);
                                zeroPcs.compositionState = 0;
                                zeroPcs.paletteUpdFlag = 0;
                                zeroPcs.paletteID = 0;
                                zeroPcs.numCompositionObject = 0;
                                WritePCS(zeroPcs, &zeroBuffer[pos]);
                                pos += zeroHeader.dataLength;

                                zeroHeader.segmentType = 0x17; // WDS
                                zeroHeader.dataLength = 10; //Length of upcoming WDS
                                WriteHeader(zeroHeader, &zeroBuffer[pos]);
                                pos += 13;
                                t_WDS zeroWds;
                                zeroWds.numberOfWindows = 1;
                                zeroWds.windows[0].windowID = 0;
                                zeroWds.windows[0].WindowsHorPos = 0;
                                zeroWds.windows[0].WindowsVerPos = 0;
                                zeroWds.windows[0].WindowsWidth = 0;
                                zeroWds.windows[0].WindowsHeight = 0;
                                WriteWDS(zeroWds, &zeroBuffer[pos]);
                                pos += zeroHeader.dataLength;

                                zeroHeader.segmentType = 0x80; // END
                                zeroHeader.dataLength = 0; //Length of upcoming END
                                WriteHeader(zeroHeader, &zeroBuffer[pos]);
                                pos += 13;

                                printf("Writing %d bytes as first display set\n", pos);
                                fwrite(zeroBuffer, pos, 1, output);
                            }
                            pcs.compositionNumber += 1;
                        }

                        WritePCS(pcs, &buffer[start + HEADER_SIZE]);
                    }
                    break;
                case 0x17:
                    //printf("WDS\r\n");
                    fixPCS = false;
                    if (doCrop) {
                        wds = ReadWDS(&buffer[start + HEADER_SIZE]);

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

                            if (wndRect.width > screenRect.width
                                || wndRect.height > screenRect.height) {
                                t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                printf("Window is bigger then new screen area at timestamp %lu:%02lu:%02lu.%03lu\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);
                                printf("Implement it!\r\n");
                                /*
                                pcs.width = wndRect.width;
                                pcs.height = wndRect.height;
                                fixPCS = true;
                                */
                            }
                            else {
                                if (!rectIsContained(screenRect, wndRect)) {
                                    t_timestamp timestamp = PTStoTimestamp(header.pts1);
                                    printf("Window is outside new screen area at timestamp %lu:%02lu:%02lu.%03lu\r\n", timestamp.hh, timestamp.mm, timestamp.ss, timestamp.ms);

                                    uint16_t wndRightPoint = wndRect.x + wndRect.width;
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
                            }

                            if (cmd.crop.left > wds.windows[i].WindowsHorPos) {
                                wds.windows[i].WindowsHorPos = 0;
                            }
                            else {
                                wds.windows[i].WindowsHorPos -= (cmd.crop.left + corrHor);
                            }

                            if (cmd.crop.top > wds.windows[i].WindowsVerPos) {
                                wds.windows[i].WindowsVerPos = 0;
                            }
                            else {
                                wds.windows[i].WindowsVerPos -= (cmd.crop.top + corrVer);
                            }

                            if (corrVer != 0) {
                                pcs.compositionObject[i].objectVerPos -= corrVer;
                                fixPCS = true;
                            }
                            if (corrHor != 0) {
                                pcs.compositionObject[i].objectHorPos -= corrHor;
                                fixPCS = true;
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