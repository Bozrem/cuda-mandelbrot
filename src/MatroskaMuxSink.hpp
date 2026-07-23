#pragma once

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/mathematics.h>
#include <libavutil/mem.h>
}

class MatroskaMuxSink {
public:
    MatroskaMuxSink(const char* path, int width, int height, int fps)
        : frame_tb_{1, fps}
    {
        int err = avformat_alloc_output_context2(&oc_, nullptr, "matroska", path);
        check_av(err, "avformat_alloc_output_context2");
        if (!oc_) {
            throw std::runtime_error("avformat_alloc_output_context2 returned null");
        }

        err = avio_open(&oc_->pb, path, AVIO_FLAG_WRITE);
        check_av(err, "avio_open");

        st_ = avformat_new_stream(oc_, nullptr);
        if (!st_) {
            throw std::runtime_error("avformat_new_stream failed");
        }

        st_->id = 0;
        // Hint only — matroska overwrites time_base on write_header (typically 1/1000).
        st_->time_base = frame_tb_;
        st_->avg_frame_rate = AVRational{fps, 1};

        AVCodecParameters* par = st_->codecpar;
        par->codec_type = AVMEDIA_TYPE_VIDEO;
        par->codec_id = AV_CODEC_ID_HEVC;
        par->width = width;
        par->height = height;
        par->format = AV_PIX_FMT_YUV420P10LE;
        par->color_primaries = AVCOL_PRI_BT2020;
        par->color_trc = AVCOL_TRC_SMPTE2084;
        par->color_space = AVCOL_SPC_BT2020_NCL;
        par->color_range = AVCOL_RANGE_MPEG;
    }

    MatroskaMuxSink(const MatroskaMuxSink&) = delete;
    MatroskaMuxSink& operator=(const MatroskaMuxSink&) = delete;

    ~MatroskaMuxSink() {
        // Best-effort trailer; do not throw from a destructor.
        if (oc_ && header_written_) {
            av_write_trailer(oc_);
        }
        cleanup();
    }

    // Expects Annex-B VPS/SPS/PPS from NVENC GetSequenceParams. Matroska converts
    // this to hvcC in CodecPrivate, and will also convert Annex-B packets on write.
    void set_extradata(const uint8_t* data, size_t size) {
        if (header_written_) {
            throw std::runtime_error("set_extradata called after header was written");
        }
        if (!data || size == 0) {
            throw std::runtime_error("set_extradata: empty sequence params");
        }

        uint8_t* buf = static_cast<uint8_t*>(
            av_malloc(size + AV_INPUT_BUFFER_PADDING_SIZE)
        );
        if (!buf) {
            throw std::runtime_error("av_malloc failed for extradata");
        }
        std::memcpy(buf, data, size);
        std::memset(buf + size, 0, AV_INPUT_BUFFER_PADDING_SIZE);

        st_->codecpar->extradata = buf;
        st_->codecpar->extradata_size = static_cast<int>(size);

        int err = avformat_write_header(oc_, nullptr);
        check_av(err, "avformat_write_header");
        header_written_ = true;
    }

    // Pass NVENC Annex-B access units through as-is. With Annex-B extradata,
    // matroskaenc converts packets to length-prefixed form itself — converting
    // here first produced empty SimpleBlocks.
    void write_au(const uint8_t* data, size_t size, bool keyframe) {
        if (!header_written_) {
            throw std::runtime_error("write_au before set_extradata / write_header");
        }
        if (!data || size == 0) {
            return;
        }

        AVPacket* pkt = av_packet_alloc();
        if (!pkt) {
            throw std::runtime_error("av_packet_alloc failed");
        }

        int err = av_new_packet(pkt, static_cast<int>(size));
        if (err < 0) {
            av_packet_free(&pkt);
            check_av(err, "av_new_packet");
        }
        std::memcpy(pkt->data, data, size);

        // Stamp in frame time_base, then rescale into whatever matroska chose
        // for st_->time_base after write_header (usually 1/1000).
        pkt->pts = next_frame_;
        pkt->dts = next_frame_;
        pkt->duration = 1;
        av_packet_rescale_ts(pkt, frame_tb_, st_->time_base);
        pkt->stream_index = st_->index;
        pkt->flags = keyframe ? AV_PKT_FLAG_KEY : 0;
        ++next_frame_;

        err = av_interleaved_write_frame(oc_, pkt);
        av_packet_free(&pkt);
        check_av(err, "av_interleaved_write_frame");
    }

private:
    AVFormatContext* oc_ = nullptr;
    AVStream* st_ = nullptr;
    bool header_written_ = false;
    AVRational frame_tb_{1, 1};
    int64_t next_frame_ = 0;

    static void check_av(int err, const char* what) {
        if (err >= 0) return;
        char buf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(err, buf, sizeof(buf));
        throw std::runtime_error(std::string(what) + ": " + buf);
    }

    void cleanup() {
        if (!oc_) return;
        if (oc_->pb) {
            avio_closep(&oc_->pb);
        }
        avformat_free_context(oc_);
        oc_ = nullptr;
        st_ = nullptr;
        header_written_ = false;
    }
};
