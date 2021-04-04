#include <iostream>
#include <string>
#include <fstream>
#include <io.h>
#include <windows.h>
#include <direct.h>
#include <png.h>
#include <zlib.h>
#include <cmath>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <atomic>
#include <future>
#include <functional>

#include "ThreadPool.h"

using namespace std;

/* got this code from libpng example */
#define PNG_BYTES_TO_CHECK 4
int check_if_png(const char* file_name) {
    FILE* fp;
    char buf[PNG_BYTES_TO_CHECK];

    /* Open the prospective PNG file. */
    if ((fp = fopen(file_name, "rb")) == NULL)
        return 0;

    /* Read in some of the signature bytes. */
    if (fread(buf, 1, PNG_BYTES_TO_CHECK, fp) != PNG_BYTES_TO_CHECK)
        return 0;

    /* Compare the first PNG_BYTES_TO_CHECK bytes of the signature.
     * Return nonzero (true) if they match.
     */
    return(!png_sig_cmp((png_const_bytep)buf, 0, PNG_BYTES_TO_CHECK));
    fclose(fp);
}

#define scale_x 0.8;
#define scale_y 0.8;
#define scale   0.8;
void process_row(unsigned char** data_ori, unsigned char** data_out, unsigned char** data_out_x, unsigned char** data_out_y, int y, int width) {
    for (int x = 1; x < width - 1; x++) {
        int out_x = abs(data_ori[y - 1][x - 1] + data_ori[y][x - 1] * 2 + data_ori[y + 1][x - 1]
            - data_ori[y - 1][x + 1] - data_ori[y][x + 1] * 2 - data_ori[y + 1][x + 1]);
        int out_y = abs(data_ori[y - 1][x - 1] + data_ori[y - 1][x] * 2 + data_ori[y - 1][x + 1]
            - data_ori[y + 1][x - 1] - data_ori[y + 1][x] * 2 - data_ori[y + 1][x + 1]);
        int out = (out_x + out_y) / 2;
        out_x *= scale_x;
        out_y *= scale_y;
        out *= scale;
        data_out_x[y][x] = out_x > 255 ? 255 : out_x;
        data_out_y[y][x] = out_y > 255 ? 255 : out_y;
        data_out[y][x] = out > 255 ? 255 : out;
        //cout << "result at [" << x << y << "]" << " is " << (int)data_out[y][x] << endl;
        if (data_out[y][x] >= 255)
            data_out[y][x] = 255;
    }
}

