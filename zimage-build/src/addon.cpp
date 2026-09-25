#include "thumbnail.h"

#include <napi.h>
#include <vips/vips8>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class ThumbnailWorker final : public Napi::AsyncWorker {
public:
    ThumbnailWorker(Napi::Function callback, std::vector<std::uint8_t> input,
        std::uint32_t width, std::uint32_t height, OutputFormat format)
        : Napi::AsyncWorker(callback), input_(std::move(input)),
          width_(width), height_(height), format_(format) {}

    void Execute() override {
        try {
            output_ = processImage(input_.data(), input_.size(), width_, height_, format_);
        } catch (const std::exception& error) {
            SetError(error.what());
        }
    }

    void OnOK() override {
        Callback().Call({Env().Null(),
            Napi::Buffer<std::uint8_t>::Copy(Env(), output_.data(), output_.size())});
    }

private:
    std::vector<std::uint8_t> input_;
    std::vector<std::uint8_t> output_;
    std::uint32_t width_;
    std::uint32_t height_;
    OutputFormat format_;
};

class ThumbnailFsWorker final : public Napi::AsyncWorker {
public:
    ThumbnailFsWorker(Napi::Function callback, std::string inputPath,
        std::string outputPath, std::uint32_t width, std::uint32_t height)
        : Napi::AsyncWorker(callback), inputPath_(std::move(inputPath)),
          outputPath_(std::move(outputPath)), width_(width), height_(height) {}

    void Execute() override {
        try {
            processFile(inputPath_, outputPath_, width_, height_);
        } catch (const std::exception& error) {
            SetError(error.what());
        }
    }

    void OnOK() override {
        Callback().Call({Env().Null(), Napi::Buffer<std::uint8_t>::New(Env(), 0)});
    }

private:
    std::string inputPath_;
    std::string outputPath_;
    std::uint32_t width_;
    std::uint32_t height_;
};

bool readDimensions(const Napi::Object& options, std::uint32_t& width,
    std::uint32_t& height, Napi::Env env) {
    if (!options.Has("width") || !options.Has("height")) {
        Napi::TypeError::New(env, "width and height are required")
            .ThrowAsJavaScriptException();
        return false;
    }
    width = options.Get("width").ToNumber().Uint32Value();
    height = options.Get("height").ToNumber().Uint32Value();
    return true;
}

Napi::Value Thumbnail(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() != 2 || !info[0].IsObject() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "thumbnail(options, callback)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    const Napi::Object options = info[0].As<Napi::Object>();
    if (!options.Has("buffer") || !options.Get("buffer").IsBuffer()) {
        Napi::TypeError::New(env, "options.buffer must be a Buffer")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (!readDimensions(options, width, height, env)) return env.Undefined();
    std::string format;
    if (options.Has("format") && options.Get("format").IsString()) {
        format = options.Get("format").As<Napi::String>().Utf8Value();
    }
    const auto buffer = options.Get("buffer").As<Napi::Buffer<std::uint8_t>>();
    auto* worker = new ThumbnailWorker(info[1].As<Napi::Function>(),
        std::vector<std::uint8_t>(buffer.Data(), buffer.Data() + buffer.Length()),
        width, height, detectOutputFormat(format));
    worker->Queue();
    return env.Undefined();
}

Napi::Value ThumbnailFs(const Napi::CallbackInfo& info) {
    const Napi::Env env = info.Env();
    if (info.Length() != 2 || !info[0].IsObject() || !info[1].IsFunction()) {
        Napi::TypeError::New(env, "thumbnailFs(options, callback)")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    const Napi::Object options = info[0].As<Napi::Object>();
    if (!options.Has("inputPath") || !options.Get("inputPath").IsString()
        || !options.Has("outputPath") || !options.Get("outputPath").IsString()) {
        Napi::TypeError::New(env, "inputPath and outputPath must be strings")
            .ThrowAsJavaScriptException();
        return env.Undefined();
    }
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    if (!readDimensions(options, width, height, env)) return env.Undefined();
    auto* worker = new ThumbnailFsWorker(info[1].As<Napi::Function>(),
        options.Get("inputPath").As<Napi::String>().Utf8Value(),
        options.Get("outputPath").As<Napi::String>().Utf8Value(),
        width, height);
    worker->Queue();
    return env.Undefined();
}

} // namespace

Napi::Object Init(Napi::Env env, Napi::Object exports) {
    if (VIPS_INIT("zimage") != 0) {
        Napi::Error::New(env, "Failed to initialize libvips")
            .ThrowAsJavaScriptException();
        return exports;
    }
    exports.Set("thumbnail", Napi::Function::New(env, Thumbnail));
    exports.Set("thumbnailFs", Napi::Function::New(env, ThumbnailFs));
    return exports;
}

NODE_API_MODULE(zimage, Init)
