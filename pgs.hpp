//Documentation: http://blog.thescorpius.com/index.php/2017/07/15/presentation-graphic-stream-sup-files-bluray-subtitle-format/

struct t_header {
    uint16_t header;
    uint32_t pts;
    uint32_t dts;
    uint8_t  segmentType;
    uint16_t dataLength;

    static t_header read(uint8_t*);
    void write(uint8_t*);
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

    static t_WDS read(uint8_t*);
    void write(uint8_t*);
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

    static t_PCS read(uint8_t*);
    void write(uint8_t*);
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

    static t_PDS read(uint8_t*, size_t);
    void write(uint8_t*);
};


struct t_ODS {
    uint16_t id;
    uint8_t  versionNumber;
    uint8_t  sequenceFlag;
    uint32_t dataLength;
    uint16_t width;
    uint16_t height;
    //uint8_t* data;

    static t_ODS read(uint8_t*);
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

t_header t_header::read(uint8_t* buffer) {
    t_header header = {};

    header.header      = swapEndianness(*(uint16_t*)&buffer[0]);
    header.pts         = swapEndianness(*(uint32_t*)&buffer[2]);
    header.dts         = swapEndianness(*(uint32_t*)&buffer[6]);
    header.segmentType =                *(uint8_t*) &buffer[10];
    header.dataLength  = swapEndianness(*(uint16_t*)&buffer[11]);

    return header;
}

void t_header::write(uint8_t* buffer) {
    *((uint16_t*)(&buffer[0]))  = swapEndianness(header);
    *((uint32_t*)(&buffer[2]))  = swapEndianness(pts);
    *((uint32_t*)(&buffer[6]))  = swapEndianness(dts);
    *((uint8_t*) (&buffer[10])) =                segmentType;
    *((uint16_t*)(&buffer[11])) = swapEndianness(dataLength);
}


t_WDS t_WDS::read(uint8_t* buffer) {
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

void t_WDS::write(uint8_t* buffer) {
    *((uint8_t*)(&buffer[0])) = numberOfWindows;
    for (int i = 0; i < numberOfWindows; i++) {
        size_t bufferStartIdx = 1 + (size_t)i * 9;

        *((uint8_t*) (&buffer[bufferStartIdx + 0])) =                windows[i].id;
        *((uint16_t*)(&buffer[bufferStartIdx + 1])) = swapEndianness(windows[i].horizontalPosition);
        *((uint16_t*)(&buffer[bufferStartIdx + 3])) = swapEndianness(windows[i].verticalPosition);
        *((uint16_t*)(&buffer[bufferStartIdx + 5])) = swapEndianness(windows[i].width);
        *((uint16_t*)(&buffer[bufferStartIdx + 7])) = swapEndianness(windows[i].height);
    }
}


t_PCS t_PCS::read(uint8_t* buffer) {
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

void t_PCS::write(uint8_t* buffer) {
    *((uint16_t*)(&buffer[0]))  = swapEndianness(width);
    *((uint16_t*)(&buffer[2]))  = swapEndianness(height);
    *((uint8_t*) (&buffer[4]))  =                frameRate;
    *((uint16_t*)(&buffer[5]))  = swapEndianness(compositionNumber);
    *((uint8_t*) (&buffer[7]))  =                compositionState;
    *((uint8_t*) (&buffer[8]))  =                paletteUpdateFlag;
    *((uint8_t*) (&buffer[9]))  =                paletteID;
    *((uint8_t*) (&buffer[10])) =                numberOfCompositionObjects;

    size_t bufferStartIdx = 11;
    for (int i = 0; i < numberOfCompositionObjects; i++) {
        *((uint16_t*)(&buffer[bufferStartIdx + 0]))  = swapEndianness(compositionObjects[i].objectID);
        *((uint8_t*) (&buffer[bufferStartIdx + 2]))  =                compositionObjects[i].windowID;
        *((uint8_t*) (&buffer[bufferStartIdx + 3]))  =                compositionObjects[i].croppedAndForcedFlag;
        *((uint16_t*)(&buffer[bufferStartIdx + 4]))  = swapEndianness(compositionObjects[i].horizontalPosition);
        *((uint16_t*)(&buffer[bufferStartIdx + 6]))  = swapEndianness(compositionObjects[i].verticalPosition);
        if (compositionObjects[i].croppedAndForcedFlag & e_objectFlags::cropped) {
            *((uint16_t*)(&buffer[bufferStartIdx + 8]))  = swapEndianness(compositionObjects[i].croppedHorizontalPosition);
            *((uint16_t*)(&buffer[bufferStartIdx + 10])) = swapEndianness(compositionObjects[i].croppedVerticalPosition);
            *((uint16_t*)(&buffer[bufferStartIdx + 12])) = swapEndianness(compositionObjects[i].croppedWidth);
            *((uint16_t*)(&buffer[bufferStartIdx + 14])) = swapEndianness(compositionObjects[i].croppedHeight);
            bufferStartIdx += 8;
        }
        bufferStartIdx += 8;
    }
}


t_PDS t_PDS::read(uint8_t* buffer, size_t segment_size) {
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

void t_PDS::write(uint8_t* buffer) {
    *((uint8_t*)(&buffer[0])) = id;
    *((uint8_t*)(&buffer[1])) = versionNumber;

    for (int i = 0; i < numberOfPalettes; i++) {
        size_t bufferStartIdx = 2 + (size_t)i * 5;

        *((uint8_t*)(&buffer[bufferStartIdx + 0])) = palettes[i].entryID;
        *((uint8_t*)(&buffer[bufferStartIdx + 1])) = palettes[i].valueY;
        *((uint8_t*)(&buffer[bufferStartIdx + 2])) = palettes[i].valueCb;
        *((uint8_t*)(&buffer[bufferStartIdx + 3])) = palettes[i].valueCr;
        *((uint8_t*)(&buffer[bufferStartIdx + 4])) = palettes[i].valueA;
    }
}


t_ODS t_ODS::read(uint8_t* buffer) {
    t_ODS ods;

    ods.id            = swapEndianness(*(uint16_t*)&buffer[0]);
    ods.versionNumber =                *(uint8_t*) &buffer[2];
    ods.sequenceFlag  =                *(uint8_t*) &buffer[3];
    ods.dataLength    = swapEndianness(*(uint32_t*)&buffer[3]) & 0xffffff;
    ods.width         = swapEndianness(*(uint16_t*)&buffer[7]);
    ods.height        = swapEndianness(*(uint16_t*)&buffer[9]);

    return ods;
}
