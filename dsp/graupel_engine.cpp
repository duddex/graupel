#include "dsp/graupel_engine.h"

#include "dsp/polyphase_resampler.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>
#include <vector>

// Upstream select their portable, non-STM32 paths with TEST.  The target also
// defines it on these sources; the local definition keeps this file usable in
// a plain translation unit too.
#ifndef TEST
#define TEST
#endif
#include "clouds/dsp/audio_buffer.h"
#include "clouds/dsp/frame.h"
#include "clouds/dsp/granular_processor.h"
#include "clouds/dsp/mu_law.h"
#include "clouds/resources.h"
#include "stmlib/dsp/dsp.h"

namespace graupel {
namespace {

constexpr size_t kAuthenticLargeBytes = 118784;
constexpr size_t kAuthenticSmallBytes = 65536 - 128;
constexpr size_t kWorkspaceBytes = 65536 - 128;
constexpr int kDspBlockSize = 32;
constexpr int kPrimeFrames = 128;
constexpr int kResamplerTaps = 64;

constexpr int32_t kMeterTopSegmentDb = 24576;  // PaintBar: db << 1 >= 49152

size_t Align4(size_t value) { return (value + 3u) & ~size_t(3u); }

int NextPowerOfTwo(int value) {
  int result = 1;
  while (result < value) {
    result <<= 1;
  }
  return result;
}

float Clamp01(float value) {
  return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

double DurationSeconds(int quality) {
  switch (quality & 3) {
    case 0: return 1.0;
    case 1: return 2.0;
    case 2: return 4.0;
    default: return 8.0;
  }
}

int HardwareQuality(int quality, Fidelity fidelity) {
  if (fidelity == Fidelity::kAuthentic) {
    return quality & 3;
  }
  // HQ has no companding and no 16 kHz buffer, so it is always the 16-bit
  // branch of the hardware encoding; the duration is carried by the buffer
  // size, not by PersistentState::quality.
  return (quality & 1) ? 1 : 0;
}

clouds::PlaybackMode ToCloudsMode(Mode mode) {
  switch (mode) {
    case Mode::kStretch: return clouds::PLAYBACK_MODE_STRETCH;
    case Mode::kLoop: return clouds::PLAYBACK_MODE_LOOPING_DELAY;
    case Mode::kSpectral: return clouds::PLAYBACK_MODE_SPECTRAL;
    case Mode::kGrain:
    default: return clouds::PLAYBACK_MODE_GRANULAR;
  }
}

int16_t ToShort(float sample) {
  const float scaled = sample * 32768.0f;
  const int32_t rounded =
      static_cast<int32_t>(scaled >= 0.0f ? scaled + 0.5f : scaled - 0.5f);
  return static_cast<int16_t>(stmlib::Clip16(rounded));
}

// Peak follower with the hardware Meter's attack/release arithmetic.
class HardwareMeter {
 public:
  void Init(double sample_rate) {
    attack_ = std::max(1, static_cast<int32_t>(32768.0 * 100.0 / sample_rate));
    release_ = std::max(1, static_cast<int32_t>(32768.0 * 4.0 / sample_rate));
    peak_l_ = 0;
    peak_r_ = 0;
  }

  void Reset() {
    peak_l_ = 0;
    peak_r_ = 0;
  }

  void Process(const clouds::ShortFrame* frames, int size) {
    for (int i = 0; i < size; ++i) {
      int32_t sample = frames[i].l;
      if (sample < 0) sample = -sample;
      int32_t error = sample - peak_l_;
      const int32_t coefficient = error > 0 ? attack_ : release_;
      peak_l_ += (error * coefficient) >> 15;

      sample = frames[i].r;
      if (sample < 0) sample = -sample;
      error = sample - peak_r_;
      const int32_t coefficient_r = error > 0 ? attack_ : release_;
      peak_r_ += (error * coefficient_r) >> 15;
    }
  }

  float Peak() const {
    const int32_t peak = peak_l_ > peak_r_ ? peak_l_ : peak_r_;
    return static_cast<float>(peak) / 32768.0f;
  }

 private:
  int32_t attack_ = 1;
  int32_t release_ = 1;
  int32_t peak_l_ = 0;
  int32_t peak_r_ = 0;
};

class StereoFifo {
 public:
  void Init(int capacity) {
    const int size = NextPowerOfTwo(std::max(2, capacity));
    left_.assign(static_cast<size_t>(size), 0.0f);
    right_.assign(static_cast<size_t>(size), 0.0f);
    mask_ = static_cast<size_t>(size - 1);
    read_ = 0;
    write_ = 0;
  }

  void Clear() {
    read_ = 0;
    write_ = 0;
  }

  void Push(float left, float right) {
    if (write_ - read_ >= left_.size()) {
      return;  // Should not happen with the prepared capacity.
    }
    left_[write_ & mask_] = left;
    right_[write_ & mask_] = right;
    ++write_;
  }

  bool Pop(float* left, float* right) {
    if (read_ == write_) {
      return false;
    }
    *left = left_[read_ & mask_];
    *right = right_[read_ & mask_];
    ++read_;
    return true;
  }

 private:
  std::vector<float> left_;
  std::vector<float> right_;
  size_t mask_ = 0;
  size_t read_ = 0;
  size_t write_ = 0;
};

struct Bank {
  void* large = nullptr;
  size_t large_size = 0;
  void* small = nullptr;
  size_t small_size = 0;
};

}  // namespace

float MeterTopThreshold() {
  // Leds::PaintBar lights the top segment when lut_db[peak >> 7] >= 24576.
  // Find the first meter index that does so and turn it back into a normalised
  // sample amplitude.  This keeps the meter light and the soft clip tied to
  // one number (ADR 0013).
  for (int i = 0; i < 256; ++i) {
    if (clouds::lut_db[i] >= kMeterTopSegmentDb) {
      return static_cast<float>(i * 128) / 32768.0f;
    }
  }
  return 1.0f;
}

float SoftClip(float sample) {
  static const float threshold = MeterTopThreshold();
  static const float range = 1.0f - threshold;
  const float magnitude = std::fabs(sample);
  if (magnitude <= threshold || range <= 0.0f) {
    return sample;
  }
  const float compressed =
      threshold + range * std::tanh((magnitude - threshold) / range);
  return sample < 0.0f ? -compressed : compressed;
}

struct Engine::Impl {
  // Upstream's DSP was a zero-initialised global on the module.  Give it the
  // same starting state without editing eurorack/.
  alignas(clouds::GranularProcessor)
      unsigned char processor_storage[sizeof(clouds::GranularProcessor)];
  clouds::GranularProcessor* processor = nullptr;

