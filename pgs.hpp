//Documentation: http://blog.thescorpius.com/index.php/2017/07/15/presentation-graphic-stream-sup-files-bluray-subtitle-format/

struct t_header {
    uint16_t header;
    uint32_t pts;
    uint32_t dts;
    uint8_t  segmentType;
    uint16_t dataLength;
};
size_t const HEADER_SIZE = 13;

struct t_window {
    uint8_t  id;
    uint16_t horizontalPosition;
    uint16_t verticalPosition;
    uint16_t width;
    uint16_t height;
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
    uint8_t  croppedAndForcedFlag;
    uint16_t horizontalPosition;
    uint16_t verticalPosition;
    uint16_t croppedHorizontalPosition;
    uint16_t croppedVerticalPosition;
    uint16_t croppedWidth;
    uint16_t croppedHeight;
};
struct t_PCS {
    uint16_t width;
    uint16_t height;
    uint8_t  frameRate;
    uint16_t compositionNumber;
    uint8_t  compositionState;
    uint8_t  paletteUpdateFlag;
    uint8_t  paletteID;
    uint8_t  numberOfCompositionObjects;
    t_compositionObject compositionObjects[256];
};

struct t_palette {
    uint8_t entryID;
    uint8_t valueY;
    uint8_t valueCr;
    uint8_t valueCb;
    uint8_t valueA;
};
struct t_PDS {
    uint8_t id;
    uint8_t versionNumber;
    t_palette palettes[255];
    uint8_t numberOfPalettes; //not in format, only used internally
};

struct t_ODS {
    uint16_t id;
    uint8_t  versionNumber;
    uint8_t  sequenceFlag;
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


t_header readHeader(uint8_t* buffer) {
    t_header header = {};

    header.header      = swapEndianness(*(uint16_t*)&buffer[0]);
    header.pts         = swapEndianness(*(uint32_t*)&buffer[2]);
    header.dts         = swapEndianness(*(uint32_t*)&buffer[6]);
    header.segmentType =                *(uint8_t*) &buffer[10];
    header.dataLength  = swapEndianness(*(uint16_t*)&buffer[11]);

    return header;
}

void writeHeader(t_header header, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0]))  = swapEndianness(header.header);
    *((uint32_t*)(&buffer[2]))  = swapEndianness(header.pts);
    *((uint32_t*)(&buffer[6]))  = swapEndianness(header.dts);
    *((uint8_t*) (&buffer[10])) =                header.segmentType;
    *((uint16_t*)(&buffer[11])) = swapEndianness(header.dataLength);
}


t_WDS readWDS(uint8_t* buffer) {
    t_WDS wds;

    wds.numberOfWindows = *(uint8_t*)&buffer[0];
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;

        wds.windows[i].id                 =                *(uint8_t*) &buffer[bufferStartIdx + 0];
        wds.windows[i].horizontalPosition = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 1]);
        wds.windows[i].verticalPosition   = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 3]);
        wds.windows[i].width              = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 5]);
        wds.windows[i].height             = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 7]);
    }

    return wds;
}

void writeWDS(t_WDS wds, uint8_t* buffer) {
    *((uint8_t*)(&buffer[0])) = wds.numberOfWindows;
    for (int i = 0; i < wds.numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;

        *((uint8_t*) (&buffer[bufferStartIdx + 0])) =                wds.windows[i].id;
        *((uint16_t*)(&buffer[bufferStartIdx + 1])) = swapEndianness(wds.windows[i].horizontalPosition);
        *((uint16_t*)(&buffer[bufferStartIdx + 3])) = swapEndianness(wds.windows[i].verticalPosition);
        *((uint16_t*)(&buffer[bufferStartIdx + 5])) = swapEndianness(wds.windows[i].width);
        *((uint16_t*)(&buffer[bufferStartIdx + 7])) = swapEndianness(wds.windows[i].height);
    }
}


t_PCS readPCS(uint8_t* buffer) {
    t_PCS pcs;

    pcs.width                      = swapEndianness(*(uint16_t*)&buffer[0]);
    pcs.height                     = swapEndianness(*(uint16_t*)&buffer[2]);
    pcs.frameRate                  =                *(uint8_t*) &buffer[4];
    pcs.compositionNumber          = swapEndianness(*(uint16_t*)&buffer[5]);
    pcs.compositionState           =                *(uint8_t*) &buffer[7];
    pcs.paletteUpdateFlag          =                *(uint8_t*) &buffer[8];
    pcs.paletteID                  =                *(uint8_t*) &buffer[9];
    pcs.numberOfCompositionObjects =                *(uint8_t*) &buffer[10];

    size_t bufferStartIdx = 11;
    for (int i = 0; i < pcs.numberOfCompositionObjects; i++) {
        pcs.compositionObjects[i].objectID                      = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 0]);
        pcs.compositionObjects[i].windowID                      =                *(uint8_t*) &buffer[bufferStartIdx + 2];
        pcs.compositionObjects[i].croppedAndForcedFlag          =                *(uint8_t*) &buffer[bufferStartIdx + 3];
        pcs.compositionObjects[i].horizontalPosition            = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 4]);
        pcs.compositionObjects[i].verticalPosition              = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 6]);
        if (pcs.compositionObjects[i].croppedAndForcedFlag & e_objectFlags::cropped) {
            pcs.compositionObjects[i].croppedHorizontalPosition = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 8]);
            pcs.compositionObjects[i].croppedVerticalPosition   = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 10]);
            pcs.compositionObjects[i].croppedWidth              = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 12]);
            pcs.compositionObjects[i].croppedHeight             = swapEndianness(*(uint16_t*)&buffer[bufferStartIdx + 14]);
            bufferStartIdx += 8;
        }
        bufferStartIdx += 8;
    }

    return pcs;
}

