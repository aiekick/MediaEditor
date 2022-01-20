#include <iostream>
#include <sstream>
#include <iomanip>
#include <memory>
#include <functional>
#include "Logger.h"
#include "FFUtils.h"
extern "C"
{
    #include "libavutil/pixdesc.h"
    #include "libavutil/hwcontext.h"
}

using namespace std;
using namespace Logger;

const AVRational MILLISEC_TIMEBASE = { 1, 1000 };
const AVRational FF_AV_TIMEBASE = { 1, AV_TIME_BASE };

string MillisecToString(int64_t millisec)
{
    ostringstream oss;
    if (millisec < 0)
    {
        oss << "-";
        millisec = -millisec;
    }
    uint64_t t = (uint64_t) millisec;
    uint32_t milli = (uint32_t)(t%1000); t /= 1000;
    uint32_t sec = (uint32_t)(t%60); t /= 60;
    uint32_t min = (uint32_t)(t%60); t /= 60;
    uint32_t hour = (uint32_t)t;
    oss << setfill('0') << setw(2) << hour << ':'
        << setw(2) << min << ':'
        << setw(2) << sec << '.'
        << setw(3) << milli;
    return oss.str();
}

string TimestampToString(double timestamp)
{
    return MillisecToString((int64_t)(timestamp*1000));
}

int AVPixelFormatToImColorFormat(AVPixelFormat pixfmt)
{
    int clrfmt = -1;
    switch (pixfmt)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
        case AV_PIX_FMT_YUV420P9:
        case AV_PIX_FMT_YUV420P10:
        case AV_PIX_FMT_YUV420P12:
        case AV_PIX_FMT_YUV420P14:
        case AV_PIX_FMT_YUV420P16:
            clrfmt = IM_CF_YUV420;
            break;
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUVJ422P:
        case AV_PIX_FMT_YUV422P9:
        case AV_PIX_FMT_YUV422P10:
        case AV_PIX_FMT_YUV422P12:
        case AV_PIX_FMT_YUV422P14:
        case AV_PIX_FMT_YUV422P16:
            clrfmt = IM_CF_YUV422;
            break;
        case AV_PIX_FMT_YUV444P:
        case AV_PIX_FMT_YUVJ444P:
        case AV_PIX_FMT_YUV444P9:
        case AV_PIX_FMT_YUV444P10:
        case AV_PIX_FMT_YUV444P12:
        case AV_PIX_FMT_YUV444P14:
        case AV_PIX_FMT_YUV444P16:
            clrfmt = IM_CF_YUV444;
            break;
        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
        case AV_PIX_FMT_NV16:
        case AV_PIX_FMT_NV24:
        case AV_PIX_FMT_NV42:
            clrfmt = IM_CF_NV12;
            break;
        case AV_PIX_FMT_NV20:
        case AV_PIX_FMT_P010:
        case AV_PIX_FMT_P016:
            clrfmt = IM_CF_P010LE;
            break;
        case AV_PIX_FMT_YUVA420P:
        case AV_PIX_FMT_YUVA422P:
        case AV_PIX_FMT_YUVA444P:
            clrfmt = IM_CF_YUVA;
            break;
        case AV_PIX_FMT_GRAY8:
        case AV_PIX_FMT_GRAY10:
        case AV_PIX_FMT_GRAY12:
        case AV_PIX_FMT_GRAY14:
        case AV_PIX_FMT_GRAY16:
            clrfmt = IM_CF_GRAY;
            break;
        case AV_PIX_FMT_RGB8:
        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_RGB555:
        case AV_PIX_FMT_RGB565:
        case AV_PIX_FMT_RGB48:
            clrfmt = IM_CF_RGB;
            break;
        case AV_PIX_FMT_BGR8:
        case AV_PIX_FMT_BGR24:
        case AV_PIX_FMT_BGR555:
        case AV_PIX_FMT_BGR565:
        case AV_PIX_FMT_BGR48:
            clrfmt = IM_CF_BGR;
            break;
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_RGB0:
        case AV_PIX_FMT_RGBA64:
            clrfmt = IM_CF_RGBA;
            break;
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:
        case AV_PIX_FMT_BGRA64:
            clrfmt = IM_CF_BGRA;
            break;
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_0RGB:
            clrfmt = IM_CF_ARGB;
            break;
        case AV_PIX_FMT_ABGR:
        case AV_PIX_FMT_0BGR:
            clrfmt = IM_CF_ABGR;
            break;
        default:
            Log(Error) << "No matching 'ImColorFormat' value for 'AVPixelFormat' " << (int)pixfmt << "!" << endl;
            break;
    }
    return clrfmt;
}

