//Documentation: http://blog.thescorpius.com/index.php/2017/07/15/presentation-graphic-stream-sup-files-bluray-subtitle-format/

size_t const HEADER_SIZE = 13;

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

enum e_objectFlags : uint8_t {
    none    = 0x00,
    forced  = 0x40,
    cropped = 0x80
};
struct t_compositionObject {
    uint16_t objectID;
    uint8_t  windowID;
    uint8_t  objectCroppedAndForcedFlag;
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

struct t_ODS {
    uint16_t objectID;
    uint8_t  objectVersionNumber;
    uint8_t  lastInSequenceFlag;
    uint32_t dataLength;
    uint16_t width;
    uint16_t height;
    //uint8_t* data;
};


uint16_t swapEndianness(uint16_t input) {
    uint16_t output;

    output = (input >> 8) | (input << 8);

    return output;
}

uint32_t swapEndianness(uint32_t input) {
    uint32_t output;

    output = ((input >> 24) & 0xff)     |  // move byte 3 to byte 0
             ((input << 8 ) & 0xff0000) |  // move byte 1 to byte 2
             ((input >> 8 ) & 0xff00)   |  // move byte 2 to byte 1
             ((input << 24) & 0xff000000); // byte 0 to byte 3;

    return output;
}


t_header ReadHeader(uint8_t* buffer) {
    t_header header = {};

    header.header      = swapEndianness(*(uint16_t*)&buffer[0]);
    header.pts1        = swapEndianness(*(uint32_t*)&buffer[2]);
    header.dts         = swapEndianness(*(uint32_t*)&buffer[6]);
    header.segmentType =                *(uint8_t*) &buffer[10];
    header.dataLength  = swapEndianness(*(uint16_t*)&buffer[11]);

    return header;
}

void WriteHeader(t_header header, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0]))  = swapEndianness(header.header);
    *((uint32_t*)(&buffer[2]))  = swapEndianness(header.pts1);
    *((uint32_t*)(&buffer[6]))  = swapEndianness(header.dts);
    *((uint8_t*) (&buffer[10])) =                header.segmentType;
    *((uint16_t*)(&buffer[11])) = swapEndianness(header.dataLength);
}


t_WDS ReadWDS(uint8_t* buffer) {
    t_WDS wds;

    wds.numberOfWindows = *(uint8_t*)&buffer[0];
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;

        wds.windows[i].windowID      =                *(uint8_t*) &buffer[bufferStartIdx + 0];
        wds.windows[i].WindowsHorPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 1]);
        wds.windows[i].WindowsVerPos = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 3]);
        wds.windows[i].WindowsWidth  = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 5]);
        wds.windows[i].WindowsHeight = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 7]);
    }

    return wds;
}

void WriteWDS(t_WDS wds, uint8_t* buffer) {
    *((uint8_t*)(&buffer[0])) = wds.numberOfWindows;
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;

        *((uint8_t*) (&buffer[bufferStartIdx + 0])) =                wds.windows[i].windowID;
        *((uint16_t*)(&buffer[bufferStartIdx + 1])) = swapEndianness(wds.windows[i].WindowsHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 3])) = swapEndianness(wds.windows[i].WindowsVerPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 5])) = swapEndianness(wds.windows[i].WindowsWidth);
        *((uint16_t*)(&buffer[bufferStartIdx + 7])) = swapEndianness(wds.windows[i].WindowsHeight);
    }
}


t_PCS ReadPCS(uint8_t* buffer) {
    t_PCS pcs;

    pcs.width                = swapEndianness(*(uint16_t*)&buffer[0]);
    pcs.height               = swapEndianness(*(uint16_t*)&buffer[2]);
    pcs.frameRate            =                *(uint8_t*) &buffer[4];
    pcs.compositionNumber    = swapEndianness(*(uint16_t*)&buffer[5]);
    pcs.compositionState     =                *(uint8_t*) &buffer[7];
    pcs.paletteUpdFlag       =                *(uint8_t*) &buffer[8];
    pcs.paletteID            =                *(uint8_t*) &buffer[9];
    pcs.numCompositionObject =                *(uint8_t*) &buffer[10];

    size_t bufferStartIdx = 11;
    for (int i = 0; i < pcs.numCompositionObject; i++) {
        pcs.compositionObject[i].objectID                   = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 0]);
        pcs.compositionObject[i].windowID                   =                *(uint8_t*) &buffer[bufferStartIdx + 2];
        pcs.compositionObject[i].objectCroppedAndForcedFlag =                *(uint8_t*) &buffer[bufferStartIdx + 3];
        pcs.compositionObject[i].objectHorPos               = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 4]);
        pcs.compositionObject[i].objectVerPos               = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 6]);
        if (pcs.compositionObject[i].objectCroppedAndForcedFlag & e_objectFlags::cropped){
            pcs.compositionObject[i].objCropHorPos          = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 8]);
            pcs.compositionObject[i].objCropVerPos          = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 10]);
            pcs.compositionObject[i].objCropWidth           = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 12]);
            pcs.compositionObject[i].objCropHeight          = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 14]);
            bufferStartIdx += 8;
        }
        bufferStartIdx += 8;
    }

    return pcs;
}

