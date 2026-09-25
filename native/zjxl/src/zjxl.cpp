#include <napi.h>

#include <jxl/decode.h>
#include <jxl/thread_parallel_runner.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {

class DecoderDeleter {
public:
    void operator()(JxlDecoder* decoder) const noexcept {
        if (decoder) {
            JxlDecoderDestroy(decoder);
        }
    }
};

class RunnerDeleter {
public:
    void operator()(void* runner) const noexcept {
        if (runner) {
            JxlThreadParallelRunnerDestroy(runner);
        }
    }
};

using DecoderPtr =
    std::unique_ptr<JxlDecoder, DecoderDeleter>;

using RunnerPtr =
    std::unique_ptr<void, RunnerDeleter>;

struct DecodedImage {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<uint8_t> pixels;
};

static void throwError(
    const Napi::Env& env,
    const std::string& message
) {
    Napi::Error::New(env, message).ThrowAsJavaScriptException();
}

static DecoderPtr createDecoder(
    const Napi::Env& env
) {
    DecoderPtr decoder(
        JxlDecoderCreate(nullptr)
    );

    if (!decoder) {
        throwError(
            env,
            "JxlDecoderCreate failed"
        );

        return nullptr;
    }

    return decoder;
}

static RunnerPtr createRunner(
    const Napi::Env& env
) {
    unsigned int threads =
        std::thread::hardware_concurrency();

    if (threads == 0) {
        threads = 1;
    }

    /*
     * Avoid creating an excessive number of workers.
     */
    threads = std::min(
        threads,
        32u
    );

    RunnerPtr runner(
        JxlThreadParallelRunnerCreate(
            nullptr,
            threads
        )
    );

    if (!runner) {
        throwError(
            env,
            "JxlThreadParallelRunnerCreate failed"
        );

        return nullptr;
    }

    return runner;
}

