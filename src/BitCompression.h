#pragma once

#include <iostream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iomanip>
#include <string>

// Helper class to pack bits sequentially into bytes
class BitWriter
{
  std::vector<uint8_t> bytes;
  uint8_t currentByte = 0;
  int bitCount = 0;

public:
  void writeBits(uint64_t val, int numBits)
  {
    for (int i = numBits - 1; i >= 0; --i)
    {
      uint8_t bit = (val >> i) & 1ULL;
      currentByte = (currentByte << 1) | bit;
      bitCount++;
      if (bitCount == 8)
      {
        bytes.push_back(currentByte);
        currentByte = 0;
        bitCount = 0;
      }
    }
  }

  void flush()
  {
    if (bitCount > 0)
    {
      currentByte <<= (8 - bitCount);
      bytes.push_back(currentByte);
      currentByte = 0;
      bitCount = 0;
    }
  }

  const std::vector<uint8_t>& getBytes() const { return bytes; }
};

// Custom hash to use pair/struct keys inside unordered_map
struct MaskHash
{
  std::size_t operator()(uint32_t val) const { return std::hash<uint32_t>{}(val); }
};

class BitStreamAnalyzer
{
private:
  // Helper to count set bits (population count)
  static int popCount(uint32_t v)
  {
    int count = 0;
    while (v)
    {
      count++;
      v &= (v - 1);
    }
    return count;
  }

public:
  // Unpacks raw 8-bit stream down to an individual bit boolean vector
  static std::vector<bool> unpackBytesToBits(const std::vector<int8_t>& inputBytes)
  {
    std::vector<bool> bits;
    bits.reserve(inputBytes.size() * 8);
    for (int8_t b : inputBytes)
    {
      uint8_t byte = static_cast<uint8_t>(b);
      for (int i = 7; i >= 0; --i)
      {
        bits.push_back((byte >> i) & 1);
      }
    }
    return bits;
  }

  template <size_t N>
  static std::vector<bool> unpackBytesToBits(const int8_t (&inputBytes)[N])
  {
    std::vector<bool> bits;
    bits.reserve(N * 8);
    for (size_t idx = 0; idx < N; ++idx)
    {
      uint8_t byte = static_cast<uint8_t>(inputBytes[idx]);
      for (int i = 7; i >= 0; --i)
      {
        bits.push_back((byte >> i) & 1);
      }
    }
    return bits;
  }

