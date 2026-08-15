#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// --------------------------- 数据结构 ---------------------------
struct miptex_t {
    char name[16];
    unsigned width, height;
    unsigned offsets[4]; // 四个 mip 层
};

struct lumpinfo_t {
    int filepos;
    int disksize;
    int size;
    char type;
    char compression;
    char pad1, pad2;
    char name[16];
};

struct wadinfo_t {
    char identification[4]; // "WAD3"
    int numlumps;
    int infotableofs;
};

// --------------------------- 全局调色板 ---------------------------
unsigned char palLogo[768];        // 256*3 调色板
float linearpalette[256][3];
float d_red, d_green, d_blue;
int colors_used;
int color_used[256];
float maxdistortion;
unsigned char pixdata[256];

// --------------------------- 辅助函数 ---------------------------
unsigned char AveragePixels(int count) {
    int sum = 0;
    for (int i = 0; i < count; ++i)
        sum += pixdata[i];
    return (unsigned char)(sum / count);
}

// --------------------------- 生成 MIP ---------------------------
int GrabMip(const unsigned char* pixels, int w, int h, unsigned char* lump_p, const char* lumpname,
            uint8_t r, uint8_t g, uint8_t b, int* outWidth, int* outHeight)
{
    *outWidth = w;
    *outHeight = h;

    if ((w & 15) || (h & 15))
        return 0; // 尺寸必须是16的倍数

    miptex_t* qtex = (miptex_t*)lump_p;
    qtex->width = w;
    qtex->height = h;
    strncpy(qtex->name, lumpname, sizeof(qtex->name));
    lump_p += sizeof(miptex_t);

    // Level 0
    memcpy(lump_p, pixels, w*h);
    lump_p += w*h;

    // 线性调色板
    for (int i = 0; i < 256; i++)
        for (int j = 0; j < 3; j++)
            linearpalette[i][j] = palLogo[i*3 + j] / 255.0f;

    maxdistortion = 0;
    colors_used = 256;
    for (int i = 0; i < 256; i++) color_used[i] = 1;

    // 生成 mip1~3
    for (int miplevel = 1; miplevel < 4; ++miplevel) {
        d_red = d_green = d_blue = 0;
        qtex->offsets[miplevel] = (unsigned)(lump_p - (unsigned char*)qtex);
        int step = 1 << miplevel;
        for (int y = 0; y < h; y += step) {
            for (int x = 0; x < w; x += step) {
                int count = 0;
                for (int yy = 0; yy < step; ++yy)
                    for (int xx = 0; xx < step; ++xx)
                        pixdata[count++] = pixels[(y + yy)*w + x + xx];
                *lump_p++ = AveragePixels(count);
            }
        }
    }

    // 写入 palette 16bit
    *(uint16_t*)lump_p = 256;
    lump_p += sizeof(uint16_t);
    memcpy(lump_p, palLogo, 768);
    lump_p += 768;

    // 写入 RGB 颜色
    *lump_p++ = r;
    *lump_p++ = g;
    *lump_p++ = b;

    return lump_p - (unsigned char*)qtex;
}

// --------------------------- 输出 WAD ---------------------------
void UpdateLogoWAD(const unsigned char* pixels, int width, int height, const char* name,
                   uint8_t r, uint8_t g, uint8_t b, const char* outFilename)
{
    if (!pixels || !name || !name[0])
        return;

    unsigned char buf[16384]; // 临时缓冲区
    int w, h;

    int length = GrabMip(pixels, width, height, buf, name, r, g, b, &w, &h);
    if (length == 0)
        return;

    // 校验尺寸
    if (!(w == h && (w == 16 || w == 32 || w == 64)))
        return;

    while (length & 3) length++; // 4字节对齐

    // WAD header
    wadinfo_t header;
    header.identification[0] = 'W';
    header.identification[1] = 'A';
    header.identification[2] = 'D';
    header.identification[3] = '3';
    header.numlumps = 1;
    header.infotableofs = sizeof(wadinfo_t) + length;

    lumpinfo_t info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, name, sizeof(info.name));
    info.filepos = sizeof(wadinfo_t);
    info.size = info.disksize = length;
    info.type = 64; // TYP_LUMPY
    info.compression = 0;

    FILE* fp = fopen(outFilename, "wb");
    if (!fp) return;

    fwrite(&header, sizeof(header), 1, fp);
    fwrite(buf, length, 1, fp);
    fwrite(&info, sizeof(info), 1, fp);
    fclose(fp);
}