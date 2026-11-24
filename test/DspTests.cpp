#include "dsp/graupel_engine.h"
#include "dsp/polyphase_resampler.h"

#include <cassert>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#define TEST
#include "clouds/dsp/mu_law.h"
#undef TEST

namespace {
void test_mu_law_round_trip() {
  int worst = 0;
  for (int value = -32768; value <= 32767; value += 257) {
    const auto encoded = clouds::Lin2MuLaw(static_cast<int16_t>(value));
    const int decoded = clouds::MuLaw2Lin(encoded);
    worst = std::max(worst, std::abs(decoded - value));
  }
  assert(worst < 2500);
}

void test_resampler_44100_is_first_class() {
  graupel::PolyphaseResampler resampler;
  resampler.Prepare(44100.0, 32000.0, 64);
  std::vector<float> in_l(4410, 0.0f), in_r(4410, 0.0f);
  std::vector<float> out_l(static_cast<size_t>(resampler.MaxOutputFrames(4410)));
  std::vector<float> out_r(out_l.size());
  in_l[0] = in_r[0] = 1.0f;
  const int produced = resampler.Process(in_l.data(), in_r.data(), 4410,
                                         out_l.data(), out_r.data(),
                                         static_cast<int>(out_l.size()));
  assert(produced >= 3190 && produced <= 3210);
  assert(resampler.GroupDelayInputSamples() > 0.0);
}
}

int main() {
  assert(graupel::MeterTopThreshold() > 0.0f);
  assert(graupel::MeterTopThreshold() < 1.0f);
  assert(std::fabs(graupel::SoftClip(0.25f) - 0.25f) < 1.0e-6f);
  assert(graupel::SoftClip(2.0f) < 1.0f);
  test_mu_law_round_trip();
  test_resampler_44100_is_first_class();

  constexpr int kFrames = 32000;
  std::vector<float> input(kFrames, 0.0f);
  std::vector<float> left(kFrames, 0.0f);
  std::vector<float> right(kFrames, 0.0f);
  for (int i = 0; i < kFrames; ++i) {
    input[static_cast<size_t>(i)] = 0.2f * std::sin(i * 0.07f);
  }

  graupel::Engine engine;
  engine.Prepare(44100.0, 257);
  graupel::Parameters p;
  p.density = 0.9f;
  p.dry_wet = 1.0f;
  engine.Process(input.data(), input.data(), left.data(), right.data(), kFrames, p);
  p.freeze = true;
  engine.Process(input.data(), input.data(), left.data(), right.data(), 257, p);
  const auto before = engine.GetStatus();
  assert(before.frozen && !before.empty);

  p.mode = graupel::Mode::kSpectral;
  engine.Process(input.data(), input.data(), left.data(), right.data(), 257, p);
  p.mode = graupel::Mode::kGrain;
  engine.Process(input.data(), input.data(), left.data(), right.data(), 257, p);
  const auto after = engine.GetStatus();
  assert(after.frozen && !after.empty);

  // Quality is the duration choice; HQ changes fidelity without changing it.
  p.mode = graupel::Mode::kGrain;
  p.quality = 3;
  p.fidelity = graupel::Fidelity::kHq;
  engine.Process(input.data(), input.data(), left.data(), right.data(), 257, p);
  const auto hq = engine.GetStatus();
  assert(hq.quality == 3 && hq.fidelity == graupel::Fidelity::kHq);

  // Summed Input is independent of the mono-buffer Quality dimension.
  p.summed_input = true;
  p.quality = 1;
  engine.Process(input.data(), input.data(), left.data(), right.data(), 257, p);
  assert(engine.GetStatus().quality == 1);
  return 0;
}
