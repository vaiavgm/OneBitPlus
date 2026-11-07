#pragma once

// Spectrogram and waveform data for the sample preview (pure functions).

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace Spectrogram
{

// In-place radix-2 FFT (size must be a power of two).
inline void Fft(std::vector<std::complex<double>>& a)
{
  const size_t n = a.size();
  for (size_t i = 1, j = 0; i < n; ++i)
  {
    size_t bit = n >> 1;
    for (; j & bit; bit >>= 1)
      j ^= bit;
    j ^= bit;
    if (i < j)
      std::swap(a[i], a[j]);
  }
  for (size_t len = 2; len <= n; len <<= 1)
  {
    const double angle = -2.0 * 3.14159265358979323846 / static_cast<double>(len);
    const std::complex<double> wlen(std::cos(angle), std::sin(angle));
    for (size_t i = 0; i < n; i += len)
    {
      std::complex<double> w(1.0, 0.0);
      for (size_t k = 0; k < len / 2; ++k)
      {
        const std::complex<double> u = a[i + k], v = a[i + k + len / 2] * w;
        a[i + k] = u + v;
        a[i + k + len / 2] = u - v;
        w *= wlen;
      }
    }
  }
}

struct Data
{
  int cols = 0;                  // time
  int rows = 0;                  // frequency, row 0 = highest frequency (top of the picture)
  std::vector<uint8_t> level;    // cols * rows, 0 = quiet ... 255 = loudest (relative to the peak)
  uint8_t At(int col, int row) const { return level[static_cast<size_t>(col) * rows + row]; }
  bool empty() const { return cols == 0 || rows == 0; }
};

// Spectrogram of a mono signal. Frequencies are log-spaced from minHz to Nyquist (rows), magnitudes are in dB
// relative to the peak and mapped over `dbRange`. At most `maxCols` columns.
inline Data Compute(const std::vector<float>& x, uint32_t sampleRate, int maxCols = 240, int rows = 96, double dbRange = 90.0, double minHz = 30.0, int fftSize = 1024)
{
  Data out;
  if (x.size() < static_cast<size_t>(fftSize) || sampleRate == 0 || maxCols < 1 || rows < 1)
    return out;

  const size_t n = x.size();
  const size_t frames = std::min<size_t>(maxCols, (n - fftSize) / (fftSize / 4) + 1);
  const double hop = frames > 1 ? static_cast<double>(n - fftSize) / static_cast<double>(frames - 1) : 0.0;

  std::vector<double> window(fftSize);
  for (int i = 0; i < fftSize; ++i)
    window[i] = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979323846 * i / (fftSize - 1));

  // bin range of every row (log-spaced frequencies)
  const double nyquist = sampleRate / 2.0;
  const double lo = std::min(minHz, nyquist / 4.0);
  const int bins = fftSize / 2;
  std::vector<int> rowFirst(rows), rowLast(rows);
  for (int r = 0; r < rows; ++r)
  {
    const int rr = rows - 1 - r; // row 0 is the top = highest frequency
    const double f0 = lo * std::pow(nyquist / lo, static_cast<double>(rr) / rows);
    const double f1 = lo * std::pow(nyquist / lo, static_cast<double>(rr + 1) / rows);
    int b0 = static_cast<int>(f0 / nyquist * bins), b1 = static_cast<int>(f1 / nyquist * bins);
    b0 = std::clamp(b0, 1, bins - 1);
    b1 = std::clamp(std::max(b1, b0), b0, bins - 1);
    rowFirst[r] = b0;
    rowLast[r] = b1;
  }

  std::vector<double> mags(frames * rows, 0.0);
  std::vector<std::complex<double>> buf(fftSize);
  double peak = 1e-30;
  for (size_t f = 0; f < frames; ++f)
  {
    const size_t start = static_cast<size_t>(hop * static_cast<double>(f));
    for (int i = 0; i < fftSize; ++i)
      buf[i] = std::complex<double>(x[start + i] * window[i], 0.0);
    Fft(buf);
    for (int r = 0; r < rows; ++r)
    {
      double m = 0.0;
      for (int b = rowFirst[r]; b <= rowLast[r]; ++b)
        m = std::max(m, std::abs(buf[b]));
      mags[f * rows + r] = m;
      peak = std::max(peak, m);
    }
  }

  out.cols = static_cast<int>(frames);
  out.rows = rows;
  out.level.resize(mags.size());
  for (size_t i = 0; i < mags.size(); ++i)
  {
    const double db = 20.0 * std::log10(std::max(mags[i], 1e-30) / peak); // <= 0
    out.level[i] = static_cast<uint8_t>(std::clamp((db + dbRange) / dbRange, 0.0, 1.0) * 255.0 + 0.5);
  }
  return out;
}

// Packed 1-bit data as a +-1 signal, optionally smoothed with a boxcar average (to show the encoded waveform).
inline std::vector<float> DecodeBits(const std::vector<int8_t>& packed, int smoothing = 1)
{
  std::vector<float> bits(packed.size() * 8);
  for (size_t i = 0; i < bits.size(); ++i)
    bits[i] = ((static_cast<uint8_t>(packed[i / 8]) >> (7 - (i % 8))) & 1) ? 1.0f : -1.0f;
  if (smoothing <= 1)
    return bits;

  std::vector<float> out(bits.size());
  double sum = 0.0;
  for (size_t i = 0; i < bits.size(); ++i)
  {
    sum += bits[i];
    if (i >= static_cast<size_t>(smoothing))
      sum -= bits[i - smoothing];
    out[i] = static_cast<float>(sum / std::min<size_t>(i + 1, smoothing));
  }
  return out;
}

struct MinMax
{
  float lo, hi;
};

// min / max of the signal per pixel column (for drawing a waveform).
inline std::vector<MinMax> Envelope(const std::vector<float>& x, int width)
{
  std::vector<MinMax> out(std::max(0, width), MinMax{0.f, 0.f});
  if (x.empty() || width <= 0)
    return out;
  const double per = static_cast<double>(x.size()) / width;
  for (int p = 0; p < width; ++p)
  {
    const size_t a = static_cast<size_t>(p * per);
    const size_t b = std::min(x.size(), std::max(a + 1, static_cast<size_t>((p + 1) * per)));
    float lo = x[a], hi = x[a];
    for (size_t i = a; i < b; ++i)
    {
      lo = std::min(lo, x[i]);
      hi = std::max(hi, x[i]);
    }
    out[p] = {lo, hi};
  }
  return out;
}

} // namespace Spectrogram
