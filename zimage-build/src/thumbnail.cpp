#include "thumbnail.h"

#include <vips/vips8>

#include <algorithm>
#include <cctype>
#include <stdexcept>

using namespace vips;

namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });
    return value;
}

void validateDimensions(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0) {
        throw std::runtime_error("width and height must both be greater than zero");
    }
}

VImage resizeExact(const VImage& image, std::uint32_t width, std::uint32_t height) {
    const double scaleX = static_cast<double>(width) / image.width();
    const double scaleY = static_cast<double>(height) / image.height();
    return image.resize(scaleX, VImage::option()
        ->set("vscale", scaleY)
        ->set("kernel", "lanczos3"));
}

VImage flattenForJpeg(VImage image) {
    if (image.has_alpha()) {
        image = image.flatten(VImage::option()
            ->set("background",
                VImage::black(image.width(), image.height())
                    .new_from_image({255, 255, 255})));
    }
    if (image.bands() > 3) {
        image = image.extract_band(0, VImage::option()->set("n", 3));
    }
    return image;
}

} // namespace

OutputFormat detectOutputFormat(const std::string& format) {
    const std::string extension = lower(format);
    return extension == "jpg" || extension == "jpeg"
        ? OutputFormat::JPEG
        : OutputFormat::PNG;
}

std::vector<std::uint8_t> processImage(
    const std::uint8_t* data,
    std::size_t size,
    std::uint32_t width,
    std::uint32_t height,
    OutputFormat format
) {
    validateDimensions(width, height);
    if (data == nullptr || size == 0) {
        throw std::runtime_error("input image buffer is empty");
    }

    try {
        VImage image = VImage::new_from_buffer(data, size, 0, nullptr);
        VImage resized = resizeExact(image, width, height);
        void* output = nullptr;
        size_t outputSize = 0;

        if (format == OutputFormat::JPEG) {
            resized = flattenForJpeg(resized);
            resized.write_to_buffer(".jpg", &output, &outputSize,
                VImage::option()->set("Q", 85));
        } else {
            resized.write_to_buffer(".png", &output, &outputSize);
        }

        if (output == nullptr || outputSize == 0) {
            throw std::runtime_error("libvips returned an empty output image");
        }
        std::vector<std::uint8_t> result(
            static_cast<std::uint8_t*>(output),
            static_cast<std::uint8_t*>(output) + outputSize);
        g_free(output);
        return result;
    } catch (const VError& error) {
        throw std::runtime_error(
            std::string("An error occurred in thumbnailing: ") + error.what());
    }
}

void processFile(
    const std::string& inputPath,
    const std::string& outputPath,
    std::uint32_t width,
    std::uint32_t height
) {
    validateDimensions(width, height);
    try {
        VImage image = VImage::new_from_file(inputPath.c_str(),
            VImage::option()->set("access", "sequential"));
        VImage resized = resizeExact(image, width, height);
        const auto position = outputPath.find_last_of('.');
        const std::string extension = position == std::string::npos
            ? std::string()
            : lower(outputPath.substr(position + 1));

        if (extension == "jpg" || extension == "jpeg") {
            resized = flattenForJpeg(resized);
            resized.write_to_file(outputPath.c_str(),
                VImage::option()->set("Q", 85));
        } else {
            resized.write_to_file(outputPath.c_str());
        }
    } catch (const VError& error) {
        throw std::runtime_error(
            std::string("An error occurred in thumbnailing using file system: ")
            + error.what());
    }
}