static ImColorFormat ConvertPixelFormatToColorFormat(AVPixelFormat pixfmt)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pixfmt);
    ImColorFormat clrfmt = (ImColorFormat)-1;
    if (pixfmt == AV_PIX_FMT_GRAY8 || pixfmt == AV_PIX_FMT_GRAY10 || pixfmt == AV_PIX_FMT_GRAY12 ||
        pixfmt == AV_PIX_FMT_GRAY14 || pixfmt == AV_PIX_FMT_GRAY16 || pixfmt == AV_PIX_FMT_GRAYF32)
        clrfmt = IM_CF_GRAY;
    else if (pixfmt == AV_PIX_FMT_BGR24 || pixfmt == AV_PIX_FMT_BGR48)
        clrfmt = IM_CF_BGR;
    else if (pixfmt == AV_PIX_FMT_ABGR || pixfmt == AV_PIX_FMT_0BGR)
        clrfmt = IM_CF_ABGR;
    else if (pixfmt == AV_PIX_FMT_BGRA || pixfmt == AV_PIX_FMT_BGR0 || pixfmt == AV_PIX_FMT_BGRA64)
        clrfmt = IM_CF_BGRA;
    else if (pixfmt == AV_PIX_FMT_RGB24 || pixfmt == AV_PIX_FMT_RGB48)
        clrfmt = IM_CF_RGB;
    else if (pixfmt == AV_PIX_FMT_ARGB || pixfmt == AV_PIX_FMT_0RGB)
        clrfmt = IM_CF_ARGB;
    else if (pixfmt == AV_PIX_FMT_RGBA || pixfmt == AV_PIX_FMT_RGB0 || pixfmt == AV_PIX_FMT_RGBA64)
        clrfmt = IM_CF_RGBA;
    else if (pixfmt == AV_PIX_FMT_YUV420P || pixfmt == AV_PIX_FMT_YUV420P10 || pixfmt == AV_PIX_FMT_YUV420P12 ||
        pixfmt == AV_PIX_FMT_YUV420P14 || pixfmt == AV_PIX_FMT_YUV420P16)
        clrfmt = IM_CF_YUV420;
    else if (pixfmt == AV_PIX_FMT_YUV422P || pixfmt == AV_PIX_FMT_YUV422P10 || pixfmt == AV_PIX_FMT_YUV422P12 ||
        pixfmt == AV_PIX_FMT_YUV422P14 || pixfmt == AV_PIX_FMT_YUV422P16)
        clrfmt = IM_CF_YUV422;
    else if (pixfmt == AV_PIX_FMT_YUV444P || pixfmt == AV_PIX_FMT_YUV444P10 || pixfmt == AV_PIX_FMT_YUV444P12 ||
        pixfmt == AV_PIX_FMT_YUV444P14 || pixfmt == AV_PIX_FMT_YUV444P16)
        clrfmt = IM_CF_YUV444;
    else if (pixfmt == AV_PIX_FMT_YUVA444P || pixfmt == AV_PIX_FMT_YUVA444P10 || pixfmt == AV_PIX_FMT_YUV444P12 ||
        pixfmt == AV_PIX_FMT_YUVA444P16)
        clrfmt = IM_CF_YUVA;
    else if (pixfmt == AV_PIX_FMT_NV12 || pixfmt == AV_PIX_FMT_NV21)
        clrfmt = IM_CF_NV12;
    else if (pixfmt == AV_PIX_FMT_P010)
        clrfmt = IM_CF_P010LE;
    return clrfmt;
}

static ImColorFormat FindPresentColorFormatForPixelFormat(AVPixelFormat pixfmt)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pixfmt);
    ImColorFormat clrfmt;
    if ((desc->flags&AV_PIX_FMT_FLAG_RGB) != 0)
    {
        if (desc->nb_components > 3)
            clrfmt = IM_CF_RGBA;
        else
            clrfmt = IM_CF_RGB;
    }
    else
    {
        if (desc->nb_components > 3)
            clrfmt = IM_CF_YUVA;
        else
            clrfmt = IM_CF_YUV444;
    }
    return clrfmt;
}

static AVPixelFormat ConvertColorFormatToPixelFormat(ImColorFormat clrfmt, ImDataType dtype)
{
    AVPixelFormat pixfmt = AV_PIX_FMT_NONE;
    if (clrfmt == IM_CF_GRAY)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_GRAY8;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_GRAY16;
        else if (dtype == IM_DT_FLOAT32)
            pixfmt = AV_PIX_FMT_GRAYF32;
    }
    else if (clrfmt == IM_CF_BGR)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_BGR24;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_BGR48;
    }
    else if (clrfmt == IM_CF_ABGR)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_ABGR;
    }
    else if (clrfmt == IM_CF_BGRA)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_BGRA;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_BGRA64;
    }
    else if (clrfmt == IM_CF_RGB)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_RGB24;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_RGB48;
    }
    else if (clrfmt == IM_CF_ARGB)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_ARGB;
    }
    else if (clrfmt == IM_CF_RGBA)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_RGBA;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_RGBA64;
    }
    else if (clrfmt == IM_CF_YUV420)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUV420P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUV420P16;
    }
    else if (clrfmt == IM_CF_YUV422)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUV422P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUV422P16;
    }
    else if (clrfmt == IM_CF_YUV444)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUV444P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUV444P16;
    }
    else if (clrfmt == IM_CF_YUVA)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_YUVA444P;
        else if (dtype == IM_DT_INT16)
            pixfmt = AV_PIX_FMT_YUVA444P16;
    }
    else if (clrfmt == IM_CF_NV12)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_NV12;
    }
    else if (clrfmt == IM_CF_P010LE)
    {
        if (dtype == IM_DT_INT8)
            pixfmt = AV_PIX_FMT_P010LE;
    }
    return pixfmt;
}

static ImColorSpace ConvertAVColorSpaceToImColorSpace(AVColorSpace clrspc)
{
    ImColorSpace imclrspc;
    if (clrspc == AVCOL_SPC_RGB)
        imclrspc = IM_CS_SRGB;
    else if (clrspc == AVCOL_SPC_UNSPECIFIED)
        imclrspc = IM_CS_BT601;
    else if (clrspc == AVCOL_SPC_BT709)
        imclrspc = IM_CS_BT709;
    else if (clrspc == AVCOL_SPC_BT2020_CL || clrspc == AVCOL_SPC_BT2020_NCL)
        imclrspc = IM_CS_BT2020;
    else
        imclrspc = IM_CS_SRGB;

    return imclrspc;
}

