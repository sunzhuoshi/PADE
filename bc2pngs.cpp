/*
 * Copyright 2002-2010 Guillaume Cottenceau.
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include <iostream>
#include <list>
#include <string>
#include <sstream>

#define PNG_DEBUG 3
#include <png.h>

#include "common.h"

using namespace std;

#define NAME_LENGTH 24

enum TEXTURE_TYPE {
    TEXTURE_TYPE_RGBA8888, // OK
    TEXTURE_TYPE_RGB565 = 2, // OK
    TEXTURE_TYPE_RGBA4444 = 3, // OK
    TEXTURE_TYPE_RGB5A1 = 4, // OK
    TEXTURE_TYPE_I8 = 8,    // OK?
    TEXTURE_TYPE_A8 = 9,    // OK?
    TEXTURE_TYPE_PVRTC4 = 11, // OK?
    TEXTURE_TYPE_PVRTC2 = 12, // OK?
    TEXTURE_TYPE_JPG = 13 // OK
};

int file_count;

png_byte color_type = 6;
png_byte bit_depth = 8;

png_structp png_ptr;
png_infop info_ptr;
int number_of_passes;
png_bytep * row_pointers;
unsigned char * raw_data;
size_t raw_data_length;

class TextureInfo {
public:
    int offset;
private:
    unsigned short width;
    unsigned short height;
public:
    char name [NAME_LENGTH];
    unsigned short getWidth() const {
        return width & 0x0FFF;
    }
    unsigned short getHeight() const {
        return height;
    }
    int getType() const {
        return (width & 0xF000) >> 12;
    }
    string toString() const {
        ostringstream strBuf;
        strBuf << "name: " << name <<
            ", offset: " << offset <<
            ", width: " << getWidth() <<
            ", height: " << getHeight() <<
            ", type: " << getType();
        return strBuf.str();
    }
};

typedef list<TextureInfo> TextureInfoList;

TextureInfoList TextureInfos;

void write_jpg_file(const TextureInfo &info) {
    FILE *fp = fopen(info.name, "wb");
    if (!fp) {
        abort_("[write_jpg_file] File %s cound not be opened for writing", info.name);
    }
    cout << "Writing " << info.name << " ..." << endl;
    if (raw_data_length != fwrite(raw_data, 1, raw_data_length, fp)) {
        abort_("[write_jpg_file] Failed to write file %s", info.name);
    }
    fclose(fp);
}

void write_pvr_file(const TextureInfo &info) {
    string file_name = info.name;
    const char *PVR_EXT = ".PVR";
    file_name.replace(file_name.find_first_of('.'), sizeof(PVR_EXT), PVR_EXT);
    FILE *fp = fopen(file_name.c_str(), "wb");
    if (!fp) {
        abort_("[write_pvr_file] File %s cound not be opened for writing", file_name.c_str());
    }
    cout << "Writing " << file_name << " ..." << endl;
    if (raw_data_length != fwrite(raw_data, 1, raw_data_length, fp)) {
        abort_("[write_pvr_file] Failed to write file %s", info.name);
    }
    fclose(fp);
}

void write_png_file(const TextureInfo &info)
{
    /* create file */
    FILE *fp = fopen(info.name, "wb");
    if (!fp) {
        abort_("[write_png_file] File %s could not be opened for writing", info.name);
    }
    printf("Writing %s ...\n", info.name);
    /* initialize stuff */
    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    
    if (!png_ptr) {
        abort_("[write_png_file] png_create_write_struct failed");
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        abort_("[write_png_file] png_create_info_struct failed");
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        abort_("[write_png_file] Error during init_io");
    }
    png_init_io(png_ptr, fp);
    /* write header */
    if (setjmp(png_jmpbuf(png_ptr))) {
        abort_("[write_png_file] Error during writing header");
    }
    png_set_IHDR(png_ptr, info_ptr, info.getWidth(), info.getHeight(),
                 bit_depth, color_type, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png_ptr, info_ptr);
    /* write bytes */
    if (setjmp(png_jmpbuf(png_ptr))) {
        abort_("[write_png_file] Error during writing bytes");
    }
    png_write_image(png_ptr, row_pointers);
    /* end write */
    if (setjmp(png_jmpbuf(png_ptr))) {
        abort_("[write_png_file] Error during end of write");
    }
    png_write_end(png_ptr, NULL);
    /* cleanup heap allocation */
    for (int y=0; y<info.getHeight(); y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    row_pointers = NULL;
    fclose(fp);
}

