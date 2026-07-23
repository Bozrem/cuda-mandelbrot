#pragma once

#include "NvEncoderCuda.h"
#include "MatroskaMuxSink.hpp"
#include "config.hpp"
#include "cuda_util.h"

#include <vector>
#include <memory>
#include <stdexcept>

class NvencSink {
private:
    std::unique_ptr<NvEncoderCuda> encoder;
    std::unique_ptr<MatroskaMuxSink> mux_;

    // HDR10 static metadata (SDK 13.1.15 first-class structs). Built once in the ctor;
    // pointers must stay valid across EncodeFrame because lookahead can defer the encode.
    // Enable hevcConfig.outputMasteringDisplay / outputMaxCll, then attach via pic params.
    MASTERING_DISPLAY_INFO mastering_display_ = {};
    CONTENT_LIGHT_LEVEL content_light_level_ = {};

public:
    NvencSink(CUcontext cu_ctx, const char* out_path) {
        mux_ = std::make_unique<MatroskaMuxSink>(out_path, WIDTH, HEIGHT, FPS);

        encoder = std::make_unique<NvEncoderCuda>(
            cu_ctx, WIDTH, HEIGHT, NV_ENC_BUFFER_FORMAT_YUV420_10BIT
        );

        NV_ENC_INITIALIZE_PARAMS init_params = { NV_ENC_INITIALIZE_PARAMS_VER };
        NV_ENC_CONFIG enc_config = { NV_ENC_CONFIG_VER };
        init_params.encodeConfig = &enc_config;

        // HEVC P5 Preset
        encoder->CreateDefaultEncoderParams(
            &init_params,
            NV_ENC_CODEC_HEVC_GUID,
            NV_ENC_PRESET_P5_GUID,
            NV_ENC_TUNING_INFO_HIGH_QUALITY // Quality over latency
        );

        init_params.encodeWidth = WIDTH;
        init_params.encodeHeight = HEIGHT;
        init_params.frameRateNum = FPS;
        init_params.frameRateDen = 1;

        // Rate Control
        enc_config.rcParams.rateControlMode = NV_ENC_PARAMS_RC_VBR; // Variable Bitrate
        enc_config.rcParams.averageBitRate = 80000000; // 80 Mbps
        enc_config.rcParams.maxBitRate = 120000000; // 120 Mbps
        // SDK 13.1.15 renamed MULTI_PASS_* -> TWO_PASS_*
        enc_config.rcParams.multiPass = NV_ENC_TWO_PASS_QUARTER_RESOLUTION;

        // Quality tuning — fractal boundary is dense high-frequency detail.
        enc_config.rcParams.enableAQ = 1;
        enc_config.rcParams.aqStrength = 8;
        enc_config.rcParams.enableTemporalAQ = 1;
        enc_config.rcParams.enableLookahead = 1;
        enc_config.rcParams.lookaheadDepth = 20; // Range is 0-(31 - #B frames); 20 is fine for offline

        // No B-frames: pts == dts == monotonic AU index for the muxer.
        enc_config.frameIntervalP = 1;

        // Profile lives on NV_ENC_CONFIG, not hevcConfig (SDK 13.x layout change).
        enc_config.profileGUID = NV_ENC_HEVC_PROFILE_MAIN10_GUID;

        // HEVC settings
        auto& hevc_config = enc_config.encodeCodecConfig.hevcConfig;
        // pixelBitDepthMinus8 was replaced by inputBitDepth/outputBitDepth enums.
        // CreateDefaultEncoderParams already sets these for YUV420_10BIT; set explicitly anyway.
        hevc_config.inputBitDepth = NV_ENC_BIT_DEPTH_10;
        hevc_config.outputBitDepth = NV_ENC_BIT_DEPTH_10;
        hevc_config.tier = NV_ENC_TIER_HEVC_HIGH;
        // Enable SDK-authored HDR10 SEI (preferred over hand-rolled seiPayloadArray).
        hevc_config.outputMasteringDisplay = 1;
        hevc_config.outputMaxCll = 1;

        // VUI HDR Flags — fields are typed enums in 13.1.15 (bare ints no longer convert).
        hevc_config.hevcVUIParameters.videoSignalTypePresentFlag = 1;
        hevc_config.hevcVUIParameters.videoFormat = NV_ENC_VUI_VIDEO_FORMAT_UNSPECIFIED; // was bare 5
        hevc_config.hevcVUIParameters.videoFullRangeFlag = 0; // TV limits (64-940)

        hevc_config.hevcVUIParameters.colourDescriptionPresentFlag = 1;
        hevc_config.hevcVUIParameters.colourPrimaries = NV_ENC_VUI_COLOR_PRIMARIES_BT2020;
        hevc_config.hevcVUIParameters.transferCharacteristics = NV_ENC_VUI_TRANSFER_CHARACTERISTIC_SMPTE2084;
        hevc_config.hevcVUIParameters.colourMatrix = NV_ENC_VUI_MATRIX_COEFFS_BT2020_NCL;

        build_hdr_metadata();

        // Lock in the configuration and allocate hardware surfaces
        encoder->CreateEncoder(&init_params);

        std::vector<uint8_t> seq;
        encoder->GetSequenceParams(seq);
        mux_->set_extradata(seq.data(), seq.size());
    }

