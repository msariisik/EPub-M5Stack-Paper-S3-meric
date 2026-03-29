#include "models/sd_image_loader.hpp"

#if !EPUB_LINUX_BUILD
#include <JPEGDEC.h>
#endif

#include "alloc.hpp"
#include <cstdio>
#include <cstring>

#if !EPUB_LINUX_BUILD

struct JpegDecCtx {
    Image::ImageData * image_data;
};

static int JPEGDraw(JPEGDRAW *pDraw) {
    JpegDecCtx * ctx = (JpegDecCtx *)pDraw->pUser;
    if (ctx == nullptr || ctx->image_data == nullptr || ctx->image_data->bitmap == nullptr) {
        return 0;
    }

    const uint8_t * src = (const uint8_t *)pDraw->pPixels;
    if (src == nullptr) {
        return 0;
    }

    const uint16_t out_w = ctx->image_data->dim.width;
    const uint16_t out_h = ctx->image_data->dim.height;

    if ((pDraw->x < 0) || (pDraw->y < 0)) {
        return 0;
    }
    if ((uint32_t)pDraw->x >= out_w || (uint32_t)pDraw->y >= out_h) {
        return 1;
    }

    const int copy_w = (pDraw->iWidthUsed > 0) ? pDraw->iWidthUsed : pDraw->iWidth;
    const int max_w = (int)out_w - pDraw->x;
    const int w = (copy_w < max_w) ? copy_w : max_w;

    for (int yy = 0; yy < pDraw->iHeight; yy++) {
        const int dst_y = pDraw->y + yy;
        if (dst_y >= (int)out_h) break;
        uint8_t * dst = ctx->image_data->bitmap + (dst_y * out_w + pDraw->x);
        memcpy(dst, src + (yy * pDraw->iWidth), w);
    }

    return 1;
}

static int32_t sd_read(JPEGFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    return fread(pBuf, 1, iLen, (FILE *)pFile->fHandle);
}

static int32_t sd_seek(JPEGFILE *pFile, int32_t iPosition) {
    return fseek((FILE *)pFile->fHandle, iPosition, SEEK_SET);
}

static void * sd_open(const char *szFilename, int32_t *pFileSize) {
    FILE * f = fopen(szFilename, "rb");
    if (f) {
        fseek(f, 0, SEEK_END);
        *pFileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
    }
    return (void *)f;
}

static void sd_close(void *pHandle) {
    if (pHandle) {
        fclose((FILE *)pHandle);
    }
}

bool SDImageLoader::load_jpeg(const char * filename, Image::ImageData & image_data, Dim max_dim) {
    JPEGDEC jpeg;
    
    if (!jpeg.open(filename, sd_open, sd_close, sd_read, sd_seek, JPEGDraw)) {
        LOG_E("Unable to open JPEG: %s", filename);
        return false;
    }

    const uint16_t orig_w = (uint16_t)jpeg.getWidth();
    const uint16_t orig_h = (uint16_t)jpeg.getHeight();

    uint8_t scale = 0;
    while (scale < 3 && ((orig_w >> scale) > max_dim.width || (orig_h >> scale) > max_dim.height)) {
        scale++;
    }

    const uint16_t out_w = (uint16_t)(orig_w >> scale);
    const uint16_t out_h = (uint16_t)(orig_h >> scale);

    image_data.dim = Dim(out_w, out_h);
    image_data.bitmap = (uint8_t *)allocate(out_w * out_h);

    if (image_data.bitmap == nullptr) {
        jpeg.close();
        return false;
    }

    jpeg.setPixelType(EIGHT_BIT_GRAYSCALE);

    int options = JPEG_LUMA_ONLY;
    if (scale == 1) options |= JPEG_SCALE_HALF;
    else if (scale == 2) options |= JPEG_SCALE_QUARTER;
    else if (scale == 3) options |= JPEG_SCALE_EIGHTH;

    JpegDecCtx ctx{&image_data};
    jpeg.setUserPointer(&ctx);

    if (!jpeg.decode(0, 0, options)) {
        LOG_E("JPEG decode failed for %s", filename);
        free(image_data.bitmap);
        image_data.bitmap = nullptr;
        jpeg.close();
        return false;
    }

    jpeg.close();
    return true;
}

#else

bool SDImageLoader::load_jpeg(const char * filename, Image::ImageData & image_data, Dim max_dim) {
    return false;
}

#endif
