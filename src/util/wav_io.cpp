#include "util/wav_io.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace
{

uint16_t rd16(const unsigned char* p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

uint32_t rd32(const unsigned char* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void wr16(std::FILE* f, uint16_t v)
{
    unsigned char b[2] = {(unsigned char)(v & 0xff), (unsigned char)(v >> 8)};
    std::fwrite(b, 1, 2, f);
}

void wr32(std::FILE* f, uint32_t v)
{
    unsigned char b[4] = {(unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff), (unsigned char)((v >> 16) & 0xff),
                          (unsigned char)((v >> 24) & 0xff)};
    std::fwrite(b, 1, 4, f);
}

} // namespace

WavReader::~WavReader()
{
    close();
}

bool WavReader::open(const std::string& path, std::string& err)
{
    close();
    f_ = std::fopen(path.c_str(), "rb");
    if (!f_)
    {
        err = "cannot open " + path;
        return false;
    }

    unsigned char hdr[12];
    if (std::fread(hdr, 1, 12, f_) != 12 || std::memcmp(hdr, "RIFF", 4) != 0 || std::memcmp(hdr + 8, "WAVE", 4) != 0)
    {
        err = "not a RIFF/WAVE file";
        close();
        return false;
    }

    bool haveFmt = false, haveData = false;
    while (!haveFmt || !haveData)
    {
        unsigned char ch[8];
        if (std::fread(ch, 1, 8, f_) != 8)
        {
            break;
        }
        uint32_t sz = rd32(ch + 4);
        if (std::memcmp(ch, "fmt ", 4) == 0)
        {
            std::vector<unsigned char> fmt(sz);
            if (std::fread(fmt.data(), 1, sz, f_) != sz)
            {
                break;
            }
            if (sz >= 16)
            {
                int audioFormat = rd16(fmt.data());
                fmt_.channels   = rd16(fmt.data() + 2);
                fmt_.sampleRate = (int)rd32(fmt.data() + 4);
                fmt_.bits       = rd16(fmt.data() + 14);
                fmt_.isFloat    = (audioFormat == 3);
            }
            haveFmt = true;
            if (sz & 1)
            {
                std::fseek(f_, 1, SEEK_CUR);
            }
        }
        else if (std::memcmp(ch, "data", 4) == 0)
        {
            dataOffset_ = std::ftell(f_);
            dataBytes_  = sz;
            haveData    = true;
            std::fseek(f_, (long)(sz + (sz & 1)), SEEK_CUR);
        }
        else
        {
            std::fseek(f_, (long)(sz + (sz & 1)), SEEK_CUR);
        }
    }

    if (!haveFmt || !haveData || fmt_.channels < 1 || fmt_.channels > 2)
    {
        err = "unsupported WAV layout";
        close();
        return false;
    }

    // Trust the physical file length over the (possibly truncated) 32-bit
    // data-chunk size, which overflows for >4 GB SDR captures.
    std::fseek(f_, 0, SEEK_END);
    long fileSize = std::ftell(f_);
    int  frameBytes = fmt_.channels * (fmt_.bits / 8);
    if (fileSize > dataOffset_ && frameBytes > 0)
    {
        uint64_t physical = (uint64_t)(fileSize - dataOffset_);
        if (dataBytes_ == 0 || dataBytes_ > physical)
        {
            dataBytes_ = physical;
        }
    }
    totalFrames_ = frameBytes > 0 ? dataBytes_ / (uint64_t)frameBytes : 0;
    rewind();
    return true;
}

void WavReader::close()
{
    if (f_)
    {
        std::fclose(f_);
        f_ = nullptr;
    }
}

void WavReader::rewind()
{
    framePos_ = 0;
    if (f_)
    {
        std::fseek(f_, (long)dataOffset_, SEEK_SET);
    }
}

int WavReader::readFrames(float* out, int maxFrames)
{
    if (!f_ || maxFrames <= 0)
    {
        return 0;
    }

    const int bytesPerSample = fmt_.bits / 8;
    const int frameBytes     = fmt_.channels * bytesPerSample;
    const int want           = maxFrames;
    std::vector<unsigned char> raw((size_t)want * frameBytes);
    size_t got = std::fread(raw.data(), 1, (size_t)want * frameBytes, f_);
    int frames = (int)(got / (size_t)frameBytes);
    if (frames <= 0)
    {
        return 0;
    }

    for (int i = 0; i < frames; ++i)
    {
        const unsigned char* p = raw.data() + (size_t)i * frameBytes;
        for (int c = 0; c < fmt_.channels; ++c)
        {
            const unsigned char* s = p + (size_t)c * bytesPerSample;
            float                v = 0.0f;
            switch (fmt_.bits)
            {
            case 8:
                v = ((int)s[0] - 128) * (1.0f / 128.0f);
                break;
            case 16:
            {
                int16_t x = (int16_t)rd16(s);
                v = x * (1.0f / 32768.0f);
                break;
            }
            case 24:
            {
                int32_t x = (int32_t)((uint32_t)s[0] << 8 | (uint32_t)s[1] << 16 | (uint32_t)s[2] << 24);
                v = (float)(x >> 8) * (1.0f / 8388608.0f);
                break;
            }
            case 32:
                if (fmt_.isFloat)
                {
                    float fx;
                    std::memcpy(&fx, s, 4);
                    v = fx;
                }
                else
                {
                    int32_t x = (int32_t)rd32(s);
                    v = (float)(x * (1.0 / 2147483648.0));
                }
                break;
            default:
                break;
            }
            out[(size_t)i * fmt_.channels + c] = v;
        }
    }
    framePos_ += (uint64_t)frames;
    return frames;
}

WavWriter::~WavWriter()
{
    close();
}

bool WavWriter::open(const std::string& path, int sampleRate, int channels, std::string& err)
{
    close();
    f_ = std::fopen(path.c_str(), "wb");
    if (!f_)
    {
        err = "cannot create " + path;
        return false;
    }
    channels_ = channels;
    frames_   = 0;

    const uint16_t blockAlign = (uint16_t)(channels * 2);
    std::fwrite("RIFF", 1, 4, f_);
    wr32(f_, 0);
    std::fwrite("WAVE", 1, 4, f_);
    std::fwrite("fmt ", 1, 4, f_);
    wr32(f_, 16);
    wr16(f_, 1);
    wr16(f_, (uint16_t)channels);
    wr32(f_, (uint32_t)sampleRate);
    wr32(f_, (uint32_t)(sampleRate * blockAlign));
    wr16(f_, blockAlign);
    wr16(f_, 16);
    std::fwrite("data", 1, 4, f_);
    wr32(f_, 0);
    return true;
}

void WavWriter::write(const float* interleaved, int frames)
{
    if (!f_ || frames <= 0)
    {
        return;
    }
    std::vector<int16_t> buf((size_t)frames * channels_);
    for (size_t i = 0; i < buf.size(); ++i)
    {
        float v = interleaved[i];
        if (v > 1.0f)
        {
            v = 1.0f;
        }
        if (v < -1.0f)
        {
            v = -1.0f;
        }
        buf[i] = (int16_t)std::lround(v * 32767.0f);
    }
    std::fwrite(buf.data(), sizeof(int16_t), buf.size(), f_);
    frames_ += (uint64_t)frames;
}

void WavWriter::close()
{
    if (!f_)
    {
        return;
    }
    uint32_t dataBytes = (uint32_t)(frames_ * (uint64_t)channels_ * 2);
    std::fseek(f_, 4, SEEK_SET);
    wr32(f_, 36 + dataBytes);
    std::fseek(f_, 40, SEEK_SET);
    wr32(f_, dataBytes);
    std::fclose(f_);
    f_ = nullptr;
}