    void queue_frame(const p010_frame_t* d_p010) {
        // Fetch pre-allocated, pre-registered hardware surface pointer
        const NvEncInputFrame* f = encoder->GetNextInputFrame();

        const uint32_t width_bytes = WIDTH * sizeof(uint16_t); // rows are the same byte width for luma and interleaved chroma

        // 2D copies map my tightly packed frame onto NVENC's pitched surface.
        // Use the offsets/pitch NVENC hands us rather than assuming the plane layout.
        CUDA_CHECK(cudaMemcpy2D(
            f->inputPtr, f->pitch,
            d_p010->y,   width_bytes,
            width_bytes, HEIGHT,
            cudaMemcpyDeviceToDevice
        ));
        CUDA_CHECK(cudaMemcpy2D(
            reinterpret_cast<uint8_t*>(f->inputPtr) + f->chromaOffsets[0], f->chromaPitch,
            d_p010->uv,  width_bytes,
            width_bytes, CHROMA_HEIGHT,
            cudaMemcpyDeviceToDevice
        ));

        // Attach HDR10 static metadata via first-class pic-param pointers.
        NV_ENC_PIC_PARAMS pic_params = { NV_ENC_PIC_PARAMS_VER };
        pic_params.codecPicParams.hevcPicParams.pMasteringDisplay = &mastering_display_;
        pic_params.codecPicParams.hevcPicParams.pMaxCll = &content_light_level_;

        std::vector<NvEncOutputFrame> packets;
        encoder->EncodeFrame(packets, &pic_params);
        write_packets(packets);
    }

    ~NvencSink() {
        if (encoder) {
            std::vector<NvEncOutputFrame> packets;
            encoder->EndEncode(packets); // Flushes out remaining frames
            write_packets(packets);
            encoder->DestroyEncoder();
            encoder.reset();
        }
        // Trailer after all AUs are written.
        mux_.reset();
    }

private:
    void build_hdr_metadata() {
        // Chromaticity in units of 0.00002; primaries ordered G, B, R (HEVC / SDK struct order).
        // BT.2020 primaries + D65 white point.
        mastering_display_.g = { 8500, 39850 };   // (0.170, 0.797)
        mastering_display_.b = { 6550,  2300 };   // (0.131, 0.046)
        mastering_display_.r = { 35400, 14600 };  // (0.708, 0.292)
        mastering_display_.whitePoint = { 15635, 16450 }; // D65 (0.3127, 0.3290)
        // Luminance in units of 0.0001 cd/m^2. apply_pq() maps channel 1.0 -> 1000 nits.
        mastering_display_.maxLuma = 10000000; // 1000.0000 cd/m^2
        mastering_display_.minLuma = 1;        // 0.0001 cd/m^2

        content_light_level_.maxContentLightLevel = 1000;  // MaxCLL
        content_light_level_.maxPicAverageLightLevel = 400; // MaxFALL (estimate)
    }

    static bool is_keyframe(const NvEncOutputFrame& packet) {
        return packet.pictureType == NV_ENC_PIC_TYPE_IDR
            || packet.pictureType == NV_ENC_PIC_TYPE_I;
    }

    void write_packets(const std::vector<NvEncOutputFrame>& packets) {
        for (const auto& packet : packets) {
            mux_->write_au(
                packet.frame.data(),
                packet.frame.size(),
                is_keyframe(packet)
            );
        }
    }
};