void process_tex1(char *file_name, long file_size, FILE *fp)
{
    fread(&file_count, 1, 4, fp);
    unsigned char unknown8[8];
    fread(unknown8, 1, 8, fp);
    for (int i=0; i<file_count; ++i) {
        TextureInfo info;
        fread(&info, 1, sizeof(info), fp);
        cout << info.toString() << endl;
        TextureInfos.push_back(info);
    }
    for (TextureInfoList::const_iterator it=TextureInfos.begin(); it!=TextureInfos.end(); ++it) {
        if (fseek(fp, it->offset, SEEK_SET)) {
            abort_("[process_bc_file] Failed to read data from file: %s", file_name);
        }
        if (raw_data) {
            free(raw_data);
            raw_data = NULL;
        }
        if (TEXTURE_TYPE_JPG == it->getType()) {
            TextureInfoList::const_iterator nextIt = it;
            nextIt ++;
            if (nextIt != TextureInfos.end()) {
                raw_data_length = nextIt->offset - it->offset;
            }
            else {
                raw_data_length = file_size - it->offset;
            }
            raw_data = (unsigned char *)malloc(raw_data_length);
            if (raw_data_length != fread(raw_data, 1, raw_data_length, fp)) {
                abort_("[process_bc_file] Read data error from file: %s", file_name);
            }
            write_jpg_file(*it);
        }
        else if (TEXTURE_TYPE_PVRTC4 == it->getType()) {
            raw_data_length = it->getWidth() * it->getHeight() / 2;
            raw_data = (unsigned char *)malloc(raw_data_length);
            if (raw_data_length != fread(raw_data, 1, raw_data_length, fp)) {
                abort_("[process_bc_file] Read data error from file: %s", file_name);
            }
            write_pvr_file(*it);
        }
        else if (TEXTURE_TYPE_PVRTC2 == it->getType()) {
            raw_data_length = it->getWidth() * it->getHeight() * 2 / 8 + 64;
            raw_data = (unsigned char *)malloc(raw_data_length);
            if (raw_data_length != fread(raw_data, 1, raw_data_length, fp)) {
                abort_("[process_bc_file] Read data error from file: %s", file_name);
            }
            write_pvr_file(*it);
        }
        else {
            if (row_pointers) {
                free(row_pointers);
                row_pointers = NULL;
            }
            row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * it->getHeight());
            for (int y=0; y<it->getHeight(); y++) {
                unsigned char *row_data = NULL;
                row_pointers[y] = (png_byte*) malloc(sizeof(png_byte) * it->getWidth() * 4);
                switch (it->getType()) {
                    case TEXTURE_TYPE_RGBA8888:
                        fread(row_pointers[y], 1, it->getWidth() * 4, fp);
                        break;
                    case TEXTURE_TYPE_RGBA4444:
                        row_data = (unsigned char *) malloc(it->getWidth() * 2);
                        fread(row_data, 1, it->getWidth() * 2, fp);
                        for (int i=0; i<sizeof(png_byte) * it->getWidth(); i++) {
                            unsigned short bits16 = *(unsigned short *)&row_data[i*2];
                            row_pointers[y][i*4  ] = ((bits16 >> 12) & 0x0F) << 4;
                            row_pointers[y][i*4+1] = ((bits16 >> 8 ) & 0x0F) << 4;
                            row_pointers[y][i*4+2] = ((bits16 >> 4 ) & 0x0F) << 4;
                            row_pointers[y][i*4+3] = ((bits16 >> 0 ) & 0x0F) << 4;
                        }
                        break;
                    case TEXTURE_TYPE_RGB565:
                        row_data = (unsigned char *)malloc(it->getWidth() * 2);
                        fread(row_data, 1, it->getWidth() * 2, fp);
                        for (int i=0; i<sizeof(png_byte) * it->getWidth(); i++) {
                            unsigned short bits16 = *(unsigned short *)&row_data[i*2];
                            row_pointers[y][i*4  ] = ((bits16 >> 11) & 0x1F) << 3;
                            row_pointers[y][i*4+1] = ((bits16 >> 5 ) & 0x3F) << 2;
                            row_pointers[y][i*4+2] = ((bits16 >> 0 ) & 0x1F) << 3;
                            row_pointers[y][i*4+3] = 255;
                        }
                        break;
                    case TEXTURE_TYPE_RGB5A1:
                        row_data = (unsigned char *) malloc(it->getWidth() * 2);
                        fread(row_data, 1, it->getWidth() * 2, fp);
                        for (int i=0; i<sizeof(png_byte) * it->getWidth(); i++) {
                            unsigned short bits16 = *(unsigned short *)&row_data[i*2];
                            row_pointers[y][i*4  ] = ((bits16 >> 11) & 0x1F) << 3;
                            row_pointers[y][i*4+1] = ((bits16 >> 6 ) & 0x1F) << 3;
                            row_pointers[y][i*4+2] = ((bits16 >> 1 ) & 0x1F) << 3;
                            row_pointers[y][i*4+3] = ((bits16 >> 0 ) & 0x01) << 7;
                        }
                        break;
                    case TEXTURE_TYPE_A8:
                        row_data = (unsigned char *)malloc(it->getWidth());
                        fread(row_data, 1, it->getWidth(), fp);
                        for (int i=0; i<sizeof(png_byte) * it->getWidth(); i++) {
                            row_pointers[y][i*4] = 255;
                            row_pointers[y][i*4+1] = 255;
                            row_pointers[y][i*4+2] = 255;
                            row_pointers[y][i*4+3] = row_data[i];
                        }
                        break;
                    case TEXTURE_TYPE_I8:
                        row_data = (unsigned char *)malloc(it->getWidth());
                        fread(row_data, 1, it->getWidth(), fp);
                        for (int i=0; i<sizeof(png_byte) * it->getWidth(); i++) {
                            row_pointers[y][i*4] = row_data[i];
                            row_pointers[y][i*4+1] = row_data[i];
                            row_pointers[y][i*4+2] = row_data[i];
                            row_pointers[y][i*4+3] = 255;
                        }
                        break;
                    default:
                        abort_("[process_bc_file] Unknown texture: %s", it->toString().c_str());
                        break;
                }
                if (row_data) {
                    free(row_data);
                    row_data = NULL;
                }
            }
            write_png_file(*it);
        }
    }
}

void process_tex2(char *file_name, long file_size, FILE *fp)
{
    process_tex1(file_name, file_size, fp);
}

void process_bc_file(char *file_name) 
{
	unsigned char fcc[4];
    cout << "Processing " << file_name << " ..." << endl;
    FILE *fp = fopen(file_name, "rb");
    if (!fp) {
        abort_("[process_bc_file] File %s cound not be opened for reading", file_name);
    }
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);    
    fseek(fp, 0, SEEK_SET);
    
	fread(fcc, 1, 4, fp);
    if (0 == memcmp(&fcc, FCC_TEX1, sizeof(fcc))) {
        process_tex1(file_name, file_size, fp);
    }
    else if (0 == memcmp(&fcc, FCC_TEX2, sizeof(fcc))) {
        process_tex2(file_name, file_size, fp);
    }
    else {
        abort_("[process_bc_file] File %s is not recognized as a BC file, unknown FCC: %c%c%c%c", file_name, fcc[0], fcc[1], fcc[2], fcc[3]);
    }
    cout << "Done" << endl;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        abort_("Usage: program_name <file1>...");
    }
    for (int i=1; i<argc; ++i) {
        process_bc_file(argv[i]);
    }
    return 0;
}