    // Runs a target simulation configuration pass with optimal entropy encoding
  static double evaluateMetricsEliasGamma(const std::vector<bool>& bits, int L, int K, bool verbose)
  {
    size_t totalBits = bits.size();
    std::vector<uint64_t> chunks;
    for (size_t i = 0; i <= totalBits - L; i += L)
    {
      uint64_t val = 0;
      for (int b = 0; b < L; ++b)
      {
        val = (val << 1) | (bits[i + b] ? 1ULL : 0ULL);
      }
      chunks.push_back(val);
    }

    if (chunks.empty())
      return 100.0;

    // Step 1: Profile chunk pattern occurrences
    std::unordered_map<uint64_t, size_t> freq;
    for (uint64_t c : chunks)
      freq[c]++;

    std::vector<std::pair<uint64_t, size_t>> sorted(freq.begin(), freq.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<uint64_t> dictionary;
    for (int i = 0; i < K && i < (int)sorted.size(); ++i)
    {
      dictionary.push_back(sorted[i].first);
    }

    while (dictionary.size() < (size_t)K)
    {
      dictionary.push_back(0);
    }

    if (verbose)
    {
      int hexWidth = (L + 3) / 4;
      std::cout << "\n=========================================================\n";
      std::cout << "  OPTIMIZED DICTIONARY TABLE (L = " << L << ", K = " << K << ")\n";
      std::cout << "=========================================================\n";
      std::cout << std::left << std::setw(8) << "Index" << std::setw(hexWidth + 6) << "Hex" << std::setw(L + 3) << "Binary Value" << std::setw(8) << "Hits" << "\n";
      std::cout << "---------------------------------------------------------\n";

      for (size_t i = 0; i < dictionary.size(); ++i)
      {
        uint64_t key = dictionary[i];
        size_t hitCount = freq[key];

        std::string binStr = "";
        for (int b = L - 1; b >= 0; --b)
        {
          binStr += ((key >> b) & 1ULL) ? '1' : '0';
        }

        std::cout << std::left << std::setw(8) << ("[" + std::to_string(i) + "]") << "0x" << std::hex << std::setw(hexWidth) << std::setfill('0') << key << std::dec << std::setfill(' ') << "  "
                  << std::setw(L + 1) << binStr << "  " << std::setw(8) << hitCount << "\n";
      }
      std::cout << "=========================================================\n\n";
    }

    // --- Bit-Packing Stream Writer ---
    std::vector<uint8_t> compressedBytes;
    uint8_t currentByte = 0;
    int bitCount = 0;

    auto writeBits = [&](uint64_t val, int numBits) {
      for (int i = numBits - 1; i >= 0; --i)
      {
        uint8_t bit = (val >> i) & 1ULL;
        currentByte = (currentByte << 1) | bit;
        bitCount++;
        if (bitCount == 8)
        {
          compressedBytes.push_back(currentByte);
          currentByte = 0;
          bitCount = 0;
        }
      }
    };

    // Elias Gamma encoder for frequency-ranked dictionary indices (N = idx + 1 >= 1)
    auto writeEliasGamma = [&](uint64_t n) {
      int highBit = 0;
      uint64_t temp = n;
      while (temp > 1)
      {
        temp >>= 1;
        highBit++;
      }
      // Write highBit zero bits as unary prefix
      for (int i = 0; i < highBit; ++i)
      {
        writeBits(0, 1);
      }
      // Write the full value using (highBit + 1) bits
      writeBits(n, highBit + 1);
    };

    // 1. Write Header: K dictionary entries, each L bits wide
    for (uint64_t key : dictionary)
    {
      writeBits(key, L);
    }

    // 2. Write Payload using Variable-Length Index Encoding
    size_t payloadBits = 0;
    for (uint64_t c : chunks)
    {
      auto it = std::find(dictionary.begin(), dictionary.end(), c);
      if (it != dictionary.end())
      {
        size_t idx = std::distance(dictionary.begin(), it);
        writeBits(1, 1); // [Hit:1]

        // Calculate bit length of Elias Gamma code for metric tracking
        size_t startBytes = compressedBytes.size();
        int startBits = bitCount;

        writeEliasGamma(idx + 1); // Variable-length index based on frequency rank

        size_t endBytes = compressedBytes.size();
        int endBits = bitCount;
        payloadBits += 1 + ((endBytes - startBytes) * 8 + (endBits - startBits));
      }
      else
      {
        writeBits(0, 1); // [Miss:1]
        writeBits(c, L); // [Raw:L]
        payloadBits += 1 + L;
      }
    }

    if (bitCount > 0)
    {
      currentByte <<= (8 - bitCount);
      compressedBytes.push_back(currentByte);
    }

    size_t headerBits = K * L;
    double rate = ((double)(headerBits + payloadBits) / totalBits) * 100.0;

    if (verbose)
    {
      std::cout << "constexpr int8_t compressed[] = {\n    ";
      for (size_t i = 0; i < compressedBytes.size(); ++i)
      {
        std::cout << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(compressedBytes[i]);
        if (i + 1 < compressedBytes.size())
        {
          std::cout << ", ";
        }
        if ((i + 1) % 16 == 0 && i + 1 < compressedBytes.size())
        {
          std::cout << "\n    ";
        }
      }
      std::cout << "\n};\n\n";
      std::cout << std::dec << std::nouppercase;
    }

    return rate;
  }

// Runs a target simulation configuration pass over the uncompressed bit list
  static double evaluateMetrics(const std::vector<bool>& bits, int L, int K, bool verbose)
  {
    size_t totalBits = bits.size();
    std::vector<uint64_t> chunks;
    for (size_t i = 0; i <= totalBits - L; i += L)
    {
      uint64_t val = 0;
      for (int b = 0; b < L; ++b)
      {
        val = (val << 1) | (bits[i + b] ? 1ULL : 0ULL);
      }
      chunks.push_back(val);
    }

    if (chunks.empty())
      return 100.0;

    // Step 1: Profile chunk pattern occurrences
    std::unordered_map<uint64_t, size_t> freq;
    for (uint64_t c : chunks)
      freq[c]++;

    std::vector<std::pair<uint64_t, size_t>> sorted(freq.begin(), freq.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<uint64_t> dictionary;
    for (int i = 0; i < K && i < (int)sorted.size(); ++i)
    {
      dictionary.push_back(sorted[i].first);
    }

    // Pad dictionary if unique chunks are fewer than K
    while (dictionary.size() < (size_t)K)
    {
      dictionary.push_back(0);
    }


    if (verbose)
    {


      // --- Print Formatted Dictionary Table ---
      int hexWidth = (L + 3) / 4; // 1 hex digit per 4 bits
      std::cout << "\n=========================================================\n";
      std::cout << "  DICTIONARY TABLE (L = " << L << ", K = " << K << ")\n";
      std::cout << "=========================================================\n";
      std::cout << std::left << std::setw(8) << "Index" << std::setw(hexWidth + 6) << "Hex" << std::setw(L + 3) << "Binary Value" << std::setw(8) << "Hits" << "\n";
      std::cout << "---------------------------------------------------------\n";

      for (size_t i = 0; i < dictionary.size(); ++i)
      {
        uint64_t key = dictionary[i];
        size_t hitCount = freq[key];

        std::string binStr = "";
        for (int b = L - 1; b >= 0; --b)
        {
          binStr += ((key >> b) & 1ULL) ? '1' : '0';
        }

        std::cout << std::left << std::setw(8) << ("[" + std::to_string(i) + "]") << "0x" << std::hex << std::setw(hexWidth) << std::setfill('0') << key << std::dec << std::setfill(' ') << "  "
                  << std::setw(L + 1) << binStr << "  " << std::setw(8) << hitCount << "\n";
      }
      std::cout << "=========================================================\n\n";
    }
    int k_bits = (K == 4) ? 2 : ((K == 8) ? 3 : 4);

    // --- Serialize to Byte Stream & Calculate Standard Compression Rate ---
    BitWriter writer;

    // 1. Write Header: K dictionary entries, each L bits wide
    for (uint64_t key : dictionary)
    {
      writer.writeBits(key, L);
    }

    // 2. Write Payload: Hits and Misses
    size_t payloadBits = 0;
    for (uint64_t c : chunks)
    {
      auto it = std::find(dictionary.begin(), dictionary.end(), c);
      if (it != dictionary.end())
      {
        size_t idx = std::distance(dictionary.begin(), it);
        writer.writeBits(1, 1);        // [Hit:1]
        writer.writeBits(idx, k_bits); // [Index:k_bits]
        payloadBits += 1 + k_bits;
      }
      else
      {
        writer.writeBits(0, 1); // [Miss:1]
        writer.writeBits(c, L); // [Raw:L]
        payloadBits += 1 + L;
      }
    }
    writer.flush();

    size_t headerBits = K * L;
    double rate = ((double)(headerBits + payloadBits) / totalBits) * 100.0;

    // --- Print Compressed Array in C++ Format ---

    if (verbose)
    {

      const auto& compressedBytes = writer.getBytes();
      std::cout << "constexpr int8_t compressed[] = {\n    ";
      for (size_t i = 0; i < compressedBytes.size(); ++i)
      {
        std::cout << "0x" << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << static_cast<int>(compressedBytes[i]);
        if (i + 1 < compressedBytes.size())
        {
          std::cout << ", ";
        }
        if ((i + 1) % 16 == 0 && i + 1 < compressedBytes.size())
        {
          std::cout << "\n    ";
        }
      }
      std::cout << "\n};\n\n";
      // Reset stream formatting back to default decimal/lowercase states
      std::cout << std::dec << std::nouppercase;
    }
    return rate;
  }


};