  double sample_rate = 48000.0;
  int max_block = 0;
  bool prepared = false;

  Mode applied_mode = Mode::kGrain;
  int applied_quality = 0;
  Fidelity applied_fidelity = Fidelity::kAuthentic;
  bool current_freeze = false;

  std::vector<uint8_t> authentic_large;
  std::vector<uint8_t> authentic_small;
  std::vector<uint8_t> hq_memory;
  size_t mono_sample_bytes = 0;
  size_t stereo_sample_bytes = 0;

  PolyphaseResampler input_resampler;
  PolyphaseResampler output_resampler;
  int input_capacity = 0;
  int output_capacity = 0;
  int internal_capacity = 0;

  std::vector<float> host_in_left;
  std::vector<float> host_in_right;
  std::vector<float> internal_in_left;
  std::vector<float> internal_in_right;
  std::vector<float> internal_out_left;
  std::vector<float> internal_out_right;
  std::vector<float> resampled_left;
  std::vector<float> resampled_right;

  clouds::ShortFrame in_block[kDspBlockSize];
  clouds::ShortFrame out_block[kDspBlockSize];
  int in_fill = 0;

  std::vector<clouds::ShortFrame> out_queue;
  int out_queue_count = 0;

  StereoFifo output_fifo;
  HardwareMeter meter;
  bool buffer_has_audio = false;

  struct Snapshot {
    bool valid = false;
    clouds::PersistentState state{};
    int channel_count = 0;
    size_t channel_size[2] = {0, 0};
    std::vector<uint8_t> channel[2];
  } snapshot;

  std::vector<uint32_t> restore_words;

  Impl() {
    std::memset(processor_storage, 0, sizeof(processor_storage));
    processor = new (processor_storage) clouds::GranularProcessor();
    authentic_large.resize(kAuthenticLargeBytes);
    authentic_small.resize(kAuthenticSmallBytes);
    snapshot.channel[0].resize(kAuthenticLargeBytes);
    snapshot.channel[1].resize(kAuthenticLargeBytes);
  }

