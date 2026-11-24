#include "dsp/polyphase_resampler.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace graupel {
namespace {

constexpr double kPi = 3.14159265358979323846264338327950288;

double BesselI0(double x) {
  // Modified Bessel function of the first kind, order 0.  The series is
  // plenty accurate for the Kaiser parameter range used here.
  double sum = 1.0;
  double term = 1.0;
  const double y = x * x * 0.25;
  for (int k = 1; k < 64; ++k) {
    term *= y / (static_cast<double>(k) * static_cast<double>(k));
    sum += term;
    if (term < sum * 1.0e-16) {
      break;
    }
  }
  return sum;
}

int NextPowerOfTwo(int value) {
  int result = 1;
  while (result < value) {
    result <<= 1;
  }
  return result;
}

}  // namespace

PolyphaseResampler::PolyphaseResampler() = default;

void PolyphaseResampler::Prepare(double input_rate, double output_rate,
                                 int taps_per_phase) {
  Reset();

  const long long in_rate = std::llround(input_rate);
  const long long out_rate = std::llround(output_rate);
  if (in_rate <= 0 || out_rate <= 0 || in_rate == out_rate ||
      taps_per_phase < 4) {
    bypassed_ = true;
    taps_ = 0;
    l_ = 1;
    m_ = 1;
    coefficients_.clear();
    ring_l_.clear();
    ring_r_.clear();
    return;
  }

  bypassed_ = false;
  taps_ = taps_per_phase;

  const long long g = std::gcd(in_rate, out_rate);
  l_ = static_cast<int>(out_rate / g);
  m_ = static_cast<int>(in_rate / g);

  // Cutoff of the ideal low-pass, normalized to the upsampled rate.  The
  // upsample-by-L image spacing means the filter must pass fs_out/2 and stop
  // before fs_in/2.
  const double cutoff =
      0.5 * std::min(1.0, static_cast<double>(l_) / static_cast<double>(m_)) /
      static_cast<double>(l_);
  const double two_pi_cutoff = 2.0 * kPi * cutoff;
  const int length = taps_ * l_;
  const double center = 0.5 * static_cast<double>(length - 1);
  constexpr double kKaiserBeta = 9.0;
  const double i0_beta = BesselI0(kKaiserBeta);

  coefficients_.resize(static_cast<size_t>(length));
  for (int i = 0; i < length; ++i) {
    const double x = static_cast<double>(i) - center;
    const double sinc =
        (std::abs(x) < 1.0e-12) ? 1.0 : std::sin(two_pi_cutoff * x) /
            (two_pi_cutoff * x);
    const double r = (2.0 * x) / static_cast<double>(length - 1);
    const double window =
        BesselI0(kKaiserBeta * std::sqrt(std::max(0.0, 1.0 - r * r))) /
        i0_beta;
    // The L factor restores unity DC gain for each polyphase branch.
    coefficients_[static_cast<size_t>(i)] = static_cast<float>(
        static_cast<double>(l_) * 2.0 * cutoff * sinc * window);
  }

  const int ring_size = NextPowerOfTwo(taps_ + 1);
  mask_ = ring_size - 1;
  ring_l_.assign(static_cast<size_t>(ring_size), 0.0f);
  ring_r_.assign(static_cast<size_t>(ring_size), 0.0f);

  input_count_ = 0;
  base_ = 0;
  phase_ = 0;
}

void PolyphaseResampler::Reset() {
  std::fill(ring_l_.begin(), ring_l_.end(), 0.0f);
  std::fill(ring_r_.begin(), ring_r_.end(), 0.0f);
  input_count_ = 0;
  base_ = 0;
  phase_ = 0;
}

int PolyphaseResampler::MaxOutputFrames(int input_frames) const {
  if (input_frames <= 0) {
    return 0;
  }
  if (bypassed_) {
    return input_frames;
  }
  const int64_t estimate =
      (static_cast<int64_t>(input_frames) * l_) / m_ + 2;
  return static_cast<int>(estimate);
}

double PolyphaseResampler::GroupDelayInputSamples() const {
  if (bypassed_ || taps_ == 0) {
    return 0.0;
  }
  const double center = 0.5 * static_cast<double>(taps_ * l_ - 1);
  return center / static_cast<double>(l_);
}

int PolyphaseResampler::Process(const float* in_l, const float* in_r,
                                int input_frames, float* out_l, float* out_r,
                                int output_capacity) {
  if (input_frames <= 0 || output_capacity <= 0) {
    return 0;
  }
  if (bypassed_) {
    const int frames = std::min(input_frames, output_capacity);
    for (int i = 0; i < frames; ++i) {
      out_l[i] = in_l[i];
      out_r[i] = in_r[i];
    }
    return frames;
  }

  int written = 0;
  for (int n = 0; n < input_frames; ++n) {
    ring_l_[static_cast<size_t>(input_count_ & mask_)] = in_l[n];
    ring_r_[static_cast<size_t>(input_count_ & mask_)] = in_r[n];
    ++input_count_;

    while (base_ < input_count_) {
      if (written >= output_capacity) {
        // The caller must size the output buffer with MaxOutputFrames().  Stop
        // rather than overwriting samples; no input has been consumed past the
        // current frame.
        return written;
      }

      const int q = static_cast<int>(phase_);
      float left = 0.0f;
      float right = 0.0f;
      const int start = static_cast<int>(
          std::min<uint64_t>(base_, static_cast<uint64_t>(taps_ - 1)));
      for (int d = 0; d <= start; ++d) {
        const float coefficient =
            coefficients_[static_cast<size_t>(d * l_ + q)];
        const size_t index = static_cast<size_t>((base_ - d) & mask_);
        left += coefficient * ring_l_[index];
        right += coefficient * ring_r_[index];
      }
      out_l[written] = left;
      out_r[written] = right;
      ++written;

      phase_ += static_cast<uint64_t>(m_);
      base_ += phase_ / static_cast<uint64_t>(l_);
      phase_ %= static_cast<uint64_t>(l_);
    }
  }
  return written;
}

}  // namespace graupel
