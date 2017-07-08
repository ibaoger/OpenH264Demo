// OpenH264Demo.cpp
//

#include "stdafx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codec_api.h"

ISVCDecoder *g_pDecoder = 0;
const char *g_pH264 = ".\\src\\720p.h264";
const char *g_pYUV = ".\\src\\720p.yuv";
FILE *g_pH264File = 0;
FILE *g_pYUVFile = 0;

void readH264();
void decodeFrame(const unsigned char *data, int len);

int _tmain(int argc, _TCHAR *argv[])
{
    int result = WelsCreateDecoder(&g_pDecoder);
    if (result == 0)
    {
        SDecodingParam sDecParam = {0};
        //param.eOutputColorFormat = EVideoFormatType.videoFormatI420;	// only I420
        sDecParam.uiTargetDqLayer = UCHAR_MAX;
        sDecParam.eEcActiveIdc = ERROR_CON_FRAME_COPY_CROSS_IDR;
        sDecParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;
        result = g_pDecoder->Initialize(&sDecParam);

        if (result == 0)
        {
            g_pH264File = fopen(g_pH264, "rb");
            g_pYUVFile = fopen(g_pYUV, "wb+");
            readH264();
            fclose(g_pH264File);
            fclose(g_pYUVFile);
        }

        g_pDecoder->Uninitialize();
        WelsDestroyDecoder(g_pDecoder);
    }

    return 0;
}

void readH264()
{
    if (g_pH264File)
    {
        int size = 1024 * 1024 * 6;
        unsigned char *data = (unsigned char *)malloc(size);
        size = fread(data, sizeof(unsigned char), size, g_pH264File);
        int start = -1;
        int end = -1;
        for (int i = 0; i < size - 4;)
        {
            // parse NALU 00 00 00 01 or 00 00 01
            if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 0 && data[i + 3] == 1)
            {
                if (start == -1)
                {
                    start = i;
                    i += 4;
                    continue;
                }
                end = i;
            }
            else if (data[i] == 0 && data[i + 1] == 0 && data[i + 2] == 1)
            {
                if (start == -1)
                {
                    start = i;
                    i += 3;
                    continue;
                }
                end = i;
            }

            if (end >= 0)
            {
                decodeFrame(data + start, end - start);

                start = -1;
                end = -1;
                continue;
            }

            ++i;
        }

        free(data);
    }
}

void decodeFrame(const unsigned char *data, int len)
{
    if (g_pDecoder)
    {
        unsigned char *dst[3] = {0}; // Y plane, U plane, V plane
        SBufferInfo sDstBufInfo = {0};
        if (g_pDecoder->DecodeFrame2(data, len, dst, &sDstBufInfo) == 0)
        {
            if (sDstBufInfo.iBufferStatus == 1)
            {
                if (g_pYUVFile)
                {
                    // int format = sDstBufInfo.UsrData.sSystemBuffer.iFormat;  // I420
                    int width = sDstBufInfo.UsrData.sSystemBuffer.iWidth;
                    int height = sDstBufInfo.UsrData.sSystemBuffer.iHeight;
                    int y_src_width = sDstBufInfo.UsrData.sSystemBuffer.iStride[0];  // y_dst_width + padding
                    int uv_src_width = sDstBufInfo.UsrData.sSystemBuffer.iStride[1]; // uv_dst_width + padding
                    int y_dst_width = width;
                    int uv_dst_width = width / 2;
                    int y_dst_height = height;
                    int uv_dst_height = height / 2;
                    // y1 ... y1280, padding, y1281 ... y2560, padding, y2561 ... y921600, padding
                    unsigned char *y_plane = dst[0];
                    // u1 ... u640, padding, u641 ... u1280, padding, u1281 ... u230400, padding
                    unsigned char *u_plane = dst[1];
                    // v1 ... v640, padding, v641 ... v1280, padding, v1281 ... v230400, padding
                    unsigned char *v_plane = dst[2];
                    // y1 ... y1280, y1281 ... y2560, y2561 ... y921600
                    unsigned char *y_dst = (unsigned char *)malloc(width * height);
                    // u1 ... u640, u641 ... u1280, u1281 ... u230400
                    unsigned char *u_dst = (unsigned char *)malloc(width * height / 4);
                    // v1 ... v640, v641 ... v1280, v1281 ... v230400
                    unsigned char *v_dst = (unsigned char *)malloc(width * height / 4);
                    int rows = height;
                    for (int row = 0; row < rows; row++)
                    {
                        memcpy(y_dst + y_dst_width * row, y_plane + y_src_width * row, y_dst_width);
                    }
                    rows = height / 2;
                    for (int row = 0; row < rows; row++)
                    {
                        memcpy(u_dst + uv_dst_width * row, u_plane + uv_src_width * row, uv_dst_width);
                        memcpy(v_dst + uv_dst_width * row, v_plane + uv_src_width * row, uv_dst_width);
                    }
                    // yuv file:
                    // y1 ... y921600, u1 ... u230400, v1 ... v230400
                    fwrite(y_dst, 1, y_dst_width * y_dst_height, g_pYUVFile);
                    fwrite(u_dst, 1, uv_dst_width * uv_dst_height, g_pYUVFile);
                    fwrite(v_dst, 1, uv_dst_width * uv_dst_height, g_pYUVFile);
                    free(y_dst);
                    free(u_dst);
                    free(v_dst);
                }
            }
        }
    }
}
