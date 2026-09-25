#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class OutputFormat {
    JPEG,
    PNG
};

OutputFormat detectOutputFormat(const std::string& format);

std::vector<std::uint8_t> processImage(
    const std::uint8_t* data,
    std::size_t size,
    std::uint32_t width,
    std::uint32_t height,
    OutputFormat format
);

void processFile(
    const std::string& inputPath,
    const std::string& outputPath,
    std::uint32_t width,
    std::uint32_t height
);