static AVColorSpace ConvertImColorSpaceToAVColorSpace(ImColorSpace imclrspc)
{
    AVColorSpace clrspc = AVCOL_SPC_UNSPECIFIED;
    if (imclrspc == IM_CS_SRGB)
        clrspc = AVCOL_SPC_RGB;
    else if (imclrspc == IM_CS_BT709)
        clrspc = AVCOL_SPC_BT709;
    else if (imclrspc == IM_CS_BT2020)
        clrspc = AVCOL_SPC_BT2020_CL;
    return clrspc;
}

static ImColorRange ConvertAVColorRangeToImColorRange(AVColorRange clrrng)
{
    ImColorRange imclrrng;
    if (clrrng == AVCOL_RANGE_JPEG)
        imclrrng = IM_CR_FULL_RANGE;
    else
        imclrrng = IM_CR_NARROW_RANGE;
    return imclrrng;
}

static AVColorRange ConvertImColorRangeToAVColorRange(ImColorRange imclrrng)
{
    AVColorRange clrrng = AVCOL_RANGE_UNSPECIFIED;
    if (imclrrng == IM_CR_FULL_RANGE)
        clrrng = AVCOL_RANGE_JPEG;
    else if (imclrrng == IM_CR_NARROW_RANGE)
        clrrng = AVCOL_RANGE_MPEG;
    return clrrng;
}

SelfFreeAVFramePtr AllocSelfFreeAVFramePtr()
{
    SelfFreeAVFramePtr frm = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame* avfrm) {
        if (avfrm)
            av_frame_free(&avfrm);
    });
    if (!frm.get())
        return nullptr;
    return frm;
}

SelfFreeAVFramePtr CloneSelfFreeAVFramePtr(const AVFrame* avfrm)
{
    SelfFreeAVFramePtr frm = shared_ptr<AVFrame>(av_frame_clone(avfrm), [](AVFrame* avfrm) {
        if (avfrm)
            av_frame_free(&avfrm);
    });
    if (!frm.get())
        return nullptr;
    return frm;
}

bool IsHwFrame(const AVFrame* avfrm)
{
    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
    return (desc->flags&AV_PIX_FMT_FLAG_HWACCEL) > 0;
}

bool HwFrameToSwFrame(AVFrame* swfrm, const AVFrame* hwfrm)
{
    av_frame_unref(swfrm);
    int fferr = av_hwframe_transfer_data(swfrm, hwfrm, 0);
    if (fferr < 0)
    {
        Log(Error) << "av_hwframe_transfer_data() FAILED! fferr = " << fferr << "." << endl;
        return false;
    }
    av_frame_copy_props(swfrm, hwfrm);
    return true;
}

bool MakeAVFrameCopy(AVFrame* dst, const AVFrame* src)
{
    av_frame_unref(dst);
    dst->format = src->format;
    dst->width = src->width;
    dst->height = src->height;
    dst->channels = src->channels;
    dst->channel_layout = src->channel_layout;
    dst->sample_rate = dst->sample_rate;
    int fferr;
    if ((fferr = av_frame_get_buffer(dst, 0)) < 0)
    {
        Log(Error) << "av_frame_get_buffer() FAILED! fferr = " << fferr << "." << endl;
        return false;
    }
    if ((fferr = av_frame_copy(dst, src)) < 0)
    {
        Log(Error) << "av_frame_copy() FAILED! fferr = " << fferr << "." << endl;
        return false;
    }
    av_frame_copy_props(dst, src);
    return true;
}