static DecodedImage decodeJxl(
    const Napi::Env& env,
    const uint8_t* data,
    size_t size
) {
    if (!data || size == 0) {
        throwError(
            env,
            "JPEG XL input is empty"
        );

        return {};
    }

    DecoderPtr decoder =
        createDecoder(env);

    if (!decoder) {
        return {};
    }

    RunnerPtr runner =
        createRunner(env);

    if (!runner) {
        return {};
    }

    JxlDecoderStatus status;

    status =
        JxlDecoderSetParallelRunner(
            decoder.get(),
            JxlThreadParallelRunner,
            runner.get()
        );

    if (status != JXL_DEC_SUCCESS) {
        throwError(
            env,
            "JxlDecoderSetParallelRunner failed"
        );

        return {};
    }

    status =
        JxlDecoderSubscribeEvents(
            decoder.get(),
            JXL_DEC_BASIC_INFO |
            JXL_DEC_FRAME |
            JXL_DEC_FULL_IMAGE
        );

    if (status != JXL_DEC_SUCCESS) {
        throwError(
            env,
            "JxlDecoderSubscribeEvents failed"
        );

        return {};
    }

    status =
        JxlDecoderSetKeepOrientation(
            decoder.get(),
            JXL_TRUE
        );

    if (status != JXL_DEC_SUCCESS) {
        throwError(
            env,
            "JxlDecoderSetKeepOrientation failed"
        );

        return {};
    }

    /*
     * Always request RGBA8.
     */
    JxlPixelFormat pixelFormat{};

    pixelFormat.num_channels = 4;
    pixelFormat.data_type = JXL_TYPE_UINT8;
    pixelFormat.endianness = JXL_NATIVE_ENDIAN;
    pixelFormat.align = 0;

    JxlDecoderSetInput(
        decoder.get(),
        data,
        size
    );

    JxlDecoderCloseInput(
        decoder.get()
    );

    JxlBasicInfo basicInfo{};

    bool haveBasicInfo = false;
    bool decoded = false;

    std::vector<uint8_t> pixels;

    while (true) {
        status =
            JxlDecoderProcessInput(
                decoder.get()
            );

        switch (status) {
        case JXL_DEC_BASIC_INFO: {
            status =
                JxlDecoderGetBasicInfo(
                    decoder.get(),
                    &basicInfo
                );

            if (status != JXL_DEC_SUCCESS) {
                throwError(
                    env,
                    "JxlDecoderGetBasicInfo failed"
                );

                return {};
            }

            if (
                basicInfo.xsize == 0 ||
                basicInfo.ysize == 0
            ) {
                throwError(
                    env,
                    "JPEG XL image has invalid dimensions"
                );

                return {};
            }

            constexpr uint64_t MAX_PIXELS =
                268435456ULL;

            const uint64_t width =
                static_cast<uint64_t>(
                    basicInfo.xsize
                );

            const uint64_t height =
                static_cast<uint64_t>(
                    basicInfo.ysize
                );

            const uint64_t pixelCount =
                width * height;

            if (pixelCount > MAX_PIXELS) {
                throwError(
                    env,
                    "JPEG XL image is too large"
                );

                return {};
            }

            const uint64_t byteCount =
                pixelCount * 4ULL;

            if (
                byteCount >
                static_cast<uint64_t>(
                    std::numeric_limits<size_t>::max()
                )
            ) {
                throwError(
                    env,
                    "JPEG XL image size overflow"
                );

                return {};
            }

            pixels.resize(
                static_cast<size_t>(
                    byteCount
                )
            );

            haveBasicInfo = true;

            break;
        }

        case JXL_DEC_NEED_IMAGE_OUT_BUFFER: {
            if (!haveBasicInfo) {
                throwError(
                    env,
                    "Decoder requested output before BASIC_INFO"
                );

                return {};
            }

            size_t requiredSize = 0;

            status =
                JxlDecoderImageOutBufferSize(
                    decoder.get(),
                    &pixelFormat,
                    &requiredSize
                );

            if (status != JXL_DEC_SUCCESS) {
                throwError(
                    env,
                    "JxlDecoderImageOutBufferSize failed"
                );

                return {};
            }

            if (requiredSize == 0) {
                throwError(
                    env,
                    "JPEG XL returned an empty output buffer"
                );

                return {};
            }

            if (requiredSize != pixels.size()) {
                pixels.resize(requiredSize);
            }

            status =
                JxlDecoderSetImageOutBuffer(
                    decoder.get(),
                    &pixelFormat,
                    pixels.data(),
                    pixels.size()
                );

            if (status != JXL_DEC_SUCCESS) {
                throwError(
                    env,
                    "JxlDecoderSetImageOutBuffer failed"
                );

                return {};
            }

            break;
        }

        case JXL_DEC_FULL_IMAGE:
            decoded = true;

            /*
             * For the Zalo image use case we only need the first
             * decoded image.
             */
            break;

        case JXL_DEC_SUCCESS:
            if (!decoded) {
                throwError(
                    env,
                    "JPEG XL image was not decoded"
                );

                return {};
            }

            {
                DecodedImage result;

                result.width =
                    static_cast<uint32_t>(
                        basicInfo.xsize
                    );

                result.height =
                    static_cast<uint32_t>(
                        basicInfo.ysize
                    );

                result.pixels =
                    std::move(pixels);

                return result;
            }

        case JXL_DEC_NEED_MORE_INPUT:
            throwError(
                env,
                "Incomplete JPEG XL input"
            );

            return {};

        case JXL_DEC_ERROR:
            throwError(
                env,
                "Invalid or unsupported JPEG XL image"
            );

            return {};

        case JXL_DEC_COLOR_ENCODING:
        case JXL_DEC_FRAME:
        case JXL_DEC_PREVIEW_IMAGE:
        case JXL_DEC_JPEG_RECONSTRUCTION:
        case JXL_DEC_BOX:
            /*
             * Nothing required for our RGBA8 decoder.
             */
            break;

        default:
            break;
        }
    }
}