  size_t SampleBytes(int quality) const {
    // HQ is always 16-bit; only the duration (quality) changes the bank size.
    const size_t frames = static_cast<size_t>(
        std::llround(sample_rate * DurationSeconds(quality)));
    return Align4(frames * 2u);
  }

  Bank MakeBank(Fidelity fidelity, int quality) const {
    Bank bank;
    if (fidelity == Fidelity::kAuthentic) {
      bank.large = const_cast<uint8_t*>(authentic_large.data());
      bank.large_size = authentic_large.size();
      bank.small = const_cast<uint8_t*>(authentic_small.data());
      bank.small_size = authentic_small.size();
      return bank;
    }

    const bool mono = (quality & 1) != 0;
    const size_t sample = SampleBytes(quality);
    uint8_t* base = const_cast<uint8_t*>(hq_memory.data());
    if (mono) {
      bank.large = base;
      bank.large_size = sample;
      bank.small = base + sample;
      bank.small_size = kWorkspaceBytes;
    } else {
      bank.large = base;
      bank.large_size = sample + kWorkspaceBytes;
      bank.small = base + sample + kWorkspaceBytes;
      bank.small_size = sample;
    }
    return bank;
  }

  void ConfigureProcessor(Mode mode, int quality, Fidelity fidelity) {
    const Bank bank = MakeBank(fidelity, quality);
    processor->Init(bank.large, bank.large_size, bank.small, bank.small_size);
    if (fidelity == Fidelity::kAuthentic) {
      processor->set_quality(quality & 3);
    } else {
      processor->set_num_channels((quality & 1) ? 1 : 2);
      processor->set_low_fidelity(false);
    }
    processor->set_silence(false);
    processor->set_bypass(false);
    processor->set_playback_mode(ToCloudsMode(mode));
    processor->Prepare();
  }

  void ResetPipeline() {
    in_fill = 0;
    out_queue_count = 0;
    input_resampler.Reset();
    output_resampler.Reset();
    output_fifo.Clear();
    for (int i = 0; i < kPrimeFrames; ++i) {
      output_fifo.Push(0.0f, 0.0f);
    }
    meter.Reset();
  }

  void ApplyParameters(const Parameters& parameters) {
    clouds::Parameters dsp{};
    dsp.position = Clamp01(parameters.position);
    dsp.size = Clamp01(parameters.size);
    dsp.pitch = std::max(-48.0f, std::min(48.0f, parameters.pitch));
    dsp.density = Clamp01(parameters.density);
    dsp.texture = Clamp01(parameters.texture);
    dsp.dry_wet = Clamp01(parameters.dry_wet);
    dsp.stereo_spread = Clamp01(parameters.stereo_spread);
    dsp.feedback = Clamp01(parameters.feedback);
    dsp.reverb = Clamp01(parameters.reverb);
    dsp.freeze = parameters.freeze;
    // ADR 0002: no trigger and no gate in Graupel.
    dsp.trigger = false;
    dsp.gate = false;
    dsp.granular.overlap = Clamp01(parameters.granular_overlap);
    dsp.granular.window_shape = Clamp01(parameters.granular_window_shape);
    dsp.granular.stereo_spread = Clamp01(parameters.granular_stereo_spread);
    dsp.granular.use_deterministic_seed = parameters.deterministic_seed;
    dsp.spectral.quantization = Clamp01(parameters.spectral_quantization);
    dsp.spectral.refresh_rate = Clamp01(parameters.spectral_refresh_rate);
    dsp.spectral.phase_randomization =
        Clamp01(parameters.spectral_phase_randomization);
    dsp.spectral.warp = Clamp01(parameters.spectral_warp);
    *processor->mutable_parameters() = dsp;
    current_freeze = parameters.freeze;
  }

  void TakeSnapshot() {
    if (applied_mode == Mode::kSpectral) {
      return;
    }
    processor->PreparePersistentData();
    clouds::PersistentBlock blocks[4];
    size_t num_blocks = 0;
    processor->GetPersistentData(blocks, &num_blocks);
    if (num_blocks == 0) {
      snapshot.valid = false;
      return;
    }

    snapshot.state = *static_cast<const clouds::PersistentState*>(blocks[0].data);
    snapshot.channel_count = 0;
    snapshot.channel_size[0] = 0;
    snapshot.channel_size[1] = 0;
    for (size_t i = 1; i < num_blocks && snapshot.channel_count < 2; ++i) {
      const size_t size = blocks[i].size;
      if (size > snapshot.channel[snapshot.channel_count].size()) {
        snapshot.valid = false;
        return;
      }
      std::memcpy(snapshot.channel[snapshot.channel_count].data(), blocks[i].data,
                  size);
      snapshot.channel_size[snapshot.channel_count] = size;
      ++snapshot.channel_count;
    }
    snapshot.valid = snapshot.channel_count > 0;
  }