bool ConvertAVFrameToImMat(const AVFrame* avfrm, ImGui::ImMat& vmat, double timestamp)
{
    SelfFreeAVFramePtr swfrm;
    if (IsHwFrame(avfrm))
    {
        swfrm = AllocSelfFreeAVFramePtr();
        if (!swfrm)
        {
            Log(Error) << "FAILED to allocate new AVFrame for ImMat conversion!" << endl;
            return false;
        }
        if (!HwFrameToSwFrame(swfrm.get(), avfrm))
            return false;
        avfrm = swfrm.get();
    }

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
    if (desc->nb_components <= 0 || desc->nb_components > 4)
    {
        Log(Error) << "INVALID 'nb_component' value " << desc->nb_components << " of pixel format '"
            << desc->name << "', can only support value from 1 ~ 4." << endl;
        return false;
    }
    bool isRgb = (desc->flags&AV_PIX_FMT_FLAG_RGB) > 0;

    int bitDepth = desc->comp[0].depth;
    ImColorSpace color_space =  avfrm->colorspace == AVCOL_SPC_BT470BG ||
                                avfrm->colorspace == AVCOL_SPC_SMPTE170M ||
                                avfrm->colorspace == AVCOL_SPC_BT470BG ? IM_CS_BT601 :
                                avfrm->colorspace == AVCOL_SPC_BT709 ? IM_CS_BT709 :
                                avfrm->colorspace == AVCOL_SPC_BT2020_NCL ||
                                avfrm->colorspace == AVCOL_SPC_BT2020_CL ? IM_CS_BT2020 : IM_CS_BT709;
    ImColorRange color_range =  avfrm->color_range == AVCOL_RANGE_MPEG ? IM_CR_NARROW_RANGE :
                                avfrm->color_range == AVCOL_RANGE_JPEG ? IM_CR_FULL_RANGE : IM_CR_NARROW_RANGE;
    int clrfmt = AVPixelFormatToImColorFormat((AVPixelFormat)avfrm->format);
    if (clrfmt < 0)
        return false;
    ImColorFormat color_format = (ImColorFormat)clrfmt;
    const int width = avfrm->width;
    const int height = avfrm->height;

    ImGui::ImMat mat_V;
    int channel;
    ImDataType dataType = bitDepth > 8 ? IM_DT_INT16 : IM_DT_INT8;
    if (isRgb)
        channel = desc->nb_components;
    else
    {
        if (color_format == IM_CF_YUV444)
            channel = 3;
        else
            channel = 2;
    }
    mat_V.create_type(width, height, channel, dataType);
    uint8_t* prevDataPtr = nullptr;
    for (int i = 0; i < desc->nb_components; i++)
    {
        int chWidth = width;
        int chHeight = height;
        if ((desc->flags&AV_PIX_FMT_FLAG_RGB) == 0 && i > 0)
        {
            chWidth >>= desc->log2_chroma_w;
            chHeight >>= desc->log2_chroma_h;
        }
        if (desc->nb_components > i && desc->comp[i].plane == i)
        {
            uint8_t* src_data = avfrm->data[i]+desc->comp[i].offset;
            uint8_t* dst_data;
            if (i < channel)
                dst_data = (uint8_t*)mat_V.channel(i).data;
            else
                dst_data = prevDataPtr;
            int bytesPerLine = chWidth*desc->comp[i].step;
            for (int j = 0; j < chHeight; j++)
            {
                memcpy(dst_data, src_data, bytesPerLine);
                src_data += avfrm->linesize[i];
                dst_data += bytesPerLine;
            }
            prevDataPtr = dst_data;
        }
    }

    mat_V.color_space = color_space;
    mat_V.color_range = color_range;
    mat_V.color_format = color_format;
    mat_V.depth = bitDepth;
    mat_V.flags = IM_MAT_FLAGS_VIDEO_FRAME;
    if (avfrm->pict_type == AV_PICTURE_TYPE_I) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_I;
    if (avfrm->pict_type == AV_PICTURE_TYPE_P) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_P;
    if (avfrm->pict_type == AV_PICTURE_TYPE_B) mat_V.flags |= IM_MAT_FLAGS_VIDEO_FRAME_B;
    if (avfrm->interlaced_frame) mat_V.flags |= IM_MAT_FLAGS_VIDEO_INTERLACED;
    mat_V.time_stamp = timestamp;

    vmat = mat_V;
    return true;
}

bool ConvertImMatToAVFrame(const ImGui::ImMat& vmat, AVFrame* avfrm, int64_t pts)
{
    AVPixelFormat cvtPixfmt = ConvertColorFormatToPixelFormat(vmat.color_format, vmat.type);
    if (cvtPixfmt < 0)
    {
        Log(ERROR) << "FAILED to convert ImColorFormat " << vmat.color_format << " and ImDataType " << vmat.type << " to AVPixelFormat!";
        return false;
    }
    av_frame_unref(avfrm);
    avfrm->width = vmat.w;
    avfrm->height = vmat.h;
    avfrm->format = (int)cvtPixfmt;
    int fferr;
    fferr = av_frame_get_buffer(avfrm, 0);
    if (fferr < 0)
    {
        Log(ERROR) << "FF api 'av_frame_get_buffer' failed! return code is " << fferr << ".";
        return false;
    }

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(cvtPixfmt);
    bool isRgb = (desc->flags&AV_PIX_FMT_FLAG_RGB) > 0;
    uint8_t* prevDataPtr = nullptr;
    int channel;
    if (isRgb)
        channel = desc->nb_components;
    else
    {
        if (vmat.color_format == IM_CF_YUV444)
            channel = 3;
        else
            channel = 2;
    }
    for (int i = 0; i < desc->nb_components; i++)
    {
        int chWidth = vmat.w;
        int chHeight = vmat.h;
        if ((desc->flags&AV_PIX_FMT_FLAG_RGB) == 0 && i > 0)
        {
            chWidth >>= desc->log2_chroma_w;
            chHeight >>= desc->log2_chroma_h;
        }
        if (desc->nb_components > i && desc->comp[i].plane == i)
        {
            uint8_t* dst_data = avfrm->data[i]+desc->comp[i].offset;
            uint8_t* src_data;
            if (i < channel)
                src_data = (uint8_t*)vmat.channel(i).data;
            else
                src_data = prevDataPtr;
            int bytesPerLine = chWidth*desc->comp[i].step;
            for (int j = 0; j < chHeight; j++)
            {
                memcpy(dst_data, src_data, bytesPerLine);
                src_data += bytesPerLine;
                dst_data += avfrm->linesize[i];
            }
            prevDataPtr = src_data;
        }
    }

    avfrm->colorspace = ConvertImColorSpaceToAVColorSpace(vmat.color_space);
    avfrm->color_range = ConvertImColorRangeToAVColorRange(vmat.color_range);
    avfrm->pts = pts;
    return true;
}

AVFrameToImMatConverter::AVFrameToImMatConverter()
{
#if IMGUI_VULKAN_SHADER
    m_useVulkanComponents = true;
    if (m_useVulkanComponents)
    {
        m_imgClrCvt = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
        m_imgRsz = new ImGui::Resize_vulkan(ImGui::get_default_gpu_index());
    }
#else
    m_useVulkanComponents = false;
#endif
}

