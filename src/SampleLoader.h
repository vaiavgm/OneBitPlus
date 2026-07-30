#pragma once
#include <vector>
#include <string_view>
#include <fstream>
#include <iterator>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <cstring>

struct WAV_HEADER
{
  uint8_t RIFF[4];
  uint32_t ChunkSize;
  uint8_t WAVE[4];
  uint8_t fmt[4];
  uint32_t Subchunk1Size;
  uint16_t AudioFormat;
  uint16_t NumOfChan;
  uint32_t SamplesPerSec;
  uint32_t bytesPerSec;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
};

struct LoadedSample {
  std::vector<int32_t> sampleBuffer;
  WAV_HEADER header;
  bool isStereo{false};
};

class SampleLoader {
public:
  auto load_file_static(const char* path) const -> LoadedSample;
};

// Helper implementations

static inline void load_samples(const std::vector<uint8_t>& fileBuffer, std::vector<int32_t>& sampleBuffer, const WAV_HEADER& header, size_t dataOffset, size_t dataSize)
{
  const int bytesPerSample = header.bitsPerSample / 8;
  if (bytesPerSample == 0) return;
  const int numSamples = static_cast<int>(dataSize / bytesPerSample);

  sampleBuffer.reserve(numSamples); // PREVENT REALLOCATIONS

  switch (header.bitsPerSample) {
    case 8:
      for (int i = 0; i < numSamples; ++i) {
        int32_t sampleVal = static_cast<int32_t>(fileBuffer[dataOffset + i]) - 128;
        sampleBuffer.push_back(sampleVal << 24); // Scale to 32-bit
      }
      break;

    case 16:
      for (int i = 0; i < numSamples; ++i) {
        int16_t val;
        std::memcpy(&val, &fileBuffer[dataOffset + i * bytesPerSample], sizeof(int16_t));
        sampleBuffer.push_back(static_cast<int32_t>(val) << 16); // Scale to 32-bit
      }
      break;

    case 24:
      for (int i = 0; i < numSamples; ++i) {
        int32_t sampleVal = 0;
        for (int j = 0; j < bytesPerSample; ++j) {
          sampleVal |= static_cast<int32_t>(fileBuffer[dataOffset + i * bytesPerSample + j]) << (j * 8);
        }
        if (sampleVal & 0x800000) {
          sampleVal |= 0xFF000000; 
        }
        sampleBuffer.push_back(sampleVal << 8); // Scale to 32-bit
      }
      break;

    case 32:
      for (int i = 0; i < numSamples; ++i) {
        int32_t val;
        std::memcpy(&val, &fileBuffer[dataOffset + i * bytesPerSample], sizeof(int32_t));
        sampleBuffer.push_back(val); // Already 32-bit
      }
      break;

    default:
      std::cerr << "Unsupported bit depth: " << header.bitsPerSample << std::endl;
      return;
  }
}