  float ReadSample(const uint8_t* data, int bytes_per_sample, int index) const {
    if (bytes_per_sample == 2) {
      int16_t value = 0;
      std::memcpy(&value, data + static_cast<size_t>(index) * 2u, 2u);
      return static_cast<float>(value) / 32768.0f;
    }
    return static_cast<float>(clouds::MuLaw2Lin(data[index])) / 32768.0f;
  }

  void WriteSample(uint8_t* data, int bytes_per_sample, int index,
                   float value) const {
    const int32_t scaled = static_cast<int32_t>(
        value >= 0.0f ? value * 32768.0f + 0.5f : value * 32768.0f - 0.5f);
    const int16_t clipped = static_cast<int16_t>(stmlib::Clip16(scaled));
    if (bytes_per_sample == 2) {
      std::memcpy(data + static_cast<size_t>(index) * 2u, &clipped, 2u);
    } else {
      data[index] = static_cast<uint8_t>(clouds::Lin2MuLaw(clipped));
    }
  }

  bool RestoreSnapshot(int quality, Fidelity fidelity) {
    // The snapshot is deliberately retained while the processor is spectral:
    // this is the restore half of a Spectral -> Grain transition (ADR 0005).
    if (!snapshot.valid) {
      return false;
    }

    clouds::PersistentBlock blocks[4];
    size_t num_blocks = 0;
    processor->GetPersistentData(blocks, &num_blocks);
    if (num_blocks < 2) {
      return false;
    }

    const int bytes_per_sample_saved =
        (snapshot.state.quality & 2) ? 1 : 2;
    const int bytes_per_sample_target =
        (fidelity == Fidelity::kAuthentic && (quality & 2)) ? 1 : 2;
    const int saved_count =
        static_cast<int>(snapshot.channel_size[0] /
                         static_cast<size_t>(bytes_per_sample_saved)) -
        ::kInterpolationTail;
    const size_t target_block_size = blocks[1].size;
    const int target_count =
        static_cast<int>(target_block_size /
                         static_cast<size_t>(bytes_per_sample_target)) -
        ::kInterpolationTail;
    if (saved_count <= 0 || target_count <= 0) {
      return false;
    }

    const bool fast_path =
        bytes_per_sample_saved == bytes_per_sample_target &&
        saved_count == target_count &&
        snapshot.channel_size[0] == target_block_size &&
        snapshot.channel_count == static_cast<int>(num_blocks - 1);

    clouds::PersistentState state = snapshot.state;
    state.quality = static_cast<uint8_t>(HardwareQuality(quality, fidelity));
    state.spectral = 0;
    if (fast_path) {
      state.write_head[0] =
          std::max(0, std::min(saved_count - 1, state.write_head[0]));
      state.write_head[1] =
          std::max(0, std::min(saved_count - 1, state.write_head[1]));
    } else {
      state.write_head[0] = 0;
      state.write_head[1] = 0;
    }

    size_t word = 0;
    auto put_u32 = [&](uint32_t value) {
      restore_words[word++] = value;
    };
    auto put_bytes = [&](const void* source, size_t bytes) {
      std::memcpy(&restore_words[word], source, bytes);
      word += bytes / sizeof(uint32_t);
    };

    put_u32(blocks[0].tag);
    put_u32(static_cast<uint32_t>(sizeof(state)));
    put_bytes(&state, sizeof(state));

    for (size_t i = 1; i < num_blocks; ++i) {
      const size_t target_size = blocks[i].size;
      const size_t channel = i - 1;
      const size_t source_channel =
          std::min<size_t>(channel, static_cast<size_t>(snapshot.channel_count - 1));
      put_u32(blocks[i].tag);
      put_u32(static_cast<uint32_t>(target_size));
      uint8_t* destination =
          reinterpret_cast<uint8_t*>(&restore_words[word]);

      if (fast_path) {
        std::memcpy(destination, snapshot.channel[source_channel].data(),
                    target_size);
      } else {
        std::memset(destination, 0, target_size);
        const uint8_t* source = snapshot.channel[source_channel].data();
        const int saved_head = snapshot.state.write_head[source_channel];
        for (int j = 0; j < target_count; ++j) {
          const double position =
              static_cast<double>(j) * static_cast<double>(saved_count) /
              static_cast<double>(target_count);
          const int i0 = static_cast<int>(position);
          const double fraction = position - static_cast<double>(i0);
          const int i1 = (i0 + 1) % saved_count;
          const int s0 = (saved_head + i0) % saved_count;
          const int s1 = (saved_head + i1) % saved_count;
          const float value =
              ReadSample(source, bytes_per_sample_saved, s0) *
                  static_cast<float>(1.0 - fraction) +
              ReadSample(source, bytes_per_sample_saved, s1) *
                  static_cast<float>(fraction);
          WriteSample(destination, bytes_per_sample_target, j, value);
        }
        // AudioBuffer keeps an 8-sample interpolation tail duplicating the
        // start of the circular region; rebuild it for a converted bank.
        const int tail = std::min(::kInterpolationTail, target_count);
        for (int j = 0; j < tail; ++j) {
          if (bytes_per_sample_target == 2) {
            int16_t value = 0;
            std::memcpy(&value, destination + static_cast<size_t>(j) * 2u, 2u);
            std::memcpy(destination +
                            static_cast<size_t>(target_count + j) * 2u,
                        &value, 2u);
          } else {
            destination[target_count + j] = destination[j];
          }
        }
      }
      word += target_size / sizeof(uint32_t);
    }

    const bool loaded = processor->LoadPersistentData(restore_words.data());
    if (!loaded) {
      buffer_has_audio = false;
    }
    return loaded;
  }