AVFrameToImMatConverter::~AVFrameToImMatConverter()
{
#if IMGUI_VULKAN_SHADER
    if (m_imgClrCvt)
    {
        delete m_imgClrCvt;
        m_imgClrCvt = nullptr;
    }
    if (m_imgRsz)
    {
        delete m_imgRsz;
        m_imgRsz = nullptr;
    }
#endif
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
}

bool AVFrameToImMatConverter::SetOutSize(uint32_t width, uint32_t height)
{
    if (m_outWidth == width && m_outHeight == height)
        return true;

    m_outWidth = width;
    m_outHeight = height;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool AVFrameToImMatConverter::SetOutColorFormat(ImColorFormat clrfmt)
{
    if (clrfmt != IM_CF_RGBA)
    {
        m_errMsg = string("Do NOT SUPPORT output color format ")+to_string((int)clrfmt)+"!";
        return false;
    }
    if (m_outClrFmt == clrfmt)
        return true;

    m_outClrFmt = clrfmt;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool AVFrameToImMatConverter::SetResizeInterpolateMode(ImInterpolateMode interp)
{
    if (m_resizeInterp == interp)
        return true;

    switch (interp)
    {
        case IM_INTERPOLATE_NEAREST:
            m_swsFlags = 0;
            break;
        case IM_INTERPOLATE_BILINEAR:
            m_swsFlags = SWS_BILINEAR;
            break;
        case IM_INTERPOLATE_BICUBIC:
            m_swsFlags = SWS_BICUBIC;
            break;
        case IM_INTERPOLATE_AREA:
            m_swsFlags = SWS_AREA;
            break;
        default:
            return false;
    }
    m_resizeInterp = interp;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool AVFrameToImMatConverter::ConvertImage(const AVFrame* avfrm, ImGui::ImMat& outMat, double timestamp)
{
    if (m_useVulkanComponents)
    {
#if IMGUI_VULKAN_SHADER
        // AVFrame -> ImMat
        ImGui::ImMat inMat;
        if (!ConvertAVFrameToImMat(avfrm, inMat, timestamp))
        {
            m_errMsg = "Failed to invoke 'ConvertAVFrameToImMat()'!";
            return false;
        }

        // YUV -> RGB
        ImGui::VkMat rgbMat;
        const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)avfrm->format);
        if ((desc->flags&AV_PIX_FMT_FLAG_RGB) == 0)
        {
            int bitDepth = inMat.type == IM_DT_INT8 ? 8 : inMat.type == IM_DT_INT16 ? 16 : 8;
            int shiftBits = inMat.depth != 0 ? inMat.depth : inMat.type == IM_DT_INT8 ? 8 : inMat.type == IM_DT_INT16 ? 16 : 8;
            rgbMat.type = IM_DT_INT8;
            m_imgClrCvt->YUV2RGBA(inMat, rgbMat, inMat.color_format, inMat.color_space, inMat.color_range, bitDepth, shiftBits);
        }
        else
        {
            if (inMat.color_format != m_outClrFmt)
            {
                m_errMsg = string("Can NOT FIND a way to convert intermediate ImMat from color format ")
                    +to_string((int)inMat.color_format)+" to output format "+to_string((int)m_outClrFmt)+"!";
                return false;
            }
            rgbMat = inMat;
        }

        // Resize
        uint32_t outWidth = m_outWidth == 0 ? rgbMat.w : m_outWidth;
        uint32_t outHeight = m_outHeight == 0 ? rgbMat.h : m_outHeight;
        if (outWidth != rgbMat.w || outHeight != rgbMat.h)
        {
            ImGui::VkMat rszMat;
            rszMat.type = IM_DT_INT8;
            m_imgRsz->Resize(rgbMat, rszMat, (float)outWidth/rgbMat.w, (float)outHeight/rgbMat.h, m_resizeInterp);
            outMat = rszMat;
        }
        else
            outMat = rgbMat;
        outMat.time_stamp = timestamp;
        return true;
#else
        m_errMsg = "Vulkan shader components is NOT ENABLED!";
        return false;
#endif
    }
    else
    {
        SelfFreeAVFramePtr swfrm;
        if (IsHwFrame(avfrm))
        {
            swfrm = AllocSelfFreeAVFramePtr();
            if (!swfrm)
            {
                Log(Error) << "FAILED to allocate new AVFrame for ImMat conversion!" << endl;
                return false;
            }
            if (!HwFrameToSwFrame(swfrm.get(), avfrm))
                return false;
            avfrm = swfrm.get();
        }

        int outWidth = m_outWidth == 0 ? avfrm->width : m_outWidth;
        int outHeight = m_outHeight == 0 ? avfrm->height : m_outHeight;
        if (!(m_swsCtx || m_passThrough) ||
            m_swsInWidth != avfrm->width || m_swsInHeight != avfrm->height ||
            (int)m_swsInFormat != avfrm->format || m_swsClrspc != avfrm->colorspace)
        {
            if (m_swsCtx)
            {
                sws_freeContext(m_swsCtx);
                m_swsCtx = nullptr;
            }
            if (avfrm->width != outWidth || avfrm->height != outHeight || avfrm->format != (int)m_swsOutFormat)
            {
                m_swsCtx = sws_getContext(avfrm->width, avfrm->height, (AVPixelFormat)avfrm->format, outWidth, outHeight, m_swsOutFormat, m_swsFlags, nullptr, nullptr, nullptr);
                if (!m_swsCtx)
                {
                    ostringstream oss;
                    oss << "FAILED to create SwsContext from WxH(" << avfrm->width << "x" << avfrm->height << "):Fmt(" << avfrm->format << ") -> WxH(" << outWidth << "x" << outHeight << "):Fmt(" << (int)m_swsOutFormat << ") with flags(" << m_swsFlags << ")!";
                    m_errMsg = oss.str();
                    return false;
                }
                int srcRange, dstRange, brightness, contrast, saturation;
                int *invTable0, *table0;
                sws_getColorspaceDetails(m_swsCtx, &invTable0, &srcRange, &table0, &dstRange, &brightness, &contrast, &saturation);
                const int *invTable1, *table1;
                table1 = invTable1 = sws_getCoefficients(avfrm->colorspace);
                sws_setColorspaceDetails(m_swsCtx, invTable1, srcRange, table1, dstRange, brightness, contrast, saturation);
                m_swsInWidth = avfrm->width;
                m_swsInHeight = avfrm->height;
                m_swsInFormat = (AVPixelFormat)avfrm->format;
                m_swsClrspc = avfrm->colorspace;
            }
            else
            {
                m_swsClrspc = avfrm->colorspace;
                m_passThrough = true;
            }
        }

        SelfFreeAVFramePtr swsfrm;
        if (!m_passThrough && m_swsCtx)
        {
            swsfrm = AllocSelfFreeAVFramePtr();
            if (!swsfrm)
            {
                m_errMsg = "FAILED to allocate AVFrame to perform 'swscale'!";
                return false;
            }

            AVFrame* pfrm = swsfrm.get();
            pfrm->width = outWidth;
            pfrm->height = outHeight;
            pfrm->format = (int)m_swsOutFormat;
            int fferr = av_frame_get_buffer(pfrm, 0);
            if (fferr < 0)
            {
                m_errMsg = string("FAILED to invoke 'av_frame_get_buffer()' for 'swsfrm'! fferr = ")+to_string(fferr)+".";
                return false;
            }
            fferr = sws_scale(m_swsCtx, avfrm->data, avfrm->linesize, 0, avfrm->height, swsfrm->data, swsfrm->linesize);
            av_frame_copy_props(swsfrm.get(), avfrm);
            avfrm = swsfrm.get();
        }

        // AVFrame -> ImMat
        if (!ConvertAVFrameToImMat(avfrm, outMat, timestamp))
        {
            m_errMsg = "Failed to invoke 'ConvertAVFrameToImMat()'!";
            return false;
        }

        outMat.time_stamp = timestamp;
        return true;
    }
}

ImMatToAVFrameConverter::ImMatToAVFrameConverter()
{
#if IMGUI_VULKAN_SHADER
    m_useVulkanComponents = true;
    if (m_useVulkanComponents)
    {
        m_imgClrCvt = new ImGui::ColorConvert_vulkan(ImGui::get_default_gpu_index());
        m_imgRsz = new ImGui::Resize_vulkan(ImGui::get_default_gpu_index());
    }
    m_outMatClrfmt = ConvertPixelFormatToColorFormat(m_outPixfmt);
    if (m_outMatClrfmt < 0)
    {
        m_outMatClrfmt = FindPresentColorFormatForPixelFormat(m_outPixfmt);
        m_isClrfmtMatched = false;
    }
    else
        m_isClrfmtMatched = true;
    m_outMatClrspc = ConvertAVColorSpaceToImColorSpace(m_outClrspc);
    m_outMatClrrng = ConvertAVColorRangeToImColorRange(m_outClrrng);
#else
    m_useVulkanComponents = false;
#endif
    m_pixDesc = av_pix_fmt_desc_get(m_outPixfmt);
    m_outBitsPerPix = m_pixDesc->comp[0].depth;
}

ImMatToAVFrameConverter::~ImMatToAVFrameConverter()
{
#if IMGUI_VULKAN_SHADER
    if (m_imgClrCvt)
    {
        delete m_imgClrCvt;
        m_imgClrCvt = nullptr;
    }
    if (m_imgRsz)
    {
        delete m_imgRsz;
        m_imgRsz = nullptr;
    }
#endif
    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
    }
}