void WritePCS(t_PCS pcs, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0]))  = swapEndianness(pcs.width);
    *((uint16_t*)(&buffer[2]))  = swapEndianness(pcs.height);
    *((uint8_t*) (&buffer[4]))  =                pcs.frameRate;
    *((uint16_t*)(&buffer[5]))  = swapEndianness(pcs.compositionNumber);
    *((uint8_t*) (&buffer[7]))  =                pcs.compositionState;
    *((uint8_t*) (&buffer[8]))  =                pcs.paletteUpdFlag;
    *((uint8_t*) (&buffer[9]))  =                pcs.paletteID;
    *((uint8_t*) (&buffer[10])) =                pcs.numCompositionObject;

    size_t bufferStartIdx = 11;
    for (int i = 0; i < pcs.numCompositionObject; i++) {
        *((uint16_t*)(&buffer[bufferStartIdx + 0]))  = swapEndianness(pcs.compositionObject[i].objectID);
        *((uint8_t*) (&buffer[bufferStartIdx + 2]))  =                pcs.compositionObject[i].windowID;
        *((uint8_t*) (&buffer[bufferStartIdx + 3]))  =                pcs.compositionObject[i].objectCroppedAndForcedFlag;
        *((uint16_t*)(&buffer[bufferStartIdx + 4]))  = swapEndianness(pcs.compositionObject[i].objectHorPos);
        *((uint16_t*)(&buffer[bufferStartIdx + 6]))  = swapEndianness(pcs.compositionObject[i].objectVerPos);
        if (pcs.compositionObject[i].objectCroppedAndForcedFlag & e_objectFlags::cropped){
            *((uint16_t*)(&buffer[bufferStartIdx + 8]))  = swapEndianness(pcs.compositionObject[i].objCropHorPos);
            *((uint16_t*)(&buffer[bufferStartIdx + 10])) = swapEndianness(pcs.compositionObject[i].objCropVerPos);
            *((uint16_t*)(&buffer[bufferStartIdx + 12])) = swapEndianness(pcs.compositionObject[i].objCropWidth);
            *((uint16_t*)(&buffer[bufferStartIdx + 14])) = swapEndianness(pcs.compositionObject[i].objCropHeight);
            bufferStartIdx += 8;
        }
        bufferStartIdx += 8;
    }
}


t_PDS ReadPDS(uint8_t* buffer, size_t segment_size) {
    t_PDS pds;

    pds.paletteID            = *(uint8_t*)&buffer[0];
    pds.paletteVersionNumber = *(uint8_t*)&buffer[1];

    pds.paletteNum = (segment_size - 2) / 5;

    for (int i = 0; i < pds.paletteNum; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        pds.palette[i].paletteEntryID = *(uint8_t*)&buffer[bufferStartIdx + 0];
        pds.palette[i].paletteY       = *(uint8_t*)&buffer[bufferStartIdx + 1];
        pds.palette[i].paletteCb      = *(uint8_t*)&buffer[bufferStartIdx + 2];
        pds.palette[i].paletteCr      = *(uint8_t*)&buffer[bufferStartIdx + 3];
        pds.palette[i].paletteA       = *(uint8_t*)&buffer[bufferStartIdx + 4];
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


t_ODS ReadODS(uint8_t* buffer) {
    t_ODS ods;

    ods.objectID            = swapEndianness(*(uint16_t*)&buffer[0]);
    ods.objectVersionNumber =                *(uint8_t*) &buffer[2];
    ods.lastInSequenceFlag  =                *(uint8_t*) &buffer[3];
    ods.dataLength          = swapEndianness(*(uint32_t*)&buffer[3]) & 0xffffff;
    ods.width               = swapEndianness(*(uint16_t*)&buffer[7]);
    ods.height              = swapEndianness(*(uint16_t*)&buffer[9]);

    return ods;
}