  void ApplyStructure(const Parameters& parameters) {
    const Mode mode = parameters.mode;
    const int quality = std::max(0, std::min(3, parameters.quality));
    const Fidelity fidelity = parameters.fidelity;

    if (mode == applied_mode && quality == applied_quality &&
        fidelity == applied_fidelity) {
      return;
    }

    const bool to_spectral = mode == Mode::kSpectral;
    const bool from_spectral = applied_mode == Mode::kSpectral;
    const bool reallocates = (to_spectral != from_spectral) ||
                             quality != applied_quality ||
                             fidelity != applied_fidelity;

    if (!reallocates) {
      // A benign change between the three non-spectral modes keeps the buffers.
      processor->set_playback_mode(ToCloudsMode(mode));
      processor->Prepare();
    } else {
      if (!from_spectral) {
        TakeSnapshot();
      }
      ConfigureProcessor(mode, quality, fidelity);
      buffer_has_audio = false;
      if (!to_spectral && snapshot.valid) {
        if (RestoreSnapshot(quality, fidelity)) {
          buffer_has_audio = true;
        }
      }
    }

    applied_mode = mode;
    applied_quality = quality;
    if (fidelity != applied_fidelity) {
      meter.Init(fidelity == Fidelity::kAuthentic ? 32000.0 : sample_rate);
    }
    applied_fidelity = fidelity;
  }

  void PushInternal(float left, float right) {
    in_block[in_fill].l = ToShort(SoftClip(left));
    in_block[in_fill].r = ToShort(SoftClip(right));
    if (!current_freeze &&
        (std::fabs(left) > 1.0e-6f || std::fabs(right) > 1.0e-6f)) {
      buffer_has_audio = true;
    }
    if (++in_fill < kDspBlockSize) {
      return;
    }

    processor->Process(in_block, out_block, kDspBlockSize);
    processor->Prepare();

    meter.Process(current_freeze ? out_block : in_block, kDspBlockSize);
    if (out_queue_count + kDspBlockSize <=
        static_cast<int>(out_queue.size())) {
      for (int i = 0; i < kDspBlockSize; ++i) {
        out_queue[out_queue_count++] = out_block[i];
      }
    }
    in_fill = 0;
  }