static Napi::Value Decode(
    const Napi::CallbackInfo& info
) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(
            env,
            "decode() requires a Buffer"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    if (!info[0].IsBuffer()) {
        Napi::TypeError::New(
            env,
            "decode() expects a Buffer"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    Napi::Buffer<uint8_t> input =
        info[0].As<Napi::Buffer<uint8_t>>();

    DecodedImage image =
        decodeJxl(
            env,
            input.Data(),
            input.Length()
        );

    if (env.IsExceptionPending()) {
        return env.Undefined();
    }

    Napi::Object result =
        Napi::Object::New(env);

    result.Set(
        "width",
        Napi::Number::New(
            env,
            image.width
        )
    );

    result.Set(
        "height",
        Napi::Number::New(
            env,
            image.height
        )
    );

    result.Set(
        "channels",
        Napi::Number::New(
            env,
            4
        )
    );

    result.Set(
        "bitsPerSample",
        Napi::Number::New(
            env,
            8
        )
    );

    result.Set(
        "data",
        Napi::Buffer<uint8_t>::Copy(
            env,
            image.pixels.data(),
            image.pixels.size()
        )
    );

    return result;
}

static Napi::Value IsJxl(
    const Napi::CallbackInfo& info
) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(
            env,
            "isJxl() requires a Buffer"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    if (!info[0].IsBuffer()) {
        Napi::TypeError::New(
            env,
            "isJxl() expects a Buffer"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    Napi::Buffer<uint8_t> input =
        info[0].As<Napi::Buffer<uint8_t>>();

    JxlSignature signature =
        JxlSignatureCheck(
            input.Data(),
            input.Length()
        );

    return Napi::Boolean::New(
        env,
        signature == JXL_SIG_CODESTREAM ||
        signature == JXL_SIG_CONTAINER
    );
}

static Napi::Value GetInfo(
    const Napi::CallbackInfo& info
) {
    Napi::Env env = info.Env();

    if (info.Length() < 1) {
        Napi::TypeError::New(
            env,
            "getInfo() requires a Buffer"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    if (!info[0].IsBuffer()) {
        Napi::TypeError::New(
            env,
            "getInfo() expects a Buffer"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    Napi::Buffer<uint8_t> input =
        info[0].As<Napi::Buffer<uint8_t>>();

    if (input.Length() == 0) {
        Napi::TypeError::New(
            env,
            "JPEG XL input is empty"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    DecoderPtr decoder =
        createDecoder(env);

    if (!decoder) {
        return env.Undefined();
    }

    JxlDecoderStatus status =
        JxlDecoderSubscribeEvents(
            decoder.get(),
            JXL_DEC_BASIC_INFO
        );

    if (status != JXL_DEC_SUCCESS) {
        Napi::Error::New(
            env,
            "JxlDecoderSubscribeEvents failed"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    JxlDecoderSetInput(
        decoder.get(),
        input.Data(),
        input.Length()
    );

    JxlDecoderCloseInput(
        decoder.get()
    );

    JxlBasicInfo basicInfo{};

    while (true) {
        status =
            JxlDecoderProcessInput(
                decoder.get()
            );

        if (status == JXL_DEC_BASIC_INFO) {
            break;
        }

        if (status == JXL_DEC_ERROR) {
            Napi::Error::New(
                env,
                "Invalid JPEG XL image"
            ).ThrowAsJavaScriptException();

            return env.Undefined();
        }

        if (status == JXL_DEC_NEED_MORE_INPUT) {
            Napi::Error::New(
                env,
                "Incomplete JPEG XL input"
            ).ThrowAsJavaScriptException();

            return env.Undefined();
        }

        if (status == JXL_DEC_SUCCESS) {
            Napi::Error::New(
                env,
                "JPEG XL basic information not found"
            ).ThrowAsJavaScriptException();

            return env.Undefined();
        }
    }

    status =
        JxlDecoderGetBasicInfo(
            decoder.get(),
            &basicInfo
        );

    if (status != JXL_DEC_SUCCESS) {
        Napi::Error::New(
            env,
            "JxlDecoderGetBasicInfo failed"
        ).ThrowAsJavaScriptException();

        return env.Undefined();
    }

    Napi::Object result =
        Napi::Object::New(env);

    result.Set(
        "width",
        Napi::Number::New(
            env,
            basicInfo.xsize
        )
    );

    result.Set(
        "height",
        Napi::Number::New(
            env,
            basicInfo.ysize
        )
    );

    result.Set(
        "bitsPerSample",
        Napi::Number::New(
            env,
            basicInfo.bits_per_sample
        )
    );

    result.Set(
        "exponentBitsPerSample",
        Napi::Number::New(
            env,
            basicInfo.exponent_bits_per_sample
        )
    );

    result.Set(
        "numColorChannels",
        Napi::Number::New(
            env,
            basicInfo.num_color_channels
        )
    );

    result.Set(
        "numExtraChannels",
        Napi::Number::New(
            env,
            basicInfo.num_extra_channels
        )
    );

    result.Set(
        "haveAnimation",
        Napi::Boolean::New(
            env,
            basicInfo.have_animation
        )
    );

    result.Set(
        "usesOriginalProfile",
        Napi::Boolean::New(
            env,
            basicInfo.uses_original_profile
        )
    );

    return result;
}

static Napi::Value Version(
    const Napi::CallbackInfo& info
) {
    Napi::Env env = info.Env();

    return Napi::Number::New(
        env,
        JxlDecoderVersion()
    );
}

static Napi::Object Init(
    Napi::Env env,
    Napi::Object exports
) {
    exports.Set(
        "decode",
        Napi::Function::New(
            env,
            Decode
        )
    );

    exports.Set(
        "isJxl",
        Napi::Function::New(
            env,
            IsJxl
        )
    );

    exports.Set(
        "getInfo",
        Napi::Function::New(
            env,
            GetInfo
        )
    );

    exports.Set(
        "version",
        Napi::Function::New(
            env,
            Version
        )
    );

    return exports;
}

NODE_API_MODULE(
    zjxl,
    Init
)

} // namespace