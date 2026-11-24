#ifndef GRAUPEL_DSP_POLYPHASE_RESAMPLER_H_
#define GRAUPEL_DSP_POLYPHASE_RESAMPLER_H_

// Host-rate <-> 32 kHz conversion for the Authentic path (ADR 0012).
//
// This is a rational polyphase FIR resampler: the rate pair is reduced to
// L/M, the input is conceptually upsampled by L, low-pass filtered by a
// Kaiser-windowed sinc prototype, and decimated by M.  It is deliberately not
// a fixed 2x stage: 44100 -> 32000 is 320/441 and 32000 -> 44100 is 441/320,
// both exact, so 44.1 kHz is a first-class case rather than an approximation.
//
// No JUCE.  All memory is allocated in Prepare(); Process() is allocation-free.

#include <cstdint>
#include <vector>

namespace graupel {

class PolyphaseResampler {
 public:
  PolyphaseResampler();

  // taps_per_phase controls the FIR length.  64 gives roughly 80 dB of
  // stop-band rejection with a Kaiser beta of 9 and is cheap at these rates.
  void Prepare(double input_rate, double output_rate, int taps_per_phase = 64);
  void Reset();

  bool bypassed() const { return bypassed_; }

  // Upper bound on frames Process() can write for `input_frames` input frames.
  int MaxOutputFrames(int input_frames) const;

  // Group delay in input samples (the filter is linear phase).
  double GroupDelayInputSamples() const;

  // Stereo, deinterleaved.  Returns the number of frames written.  The caller
  // must provide at least MaxOutputFrames(input_frames) of output capacity.
  int Process(const float* in_l, const float* in_r, int input_frames,
              float* out_l, float* out_r, int output_capacity);

 private:
  std::vector<float> coefficients_;
  std::vector<float> ring_l_;
  std::vector<float> ring_r_;

  int taps_ = 0;
  int l_ = 1;
  int m_ = 1;
  int mask_ = 0;

  uint64_t input_count_ = 0;
  uint64_t base_ = 0;
  uint64_t phase_ = 0;

  bool bypassed_ = true;
};

}  // namespace graupel

#endif  // GRAUPEL_DSP_POLYPHASE_RESAMPLER_H_