static inline bool ends_with(std::string_view value, std::string_view ending)
{
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

static inline void convert_stereo_to_mono_inplace(std::vector<int32_t>& buffer)
{
  if (buffer.empty()) return;
  
  size_t numFrames = buffer.size() / 2;
  for (size_t i = 0; i < numFrames; ++i) {
    // Process in-place to save memory
    buffer[i] = (buffer[i * 2] / 2) + (buffer[(i * 2) + 1] / 2);
  }
  buffer.resize(numFrames); // Shrink the vector down to mono size
}

inline auto SampleLoader::load_file_static(const char* path) const -> LoadedSample
{
  if (!path) return LoadedSample{ std::vector<int32_t>{}, WAV_HEADER{}, false };

  std::string path_str = path;
  for (auto &c : path_str) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
  if (!ends_with(path_str, ".wav")) {
    return LoadedSample{ std::vector<int32_t>{}, WAV_HEADER{}, false };
  }

  std::ifstream input(path, std::ios::binary | std::ios::ate); // Open at end to get size
  if (!input) {
    std::cerr << "Failed to open file: " << path << std::endl;
    return LoadedSample{ std::vector<int32_t>{}, WAV_HEADER{}, false };
  }

  // Fast file reading
  std::streamsize size = input.tellg();
  input.seekg(0, std::ios::beg);
  std::vector<uint8_t> fileBuffer(size);
  if (!input.read(reinterpret_cast<char*>(fileBuffer.data()), size)) {
      std::cerr << "Failed to read file: " << path << std::endl;
      return LoadedSample{ std::vector<int32_t>{}, WAV_HEADER{}, false };
  }
  input.close();

  if (fileBuffer.size() < 12) {
    std::cerr << "File too small or not a valid RIFF: " << path << std::endl;
    return LoadedSample{ std::vector<int32_t>{}, WAV_HEADER{}, false };
  }

  if (!(fileBuffer[0] == 'R' && fileBuffer[1] == 'I' && fileBuffer[2] == 'F' && fileBuffer[3] == 'F') ||
      !(fileBuffer[8] == 'W' && fileBuffer[9] == 'A' && fileBuffer[10] == 'V' && fileBuffer[11] == 'E')) {
    std::cerr << "Not a valid RIFF/WAVE file: " << path << std::endl;
    return LoadedSample{ std::vector<int32_t>{}, WAV_HEADER{}, false };
  }

  auto read_u16 = [&](size_t off)->uint16_t {
    if (off + 1 >= fileBuffer.size()) return 0;
    return static_cast<uint16_t>(fileBuffer[off]) | (static_cast<uint16_t>(fileBuffer[off+1]) << 8);
  };
  
  auto read_u32 = [&](size_t off)->uint32_t {
    if (off + 3 >= fileBuffer.size()) return 0;
    return static_cast<uint32_t>(fileBuffer[off]) | 
          (static_cast<uint32_t>(fileBuffer[off+1]) << 8) | 
          (static_cast<uint32_t>(fileBuffer[off+2]) << 16) | 
          (static_cast<uint32_t>(fileBuffer[off+3]) << 24);
  };

  bool haveFmt = false, haveData = false;
  WAV_HEADER header{};
  size_t dataOffset = 0, dataSize = 0, offset = 12;

  while (offset + 8 <= fileBuffer.size()) {
    char id[5] = {0};
    std::memcpy(id, &fileBuffer[offset], 4);
    uint32_t chunkSize = read_u32(offset + 4);
    size_t chunkData = offset + 8;
    
    if (chunkData + chunkSize > fileBuffer.size()) break;

    if (std::memcmp(id, "fmt ", 4) == 0) {
      haveFmt = true;
      header.Subchunk1Size = chunkSize;
      header.AudioFormat = read_u16(chunkData + 0);
      header.NumOfChan = read_u16(chunkData + 2);
      header.SamplesPerSec = read_u32(chunkData + 4);
      header.bytesPerSec = read_u32(chunkData + 8);
      header.blockAlign = read_u16(chunkData + 12);
      header.bitsPerSample = read_u16(chunkData + 14);
    }
    else if (std::memcmp(id, "data", 4) == 0) {
      haveData = true;
      dataOffset = chunkData;
      dataSize = chunkSize;
    }

    offset = chunkData + chunkSize + (chunkSize & 1);
  }

  if (!haveFmt || !haveData) {
    std::cerr << "Missing fmt or data chunk: " << path << std::endl;
    return LoadedSample{ std::vector<int32_t>{}, WAV_HEADER{}, false };
  }

  bool stereo = (header.NumOfChan == 2);
  if (header.AudioFormat != 1 && header.AudioFormat != 3) {
    std::cerr << "Unsupported WAV audio format: " << header.AudioFormat << std::endl;
    return LoadedSample{ std::vector<int32_t>{}, header, stereo };
  }

  if (dataOffset + dataSize > fileBuffer.size()) {
    dataSize = static_cast<uint32_t>(fileBuffer.size() - dataOffset);
  }

  std::vector<int32_t> sampleBuffer;

  if (header.AudioFormat == 3 && header.bitsPerSample == 32) {
    size_t numSamples = dataSize / 4;
    sampleBuffer.reserve(numSamples);
    for (size_t i = 0; i < numSamples; ++i) {
      float f;
      std::memcpy(&f, &fileBuffer[dataOffset + i * 4], sizeof(float));
      if (f > 1.0f) f = 1.0f;
      if (f < -1.0f) f = -1.0f;
      sampleBuffer.push_back(static_cast<int32_t>(f * 2147483647.0f));
    }
  } else {
    load_samples(fileBuffer, sampleBuffer, header, dataOffset, dataSize);
  }

  if (stereo) {
    convert_stereo_to_mono_inplace(sampleBuffer);
  }

  return LoadedSample{ std::move(sampleBuffer), header, stereo };
}