  void ProcessSlice(const float* input_left, const float* input_right,
                    float* output_left, float* output_right, int num_frames,
                    const Parameters& parameters) {
    ApplyStructure(parameters);
    ApplyParameters(parameters);

    const float gain = std::pow(10.0f, parameters.input_gain_db / 20.0f);
    const bool summed = parameters.summed_input;
    for (int i = 0; i < num_frames; ++i) {
      float left = input_left[i] * gain;
      float right = input_right[i] * gain;
      if (summed) {
        const float mono = 0.5f * (left + right);
        left = mono;
        right = mono;
      }
      host_in_left[static_cast<size_t>(i)] = left;
      host_in_right[static_cast<size_t>(i)] = right;
    }

    out_queue_count = 0;
    if (applied_fidelity == Fidelity::kAuthentic) {
      const int produced = input_resampler.Process(
          host_in_left.data(), host_in_right.data(), num_frames,
          internal_in_left.data(), internal_in_right.data(), input_capacity);
      for (int i = 0; i < produced; ++i) {
        PushInternal(internal_in_left[static_cast<size_t>(i)],
                     internal_in_right[static_cast<size_t>(i)]);
      }
    } else {
      for (int i = 0; i < num_frames; ++i) {
        PushInternal(host_in_left[static_cast<size_t>(i)],
                     host_in_right[static_cast<size_t>(i)]);
      }
    }

    if (applied_fidelity == Fidelity::kAuthentic) {
      for (int i = 0; i < out_queue_count; ++i) {
        internal_out_left[static_cast<size_t>(i)] =
            static_cast<float>(out_queue[static_cast<size_t>(i)].l) / 32768.0f;
        internal_out_right[static_cast<size_t>(i)] =
            static_cast<float>(out_queue[static_cast<size_t>(i)].r) / 32768.0f;
      }
      const int produced = output_resampler.Process(
          internal_out_left.data(), internal_out_right.data(), out_queue_count,
          resampled_left.data(), resampled_right.data(), output_capacity);
      for (int i = 0; i < produced; ++i) {
        output_fifo.Push(resampled_left[static_cast<size_t>(i)],
                         resampled_right[static_cast<size_t>(i)]);
      }
    } else {
      for (int i = 0; i < out_queue_count; ++i) {
        output_fifo.Push(
            static_cast<float>(out_queue[static_cast<size_t>(i)].l) / 32768.0f,
            static_cast<float>(out_queue[static_cast<size_t>(i)].r) / 32768.0f);
      }
    }

    for (int i = 0; i < num_frames; ++i) {
      float left = 0.0f;
      float right = 0.0f;
      output_fifo.Pop(&left, &right);
      output_left[i] = left;
      output_right[i] = right;
    }
  }
};

Engine::Engine() : impl_(std::make_unique<Impl>()) {}

Engine::~Engine() = default;

void Engine::Prepare(double sample_rate, int max_block_size) {
  Impl& impl = *impl_;
  if (sample_rate <= 0.0) {
    sample_rate = 48000.0;
  }
  impl.max_block = std::max(1, max_block_size);

  const bool rate_changed =
      impl.prepared && std::abs(impl.sample_rate - sample_rate) > 0.01;
  if (impl.prepared && !rate_changed) {
    // Reallocation of scratch for a larger host block; keep the Recording
    // Buffer untouched.
  } else {
    if (rate_changed && impl.applied_mode != Mode::kSpectral) {
      impl.TakeSnapshot();
    }
    impl.sample_rate = sample_rate;

    impl.mono_sample_bytes = Align4(static_cast<size_t>(
        std::llround(sample_rate * 8.0 * 2.0)));
    impl.stereo_sample_bytes = Align4(static_cast<size_t>(
        std::llround(sample_rate * 4.0 * 2.0)));
    const size_t hq_total = std::max(
        impl.mono_sample_bytes + kWorkspaceBytes,
        2u * impl.stereo_sample_bytes + kWorkspaceBytes);
    impl.hq_memory.assign(hq_total, 0);

    const size_t snapshot_bytes = std::max(
        {impl.authentic_large.size(), impl.mono_sample_bytes,
         impl.stereo_sample_bytes});
    impl.snapshot.channel[0].assign(snapshot_bytes, 0);
    impl.snapshot.channel[1].assign(snapshot_bytes, 0);
    impl.restore_words.assign(hq_total / sizeof(uint32_t) + 32, 0);

    impl.input_resampler.Prepare(sample_rate, 32000.0, kResamplerTaps);
    impl.output_resampler.Prepare(32000.0, sample_rate, kResamplerTaps);
  }

  const double ratio_pull = 32000.0 / sample_rate;
  const int internal_from_resampler =
      static_cast<int>(std::ceil(impl.max_block * ratio_pull)) + 64;
  impl.internal_capacity =
      std::max(impl.max_block, internal_from_resampler);
  impl.input_capacity =
      std::max(1, impl.input_resampler.MaxOutputFrames(impl.max_block));
  impl.output_capacity = std::max(
      1, impl.output_resampler.MaxOutputFrames(impl.internal_capacity) + 32);

  impl.host_in_left.assign(static_cast<size_t>(impl.max_block), 0.0f);
  impl.host_in_right.assign(static_cast<size_t>(impl.max_block), 0.0f);
  impl.internal_in_left.assign(static_cast<size_t>(impl.input_capacity), 0.0f);
  impl.internal_in_right.assign(static_cast<size_t>(impl.input_capacity), 0.0f);
  impl.internal_out_left.assign(static_cast<size_t>(impl.internal_capacity),
                                0.0f);
  impl.internal_out_right.assign(static_cast<size_t>(impl.internal_capacity),
                                 0.0f);
  impl.out_queue.assign(static_cast<size_t>(impl.internal_capacity),
                        clouds::ShortFrame{0, 0});
  impl.resampled_left.assign(static_cast<size_t>(impl.output_capacity), 0.0f);
  impl.resampled_right.assign(static_cast<size_t>(impl.output_capacity), 0.0f);
  impl.output_fifo.Init(impl.max_block * 2 + 4096);

  if (!impl.prepared) {
    impl.ConfigureProcessor(impl.applied_mode, impl.applied_quality,
                            impl.applied_fidelity);
    impl.prepared = true;
    impl.ResetPipeline();
  } else if (rate_changed) {
    impl.ConfigureProcessor(impl.applied_mode, impl.applied_quality,
                            impl.applied_fidelity);
    if (impl.applied_mode != Mode::kSpectral && impl.snapshot.valid) {
      impl.RestoreSnapshot(impl.applied_quality, impl.applied_fidelity);
    }
    impl.ResetPipeline();
  } else {
    impl.ResetPipeline();
  }

  impl.meter.Init(impl.applied_fidelity == Fidelity::kAuthentic ? 32000.0
                                                                : sample_rate);
}

void Engine::Reset() {
  Impl& impl = *impl_;
  impl.snapshot.valid = false;
  impl.buffer_has_audio = false;
  impl.ConfigureProcessor(impl.applied_mode, impl.applied_quality,
                          impl.applied_fidelity);
  impl.ResetPipeline();
  impl.meter.Init(impl.applied_fidelity == Fidelity::kAuthentic ? 32000.0
                                                                : impl.sample_rate);
}

void Engine::Process(const float* input_left, const float* input_right,
                     float* output_left, float* output_right, int num_frames,
                     const Parameters& parameters) {
  Impl& impl = *impl_;
  if (!impl.prepared || num_frames <= 0) {
    for (int i = 0; i < num_frames; ++i) {
      output_left[i] = 0.0f;
      output_right[i] = 0.0f;
    }
    return;
  }

  int offset = 0;
  while (offset < num_frames) {
    const int count = std::min(impl.max_block, num_frames - offset);
    impl.ProcessSlice(input_left + offset, input_right + offset,
                      output_left + offset, output_right + offset, count,
                      parameters);
    offset += count;
  }
}

Status Engine::GetStatus() const {
  const Impl& impl = *impl_;
  Status status;
  status.meter_peak = impl.meter.Peak();
  status.frozen = impl.current_freeze;
  status.empty = !impl.buffer_has_audio;
  status.mode = impl.applied_mode;
  status.quality = impl.applied_quality;
  status.fidelity = impl.applied_fidelity;
  return status;
}

int Engine::LatencySamples() const {
  const Impl& impl = *impl_;
  if (!impl.prepared) {
    return 0;
  }
  const double resample_ratio = impl.sample_rate / 32000.0;
  if (impl.applied_fidelity == Fidelity::kAuthentic) {
    const double input_delay = impl.input_resampler.GroupDelayInputSamples();
    const double block_delay = kDspBlockSize * resample_ratio;
    const double output_delay =
        impl.output_resampler.GroupDelayInputSamples() * resample_ratio;
    return static_cast<int>(std::lround(
        kPrimeFrames + input_delay + block_delay + output_delay));
  }
  return kPrimeFrames + kDspBlockSize;
}

}  // namespace graupel