int process(string pathIn, string pathOut, string fileName) {
    string p1, p2; // s_str is valid until the string is modified
    string file_name_out, file_name_out_x, file_name_out_y;
    file_name_out.assign(fileName).insert(fileName.size() - 4, "_o");
    file_name_out_x.assign(fileName).insert(fileName.size() - 4, "_ox");
    file_name_out_y.assign(fileName).insert(fileName.size() - 4, "_oy");
    const char* fromFile = p1.assign(pathIn).append("//").append(fileName).c_str();
    const char* toFile = p2.assign(pathOut).append("//").append(file_name_out).c_str();

    // read png
    png_structp png_ptr = nullptr;
    png_infop info_ptr = nullptr;

    FILE* fp = fopen(fromFile, "rb");


    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if (!png_ptr)
        return (ERROR);
    info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        return (ERROR);
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        fclose(fp);
        return ERROR;
    }

    png_init_io(png_ptr, fp);
    unsigned int width, height;
    int bit_depth, color_type, interlace_type, compression_type, filter_method;
    
    png_read_info(png_ptr, info_ptr);
    png_get_IHDR(png_ptr, info_ptr, &width, &height,
        &bit_depth, &color_type, &interlace_type,
        &compression_type, &filter_method);

    // transfer the image into gray image
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        cout << "not ready for paletted images" << endl;
        png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
        if (fp)
            fclose(fp);
        return 1;
    }
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_RGB_ALPHA)
        png_set_rgb_to_gray(png_ptr, 1, 0.211, 0.715);
    if (color_type & PNG_COLOR_MASK_ALPHA)
        png_set_strip_alpha(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr, info_ptr, &width, &height,
        &bit_depth, &color_type, &interlace_type,
        &compression_type, &filter_method);

    png_bytepp row_pointers = new png_bytep[height];
    for (int i = 0; i < height; i++) {
        row_pointers[i] = new unsigned char[width];
        memset(row_pointers[i], 0, sizeof(char) * width);
    }
    png_read_image(png_ptr, row_pointers);

    png_read_end(png_ptr, NULL);

    // save data
    unsigned char** data_ori = new unsigned char* [height];
    for (int i = 0; i < height; i++) {
        data_ori[i] = new unsigned char[width];
        memset(data_ori[i], 0, sizeof(char) * width);
    }
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            data_ori[y][x] = row_pointers[y][x];
            //cout << (int)data_ori[y][x] << endl;
        }
    }
    for (int i = 0; i < height; i++) {
        delete[] row_pointers[i];
        row_pointers[i] = nullptr;
    }
    delete[] row_pointers;
    row_pointers = nullptr;

    // Sobel
    unsigned char** data_out_x = new unsigned char* [height];
    unsigned char** data_out_y = new unsigned char* [height];
    unsigned char** data_out = new unsigned char* [height];
    for (int i = 0; i < height; i++) {
        data_out[i] = new unsigned char[width];
        memset(data_out[i], 0, sizeof(char) * width);
        data_out_x[i] = new unsigned char[width];
        memset(data_out_x[i], 0, sizeof(char) * width);
        data_out_y[i] = new unsigned char[width];
        memset(data_out_y[i], 0, sizeof(char) * width);
    }

    ThreadPool pool(5);
    for (int y = 1; y < height - 1; y++) {
        pool.enqueue([data_ori, data_out, data_out_x, data_out_y, y, width] {
            process_row(data_ori, data_out, data_out_x, data_out_y, y, width);
            });
    }

    // write to file
    FILE* fp2 = nullptr;
    fopen_s(&fp2, toFile, "wb");

    png_structp png_w_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    png_infop info_w_ptr = png_create_info_struct(png_w_ptr);
    setjmp(png_jmpbuf(png_w_ptr));

    png_init_io(png_w_ptr, fp2);
    png_set_IHDR(png_w_ptr, info_w_ptr, width, height,
        bit_depth, color_type,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_w_ptr, info_w_ptr);
    png_bytep row_w_ptr = new png_byte[width * sizeof(png_byte)];
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            row_w_ptr[x] = data_out[y][x];
        }
        png_write_row(png_w_ptr, row_w_ptr);
    }
    delete[] row_w_ptr;
    row_w_ptr = nullptr;

    for (int i = 0; i < height; i++) {
        delete[] data_ori[i];
        data_ori[i] = nullptr;
        delete[] data_out[i];
        data_out[i] = nullptr;
        delete[] data_out_x[i];
        data_out_x[i] = nullptr;
        delete[] data_out_y[i];
        data_out_y[i] = nullptr;
    }
    delete[] data_ori;
    data_ori = nullptr;
    delete[] data_out;
    data_out = nullptr;
    delete[] data_out_x;
    data_out_x = nullptr;
    delete[] data_out_y;
    data_out_y = nullptr;

    png_write_end(png_w_ptr, nullptr);
    if (fp2)
        fclose(fp2);

    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
    if (fp)
        fclose(fp);
    return 0;
}

int main() {
    string path, pathO, format = ".png";
    cout << "Input Path: ";
    cin >> path;
    cout << "Output Path: ";
    cin >> pathO;
    // if pathO exits.
    if (_access(pathO.c_str(), 0) == -1 || GetFileAttributesA(pathO.c_str()) != FILE_ATTRIBUTE_DIRECTORY) {
        if (_mkdir(pathO.c_str()) == -1) {
            exit(1);
        }
    }
    long hFile = 0;
    struct _finddata_t fileinfo;
    string p;
    if ((hFile = _findfirst(p.assign(path).append("\\*" + format).c_str(), &fileinfo)) != -1)
    {
        do
        {
            p.assign(path).append("\\").append(fileinfo.name);
            if (check_if_png(p.c_str())) {
                cout << p << " is a png file" << endl;
                process(path, pathO, p.assign(fileinfo.name));
            }
            else {
                cout << p << " is not a png file" << endl;
            }

        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
}