bool ImMatToAVFrameConverter::SetOutSize(uint32_t width, uint32_t height)
{
    if (m_outWidth == width && m_outHeight == height)
        return true;

    m_outWidth = width;
    m_outHeight = height;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetOutPixelFormat(AVPixelFormat pixfmt)
{
    if (m_outPixfmt == pixfmt)
        return true;

    const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(pixfmt);
    if (!desc)
    {
        ostringstream oss;
        oss << "FAILED to get 'AVPixFmtDescriptor' for AVPixelFormat " << pixfmt << "!";
        m_errMsg = oss.str();
        return false;
    }

    m_outPixfmt = pixfmt;
    m_pixDesc = desc;
    m_outBitsPerPix = desc->comp[0].depth;
#if IMGUI_VULKAN_SHADER
    m_outMatClrfmt = ConvertPixelFormatToColorFormat(m_outPixfmt);
    if (m_outMatClrfmt < 0)
    {
        m_outMatClrfmt = FindPresentColorFormatForPixelFormat(m_outPixfmt);
        m_isClrfmtMatched = false;
    }
    else
        m_isClrfmtMatched = true;
#endif

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetOutColorSpace(AVColorSpace clrspc)
{
    if (m_outClrspc == clrspc)
        return true;

    m_outClrspc = clrspc;
#if IMGUI_VULKAN_SHADER
    m_outMatClrspc = ConvertAVColorSpaceToImColorSpace(clrspc);
#endif

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetOutColorRange(AVColorRange clrrng)
{
    if (m_outClrrng == clrrng)
        return true;

    m_outClrrng = clrrng;
#if IMGUI_VULKAN_SHADER
    m_outMatClrrng = ConvertAVColorRangeToImColorRange(clrrng);
#endif

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::SetResizeInterpolateMode(ImInterpolateMode interp)
{
    if (m_resizeInterp == interp)
        return true;

    switch (interp)
    {
        case IM_INTERPOLATE_NEAREST:
            m_swsFlags = 0;
            break;
        case IM_INTERPOLATE_BILINEAR:
            m_swsFlags = SWS_BILINEAR;
            break;
        case IM_INTERPOLATE_BICUBIC:
            m_swsFlags = SWS_BICUBIC;
            break;
        case IM_INTERPOLATE_AREA:
            m_swsFlags = SWS_AREA;
            break;
        default:
            return false;
    }
    m_resizeInterp = interp;

    if (m_swsCtx)
    {
        sws_freeContext(m_swsCtx);
        m_swsCtx = nullptr;
        m_passThrough = false;
    }
    return true;
}

bool ImMatToAVFrameConverter::ConvertImage(ImGui::ImMat& vmat, AVFrame* avfrm, int64_t pts)
{
    ImGui::ImMat inMat = vmat;
    if (m_useVulkanComponents)
    {
#if IMGUI_VULKAN_SHADER
        // Resize
        uint32_t outWidth = m_outWidth == 0 ? inMat.w : m_outWidth;
        uint32_t outHeight = m_outHeight == 0 ? inMat.h : m_outHeight;
        if (outWidth != inMat.w || outHeight != inMat.h)
        {
            ImGui::VkMat rszMat;
            rszMat.type = m_outBitsPerPix > 8 ? IM_DT_INT16 : IM_DT_INT8;
            m_imgRsz->Resize(inMat, rszMat, (float)outWidth/inMat.w, (float)outHeight/inMat.h, m_resizeInterp);
            inMat = rszMat;
        }

        // RGB -> YUV
        bool isSrcRgb = inMat.color_format >= IM_CF_BGR && inMat.color_format <= IM_CF_RGBA;
        bool isDstYuv = (m_pixDesc->flags&AV_PIX_FMT_FLAG_RGB) == 0;
        if (isSrcRgb && isDstYuv)
        {
            int bitDepth = inMat.type == IM_DT_INT8 ? 8 : inMat.type == IM_DT_INT16 ? 16 : 8;
            int shiftBits = inMat.depth != 0 ? inMat.depth : inMat.type == IM_DT_INT8 ? 8 : inMat.type == IM_DT_INT16 ? 16 : 8;
            ImGui::ImMat yuvMat;
            yuvMat.type = inMat.type;
            if (inMat.device == IM_DD_CPU)
                m_imgClrCvt->RGBA2YUV(inMat, yuvMat, m_outMatClrfmt, m_outMatClrspc, m_outMatClrrng, shiftBits);
            else
            {
                ImGui::VkMat vkMat = inMat;
                m_imgClrCvt->RGBA2YUV(vkMat, yuvMat, m_outMatClrfmt, m_outMatClrspc, m_outMatClrrng, shiftBits);
            }
            inMat = yuvMat;
        }
#else
        m_errMsg = "Vulkan shader components is NOT ENABLED!";
        return false;
#endif
    }

    // ImMat -> AVFrame
    AVPixelFormat cvtPixfmt = ConvertColorFormatToPixelFormat(inMat.color_format, inMat.type);
    if (cvtPixfmt < 0)
    {
        ostringstream oss;
        oss << "FAILED to convert ImColorFormat " << inMat.color_format << " and ImDataType " << inMat.type << " to AVPixelFormat!";
        m_errMsg = oss.str();
        return false;
    }

    if (m_outPixfmt == cvtPixfmt && m_outWidth == inMat.w && m_outHeight == inMat.h)
    {
        if (!ConvertImMatToAVFrame(inMat, avfrm, pts))
        {
            m_errMsg = "FAILED to invoke 'ConvertImMatToAVFrame'!";
            return false;
        }
        return true;
    }

    SelfFreeAVFramePtr cvtfrm = AllocSelfFreeAVFramePtr();
    if (!cvtfrm)
    {
        m_errMsg = "FAILED allocate AVFrame for conversion from ImMat!";
        return false;
    }
    if (!ConvertImMatToAVFrame(inMat, cvtfrm.get(), pts))
    {
        m_errMsg = "FAILED to invoke 'ConvertImMatToAVFrame'!";
        return false;
    }

    if (!m_swsCtx || m_swsInWidth != inMat.w || m_swsInHeight != inMat.h || m_swsInFormat != cvtPixfmt)
    {
        if (m_swsCtx)
        {
            sws_freeContext(m_swsCtx);
            m_swsCtx = nullptr;
        }
        m_swsCtx = sws_getContext(inMat.w, inMat.h, cvtPixfmt, m_outWidth, m_outHeight, m_outPixfmt, m_swsFlags, nullptr, nullptr, nullptr);
        if (!m_swsCtx)
        {
            ostringstream oss;
            oss << "FAILED to create SwsContext from WxH(" << inMat.w << "x" << inMat.h << "):Fmt(" << (int)cvtPixfmt << ") -> WxH(" << m_outWidth << "x" << m_outHeight << "):Fmt(" << (int)m_outPixfmt << ") with flags(" << m_swsFlags << ")!";
            m_errMsg = oss.str();
            return false;
        }
        int srcRange, dstRange, brightness, contrast, saturation;
        int *invTable0, *table0;
        sws_getColorspaceDetails(m_swsCtx, &invTable0, &srcRange, &table0, &dstRange, &brightness, &contrast, &saturation);
        const int *invTable1, *table1;
        table1 = invTable1 = sws_getCoefficients(m_outClrspc);
        sws_setColorspaceDetails(m_swsCtx, invTable1, srcRange, table1, dstRange, brightness, contrast, saturation);
        m_swsInWidth = inMat.w;
        m_swsInHeight = inMat.h;
        m_swsInFormat = cvtPixfmt;
    }

    av_frame_unref(avfrm);
    avfrm->format = m_outPixfmt;
    avfrm->width = m_outWidth;
    avfrm->height = m_outHeight;
    int fferr;
    fferr = av_frame_get_buffer(avfrm, 0);
    if (fferr < 0)
    {
        ostringstream oss;
        oss << "FAILED to invoke 'av_frame_get_buffer()' for 'swsfrm'! fferr = " << fferr << ".";
        m_errMsg = oss.str();
        return false;
    }
    fferr = sws_scale(m_swsCtx, cvtfrm->data, cvtfrm->linesize, 0, m_outHeight, avfrm->data, avfrm->linesize);
    av_frame_copy_props(avfrm, cvtfrm.get());

    return true;
}

MediaInfo::Ratio MediaInfoRatioFromAVRational(const AVRational& src)
{
    return { src.num, src.den };
}

MediaInfo::InfoHolder GenerateMediaInfoByAVFormatContext(const AVFormatContext* avfmtCtx)
{
    MediaInfo::InfoHolder hInfo(new MediaInfo::Info());
    hInfo->url = string(avfmtCtx->url);
    double fftb = av_q2d(FF_AV_TIMEBASE);
    hInfo->duration = avfmtCtx->duration*fftb;
    hInfo->startTime = avfmtCtx->start_time*fftb;
    hInfo->streams.reserve(avfmtCtx->nb_streams);
    for (uint32_t i = 0; i < avfmtCtx->nb_streams; i++)
    {
        MediaInfo::StreamHolder hStream;
        AVStream* stream = avfmtCtx->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;
        double streamtb = av_q2d(stream->time_base);
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            auto vidStream = new MediaInfo::VideoStream();
            vidStream->bitRate = codecpar->bit_rate;
            if (stream->start_time != AV_NOPTS_VALUE)
                vidStream->startTime = stream->start_time*streamtb;
            else
                vidStream->startTime = hInfo->startTime;
            if (stream->duration > 0)
                vidStream->duration = stream->duration*streamtb;
            else
                vidStream->duration = hInfo->duration;
            vidStream->timebase = MediaInfoRatioFromAVRational(stream->time_base);
            vidStream->width = codecpar->width;
            vidStream->height = codecpar->height;
            vidStream->sampleAspectRatio = MediaInfoRatioFromAVRational(stream->sample_aspect_ratio);
            vidStream->avgFrameRate = MediaInfoRatioFromAVRational(stream->avg_frame_rate);
            vidStream->realFrameRate = MediaInfoRatioFromAVRational(stream->r_frame_rate);
            if (stream->nb_frames > 0)
                vidStream->frameNum = stream->nb_frames;
            else if (stream->r_frame_rate.num > 0 && stream->r_frame_rate.den > 0)
                vidStream->frameNum = (uint64_t)(stream->duration*av_q2d(stream->r_frame_rate));
            else if (stream->avg_frame_rate.num > 0 && stream->avg_frame_rate.den > 0)
                vidStream->frameNum = (uint64_t)(stream->duration*av_q2d(stream->avg_frame_rate));
            switch (codecpar->color_trc)
            {
                case AVCOL_TRC_SMPTE2084:
                case AVCOL_TRC_ARIB_STD_B67:
                    vidStream->isHdr = true;
                    break;
                default:
                    vidStream->isHdr = false;
            }
            const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get((AVPixelFormat)codecpar->format);
            if (desc->nb_components > 0)
                vidStream->bitDepth = desc->comp[0].depth;
            hStream = MediaInfo::StreamHolder(vidStream);
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            auto audStream = new MediaInfo::AudioStream();
            audStream->bitRate = codecpar->bit_rate;
            if (stream->start_time != AV_NOPTS_VALUE)
                audStream->startTime = stream->start_time*streamtb;
            else
                audStream->startTime = hInfo->startTime;
            if (stream->duration > 0)
                audStream->duration = stream->duration*streamtb;
            else
                audStream->duration = hInfo->duration;
            audStream->timebase = MediaInfoRatioFromAVRational(stream->time_base);
            audStream->channels = codecpar->channels;
            audStream->sampleRate = codecpar->sample_rate;
            audStream->bitDepth = av_get_bytes_per_sample((AVSampleFormat)codecpar->format) << 3;
            hStream = MediaInfo::StreamHolder(audStream);
        }
        if (hStream)
            hInfo->streams.push_back(hStream);
    }
    return hInfo;
}
