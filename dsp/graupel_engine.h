#ifndef GRAUPEL_DSP_GRAUPEL_ENGINE_H_
#define GRAUPEL_DSP_GRAUPEL_ENGINE_H_

// The seam between the plugin and the ported Clouds DSP (ADR 0008).
//
// Everything here is JUCE-free on purpose (ADR 0008): the same translation
// unit is linked into the plugin, render_wav and GraupelDspTests.  The upstream
// headers are kept inside the .cpp so no caller has to know about TEST or the
// eurorack include root.

#include <cstdint>
#include <memory>

namespace graupel {

enum class Mode {
  kGrain = 0,
  kStretch = 1,
  kLoop = 2,
  kSpectral = 3,
};

enum class Fidelity {
  kAuthentic = 0,
  kHq = 1,
};

// One block of parameter values (ADR 0010: they move once per block).
struct Parameters {
  float position = 0.5f;
  float size = 0.5f;
  float pitch = 0.0f;  // semitones, coarse + fine already summed
  float density = 0.55f;
  float texture = 0.5f;
  float dry_wet = 0.5f;
  float stereo_spread = 0.5f;
  float feedback = 0.0f;
  float reverb = 0.0f;

  float input_gain_db = 0.0f;  // -18 .. +6
  bool freeze = false;
  bool summed_input = false;

  Mode mode = Mode::kGrain;
  int quality = 0;  // 0=1 s stereo, 1=2 s mono, 2=4 s stereo, 3=8 s mono
  Fidelity fidelity = Fidelity::kAuthentic;

  // Parameters the DSP owns as meta-parameters but which the shim still fills
  // honestly (ADR 0002/0010). The mode-specific granular/spectral
  // values are overwritten by the upstream processor from density/texture/
  // size in the modes that use them; use_deterministic_seed is the one that
  // is a real input to the grain scheduler.
  bool deterministic_seed = false;
  float granular_overlap = 0.0f;
  float granular_window_shape = 0.5f;
  float granular_stereo_spread = 0.5f;
  float spectral_quantization = 0.5f;
  float spectral_refresh_rate = 0.5f;
  float spectral_phase_randomization = 0.0f;
  float spectral_warp = 0.5f;
};

struct Status {
  float meter_peak = 0.0f;  // 0..1, input normally, output while frozen
  bool frozen = false;
  bool empty = true;  // no audio has been captured since the buffer was cleared
  Mode mode = Mode::kGrain;
  int quality = 0;
  Fidelity fidelity = Fidelity::kAuthentic;
};

class Engine {
 public:
  Engine();
  ~Engine();

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // Called from the host's prepareToPlay (never the audio thread).  A change
  // of sample rate snapshots the Recording Buffer, rebuilds the 32 kHz/HQ
  // banks, and restores it (ADR 0005 applied to rate changes too).
  void Prepare(double sample_rate, int max_block_size);

  // Clears the Recording Buffer and all pipeline state.  Presets are settings
  // only unless Frozen; this is the "new instance" reset.
  void Reset();

  // Stereo in/out. May be called with any block size; internally the DSP is
  // always driven in 32-sample blocks in the Authentic domain (ADR 0012).
  void Process(const float* input_left, const float* input_right,
               float* output_left, float* output_right, int num_frames,
               const Parameters& parameters);

  Status GetStatus() const;
  int LatencySamples() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Soft clip threshold where the hardware meter's top segment lights, derived
// from lut_db and Leds::PaintBar (ADR 0013: the light and the clip point are
// one fact).  Exposed for tests.
float MeterTopThreshold();
float SoftClip(float sample);

}  // namespace graupel

#endif  // GRAUPEL_DSP_GRAUPEL_ENGINE_H_