void writePCS(t_PCS pcs, uint8_t* buffer) {
    *((uint16_t*)(&buffer[0]))  = swapEndianness(pcs.width);
    *((uint16_t*)(&buffer[2]))  = swapEndianness(pcs.height);
    *((uint8_t*) (&buffer[4]))  =                pcs.frameRate;
    *((uint16_t*)(&buffer[5]))  = swapEndianness(pcs.compositionNumber);
    *((uint8_t*) (&buffer[7]))  =                pcs.compositionState;
    *((uint8_t*) (&buffer[8]))  =                pcs.paletteUpdateFlag;
    *((uint8_t*) (&buffer[9]))  =                pcs.paletteID;
    *((uint8_t*) (&buffer[10])) =                pcs.numberOfCompositionObjects;

    size_t bufferStartIdx = 11;
    for (int i = 0; i < pcs.numberOfCompositionObjects; i++) {
        *((uint16_t*)(&buffer[bufferStartIdx + 0]))  = swapEndianness(pcs.compositionObjects[i].objectID);
        *((uint8_t*) (&buffer[bufferStartIdx + 2]))  =                pcs.compositionObjects[i].windowID;
        *((uint8_t*) (&buffer[bufferStartIdx + 3]))  =                pcs.compositionObjects[i].croppedAndForcedFlag;
        *((uint16_t*)(&buffer[bufferStartIdx + 4]))  = swapEndianness(pcs.compositionObjects[i].horizontalPosition);
        *((uint16_t*)(&buffer[bufferStartIdx + 6]))  = swapEndianness(pcs.compositionObjects[i].verticalPosition);
        if (pcs.compositionObjects[i].croppedAndForcedFlag & e_objectFlags::cropped) {
            *((uint16_t*)(&buffer[bufferStartIdx + 8]))  = swapEndianness(pcs.compositionObjects[i].croppedHorizontalPosition);
            *((uint16_t*)(&buffer[bufferStartIdx + 10])) = swapEndianness(pcs.compositionObjects[i].croppedVerticalPosition);
            *((uint16_t*)(&buffer[bufferStartIdx + 12])) = swapEndianness(pcs.compositionObjects[i].croppedWidth);
            *((uint16_t*)(&buffer[bufferStartIdx + 14])) = swapEndianness(pcs.compositionObjects[i].croppedHeight);
            bufferStartIdx += 8;
        }
        bufferStartIdx += 8;
    }
}


t_PDS readPDS(uint8_t* buffer, size_t segment_size) {
    t_PDS pds;

    pds.id            = *(uint8_t*)&buffer[0];
    pds.versionNumber = *(uint8_t*)&buffer[1];

    pds.numberOfPalettes = (segment_size - 2) / 5;

    for (int i = 0; i < pds.numberOfPalettes; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        pds.palettes[i].entryID = *(uint8_t*)&buffer[bufferStartIdx + 0];
        pds.palettes[i].valueY  = *(uint8_t*)&buffer[bufferStartIdx + 1];
        pds.palettes[i].valueCb = *(uint8_t*)&buffer[bufferStartIdx + 2];
        pds.palettes[i].valueCr = *(uint8_t*)&buffer[bufferStartIdx + 3];
        pds.palettes[i].valueA  = *(uint8_t*)&buffer[bufferStartIdx + 4];
    }

    return pds;
}

void writePDS(t_PDS pds, uint8_t* buffer) {
    *((uint8_t*)(&buffer[0])) = pds.id;
    *((uint8_t*)(&buffer[1])) = pds.versionNumber;

    for (int i = 0; i < pds.numberOfPalettes; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        *((uint8_t*)(&buffer[bufferStartIdx + 0])) = pds.palettes[i].entryID;
        *((uint8_t*)(&buffer[bufferStartIdx + 1])) = pds.palettes[i].valueY;
        *((uint8_t*)(&buffer[bufferStartIdx + 2])) = pds.palettes[i].valueCb;
        *((uint8_t*)(&buffer[bufferStartIdx + 3])) = pds.palettes[i].valueCr;
        *((uint8_t*)(&buffer[bufferStartIdx + 4])) = pds.palettes[i].valueA;
    }
}


t_ODS readODS(uint8_t* buffer) {
    t_ODS ods;

    ods.id            = swapEndianness(*(uint16_t*)&buffer[0]);
    ods.versionNumber =                *(uint8_t*) &buffer[2];
    ods.sequenceFlag  =                *(uint8_t*) &buffer[3];
    ods.dataLength    = swapEndianness(*(uint32_t*)&buffer[3]) & 0xffffff;
    ods.width         = swapEndianness(*(uint16_t*)&buffer[7]);
    ods.height        = swapEndianness(*(uint16_t*)&buffer[9]);

    return ods;
}
