// WAV-in / WAV-out harness for the ported Clouds DSP.
//
// Started from eurorack/clouds/test/clouds_test.cc: same WAV header writer and
// same 32 kHz / 32-sample-block loop. The WAV writer and loop are adapted from
// that file, Copyright 2014 Emilie Gillet, MIT licensed; the full notice stays
// with the upstream file in the read-only submodule (ADR 0001).
// This is not the plugin and links no JUCE (ADR 0008).
//
// An input WAV is played through GranularProcessor exactly as a host would feed
// audio. It is NOT loaded into the Recording Buffer: the DSP's own capture path
// writes the signal it receives, and grains are drawn from that recording
// (ADR 0003).

#include "dsp/msvc_compat.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// Upstream headers select their portable, non-STM32 implementation with TEST.
// Keep the definition local to this include so it cannot leak into the harness
// (and so it stays out of any GoogleTest surface; we use plain asserts).
#define TEST
#include "clouds/dsp/granular_processor.h"
#undef TEST

namespace {

constexpr uint32_t kSampleRate = 32000;
constexpr size_t kBlockSize = 32;

static_assert(sizeof(clouds::ShortFrame) == 4,
              "ShortFrame must be two packed 16-bit samples");

struct WavInfo {
  uint16_t format = 0;
  uint16_t channels = 0;
  uint32_t sample_rate = 0;
  uint16_t bits_per_sample = 0;
  uint32_t data_size = 0;
  long data_offset = 0;
};

uint16_t le16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) |
      static_cast<uint16_t>(p[1]) << 8;
}

uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
      static_cast<uint32_t>(p[1]) << 8 |
      static_cast<uint32_t>(p[2]) << 16 |
      static_cast<uint32_t>(p[3]) << 24;
}

bool read_exact(FILE* fp, void* dst, size_t bytes) {
  return std::fread(dst, 1, bytes, fp) == bytes;
}

bool parse_wav(FILE* fp, WavInfo* info, std::string* error) {
  uint8_t header[12];
  if (!read_exact(fp, header, sizeof(header))) {
    *error = "input is shorter than a RIFF/WAVE header";
    return false;
  }
  if (std::memcmp(header, "RIFF", 4) != 0 ||
      std::memcmp(header + 8, "WAVE", 4) != 0) {
    *error = "input is not a RIFF/WAVE file";
    return false;
  }

  bool have_fmt = false;
  bool have_data = false;
  for (;;) {
    uint8_t chunk[8];
    size_t got = std::fread(chunk, 1, sizeof(chunk), fp);
    if (got == 0) {
      break;
    }
    if (got != sizeof(chunk)) {
      *error = "truncated WAV chunk header";
      return false;
    }

    const uint32_t size = le32(chunk + 4);
    const long body = std::ftell(fp);
    if (body < 0) {
      *error = "could not determine WAV chunk offset";
      return false;
    }

    if (std::memcmp(chunk, "fmt ", 4) == 0) {
      if (size < 16) {
        *error = "WAV fmt chunk is too small";
        return false;
      }
      std::vector<uint8_t> fmt(size);
      if (!read_exact(fp, fmt.data(), fmt.size())) {
        *error = "truncated WAV fmt chunk";
        return false;
      }
      info->format = le16(fmt.data());
      info->channels = le16(fmt.data() + 2);
      info->sample_rate = le32(fmt.data() + 4);
      info->bits_per_sample = le16(fmt.data() + 14);
      have_fmt = true;
    } else if (std::memcmp(chunk, "data", 4) == 0) {
      info->data_offset = body;
      info->data_size = size;
      have_data = true;
    }

    // Chunks are word-aligned: skip the pad byte after an odd-sized chunk.
    const long next = body + static_cast<long>(size) + static_cast<long>(size & 1u);
    if (std::fseek(fp, next, SEEK_SET) != 0) {
      *error = "could not seek through WAV chunks";
      return false;
    }
  }

  if (!have_fmt) {
    *error = "WAV has no fmt chunk";
    return false;
  }
  if (!have_data) {
    *error = "WAV has no data chunk";
    return false;
  }
  if (info->format != 1) {
    *error = "only uncompressed PCM WAV is supported (format 1)";
    return false;
  }
  if (info->bits_per_sample != 16) {
    *error = "only 16-bit PCM WAV is supported";
    return false;
  }
  if (info->channels != 1 && info->channels != 2) {
    *error = "only mono or stereo WAV is supported";
    return false;
  }
  if (info->sample_rate != kSampleRate) {
    *error =
        "input is " + std::to_string(info->sample_rate) +
        " Hz; this harness feeds the DSP's native 32000 Hz domain and does "
        "not resample (ADR 0012 owns host-rate conversion). Resample the "
        "input to 32000 Hz first.";
    return false;
  }
  if (info->data_size == 0) {
    *error = "WAV data chunk is empty";
    return false;
  }
  return true;
}

// Same header layout as eurorack/clouds/test/clouds_test.cc, generalised to
// the actual frame count and always stereo output.
void write_wav_header(FILE* fp, uint32_t num_frames, uint16_t num_channels) {
  uint32_t l;
  uint16_t s;

  std::fwrite("RIFF", 4, 1, fp);
  l = 36 + num_frames * 2 * num_channels;
  std::fwrite(&l, 4, 1, fp);
  std::fwrite("WAVE", 4, 1, fp);

  std::fwrite("fmt ", 4, 1, fp);
  l = 16;
  std::fwrite(&l, 4, 1, fp);
  s = 1;
  std::fwrite(&s, 2, 1, fp);
  s = num_channels;
  std::fwrite(&s, 2, 1, fp);
  l = kSampleRate;
  std::fwrite(&l, 4, 1, fp);
  l = kSampleRate * 2 * num_channels;
  std::fwrite(&l, 4, 1, fp);
  s = static_cast<uint16_t>(2 * num_channels);
  std::fwrite(&s, 2, 1, fp);
  s = 16;
  std::fwrite(&s, 2, 1, fp);

  std::fwrite("data", 4, 1, fp);
  l = num_frames * 2 * num_channels;
  std::fwrite(&l, 4, 1, fp);
}

class WavReader {
 public:
  WavReader(FILE* fp, const WavInfo& info)
      : fp_(fp), info_(info), remaining_(info.data_size) {
    std::fseek(fp_, info_.data_offset, SEEK_SET);
  }

  // Fills up to `frames` stereo frames, duplicating mono input to both sides.
  size_t read(clouds::ShortFrame* out, size_t frames) {
    const size_t frame_bytes = static_cast<size_t>(info_.channels) * 2;
    const size_t available = remaining_ / frame_bytes;
    if (frames > available) {
      frames = available;
    }
    if (frames == 0) {
      return 0;
    }

    if (info_.channels == 2) {
      const size_t got = std::fread(out, sizeof(clouds::ShortFrame), frames, fp_);
      remaining_ -= static_cast<uint32_t>(got * sizeof(clouds::ShortFrame));
      return got;
    }

    std::vector<int16_t> mono(frames);
    const size_t got = std::fread(mono.data(), sizeof(int16_t), frames, fp_);
    for (size_t i = 0; i < got; ++i) {
      out[i].l = mono[i];
      out[i].r = mono[i];
    }
    remaining_ -= static_cast<uint32_t>(got * sizeof(int16_t));
    return got;
  }

 private:
  FILE* fp_;
  WavInfo info_;
  uint32_t remaining_;
};

struct Options {
  std::string input;
  std::string output;
  float position = 0.5f;
  float size = 0.5f;
  float pitch = 0.0f;
  // Harness demo defaults. The product defaults are Density just past centre
  // and Dry/Wet centred; these are denser/wetter so a default render has
  // obvious grains.
  float density = 0.9f;
  float texture = 0.5f;
  float dry_wet = 0.75f;
  float stereo_spread = 0.5f;
  float feedback = 0.0f;
  float reverb = 0.0f;
  int quality = 0;
  bool freeze = false;
  bool help = false;
};

const char* quality_name(int quality) {
  switch (quality) {
    case 0: return "1 s stereo";
    case 1: return "2 s mono";
    case 2: return "4 s stereo";
    case 3: return "8 s mono";
    default: return "?";
  }
}

void print_usage() {
  std::printf(
      "Usage: render_wav --input IN.wav --output OUT.wav [options]\n"
      "\n"
      "Input must be 16-bit PCM WAV at 32000 Hz, mono or stereo. This harness\n"
      "feeds the DSP's native 32 kHz domain; it does not resample (ADR 0012).\n"
      "The input is played through the effect, not loaded into the buffer\n"
      "(ADR 0003). Output is 32000 Hz, 16-bit stereo.\n"
      "\n"
      "Options:\n"
      "  --position <0..1>       Position in the Recording Buffer (default 0.5)\n"
      "  --size <0..1>           Grain size (default 0.5)\n"
      "  --pitch <semitones>     Grain transposition (default 0)\n"
      "  --density <0..1>        Grain density (default 0.9 for this harness)\n"
      "  --texture <0..1>        Grain envelope (default 0.5)\n"
      "  --dry-wet <0..1>        Dry/wet (default 0.75 for this harness)\n"
      "  --stereo-spread <0..1>  Stereo spread (default 0.5)\n"
      "  --feedback <0..1>       Feedback (default 0)\n"
      "  --reverb <0..1>         Reverberation (default 0)\n"
      "  --quality <0..3>        0=1 s stereo, 1=2 s mono, 2=4 s stereo, 3=8 s mono\n"
      "                          (default 0 = 1 s stereo)\n"
      "  --freeze                Freeze from the first block (empty buffer => silent wet path)\n"
      "  --help                  Show this text\n");
}

bool parse_float(const char* text, float* value) {
  char* end = nullptr;
  const float parsed = std::strtof(text, &end);
  if (end == text || *end != '\0' || !std::isfinite(parsed)) {
    return false;
  }
  *value = parsed;
  return true;
}

bool parse_int(const char* text, int* value) {
  char* end = nullptr;
  const long parsed = std::strtol(text, &end, 10);
  if (end == text || *end != '\0') {
    return false;
  }
  *value = static_cast<int>(parsed);
  return true;
}

bool in_unit_interval(float value) {
  return value >= 0.0f && value <= 1.0f;
}

bool parse_args(int argc, char** argv, Options* options, std::string* error) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto need_value = [&](const char* name) -> const char* {
      if (i + 1 >= argc) {
        *error = std::string(name) + " needs a value";
        return nullptr;
      }
      return argv[++i];
    };

    if (arg == "--help" || arg == "-h") {
      options->help = true;
    } else if (arg == "--input") {
      const char* value = need_value("--input");
      if (!value) return false;
      options->input = value;
    } else if (arg == "--output") {
      const char* value = need_value("--output");
      if (!value) return false;
      options->output = value;
    } else if (arg == "--freeze") {
      options->freeze = true;
    } else if (arg == "--position" || arg == "--size" ||
               arg == "--pitch" || arg == "--density" ||
               arg == "--texture" || arg == "--dry-wet" ||
               arg == "--stereo-spread" || arg == "--feedback" ||
               arg == "--reverb") {
      const char* value = need_value(arg.c_str());
      if (!value) return false;
      float parsed = 0.0f;
      if (!parse_float(value, &parsed)) {
        *error = "invalid number for " + arg + ": " + value;
        return false;
      }
      if (arg == "--position") options->position = parsed;
      else if (arg == "--size") options->size = parsed;
      else if (arg == "--pitch") options->pitch = parsed;
      else if (arg == "--density") options->density = parsed;
      else if (arg == "--texture") options->texture = parsed;
      else if (arg == "--dry-wet") options->dry_wet = parsed;
      else if (arg == "--stereo-spread") options->stereo_spread = parsed;
      else if (arg == "--feedback") options->feedback = parsed;
      else if (arg == "--reverb") options->reverb = parsed;
    } else if (arg == "--quality") {
      const char* value = need_value("--quality");
      if (!value) return false;
      if (!parse_int(value, &options->quality) ||
          options->quality < 0 || options->quality > 3) {
        *error = "--quality must be one of 0, 1, 2, 3";
        return false;
      }
    } else {
      *error = "unknown option: " + arg;
      return false;
    }
  }

  if (options->help) {
    return true;
  }
  if (options->input.empty()) {
    *error = "--input is required";
    return false;
  }
  if (options->output.empty()) {
    *error = "--output is required";
    return false;
  }
  if (options->input == options->output) {
    *error = "--input and --output must be different files";
    return false;
  }

  const float unit_values[] = {
      options->position, options->size, options->density, options->texture,
      options->dry_wet, options->stereo_spread, options->feedback,
      options->reverb};
  const char* unit_names[] = {
      "--position", "--size", "--density", "--texture",
      "--dry-wet", "--stereo-spread", "--feedback", "--reverb"};
  for (size_t i = 0; i < sizeof(unit_values) / sizeof(unit_values[0]); ++i) {
    if (!in_unit_interval(unit_values[i])) {
      *error = std::string(unit_names[i]) + " must be in 0..1";
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  Options options;
  std::string error;
  if (!parse_args(argc, argv, &options, &error)) {
    std::fprintf(stderr, "render_wav: %s\n", error.c_str());
    std::fprintf(stderr, "Try --help.\n");
    return 2;
  }
  if (options.help) {
    print_usage();
    return 0;
  }

  FILE* fp_in = std::fopen(options.input.c_str(), "rb");
  if (!fp_in) {
    std::fprintf(stderr, "render_wav: cannot open input: %s\n",
                 options.input.c_str());
    return 1;
  }

  WavInfo info;
  if (!parse_wav(fp_in, &info, &error)) {
    std::fprintf(stderr, "render_wav: %s: %s\n", options.input.c_str(),
                 error.c_str());
    std::fclose(fp_in);
    return 1;
  }

  const uint32_t frame_bytes = static_cast<uint32_t>(info.channels) * 2;
  const uint32_t total_frames = info.data_size / frame_bytes;

  FILE* fp_out = std::fopen(options.output.c_str(), "wb");
  if (!fp_out) {
    std::fprintf(stderr, "render_wav: cannot create output: %s\n",
                 options.output.c_str());
    std::fclose(fp_in);
    return 1;
  }
  write_wav_header(fp_out, total_frames, 2);

  // Authentic memory budget: 118784 + (65536 - 128) bytes, as upstream. We
  // allocate it; the processor never allocates (ADR 0004).
  std::vector<uint8_t> large_buffer(118784);
  std::vector<uint8_t> small_buffer(65536 - 128);

  auto processor = std::make_unique<clouds::GranularProcessor>();
  processor->Init(large_buffer.data(), large_buffer.size(),
                  small_buffer.data(), small_buffer.size());
  processor->set_quality(options.quality);
  processor->set_silence(false);
  processor->set_bypass(false);
  processor->set_playback_mode(clouds::PLAYBACK_MODE_GRANULAR);

  // Prepare() clears reset_buffers_ and can clear freeze on a reallocation, so
  // fill the parameters after it.
  processor->Prepare();

  clouds::Parameters parameters{};
  parameters.position = options.position;
  parameters.size = options.size;
  parameters.pitch = options.pitch;
  parameters.density = options.density;
  parameters.texture = options.texture;
  parameters.dry_wet = options.dry_wet;
  parameters.stereo_spread = options.stereo_spread;
  parameters.feedback = options.feedback;
  parameters.reverb = options.reverb;
  parameters.freeze = options.freeze;
  parameters.trigger = false;
  parameters.gate = false;
  parameters.granular.overlap = 0.0f;
  parameters.granular.window_shape = 0.5f;
  parameters.granular.stereo_spread = options.stereo_spread;
  parameters.granular.use_deterministic_seed = false;
  parameters.spectral.quantization = 0.5f;
  parameters.spectral.refresh_rate = 0.5f;
  parameters.spectral.phase_randomization = 0.0f;
  parameters.spectral.warp = 0.5f;
  *processor->mutable_parameters() = parameters;

  WavReader reader(fp_in, info);
  std::vector<clouds::ShortFrame> input(kBlockSize);
  std::vector<clouds::ShortFrame> output(kBlockSize);
  uint32_t frames_written = 0;
  bool write_failed = false;

  for (;;) {
    const size_t got = reader.read(input.data(), kBlockSize);
    if (got == 0) {
      break;
    }
    processor->Process(input.data(), output.data(), got);
    processor->Prepare();
    if (std::fwrite(output.data(), sizeof(clouds::ShortFrame), got, fp_out) !=
        got) {
      write_failed = true;
      break;
    }
    frames_written += static_cast<uint32_t>(got);
  }

  std::fclose(fp_in);

  if (frames_written != total_frames) {
    // Keep the header honest if the input was truncated mid-render.
    if (std::fseek(fp_out, 0, SEEK_SET) == 0) {
      write_wav_header(fp_out, frames_written, 2);
    }
  }
  if (std::fclose(fp_out) != 0) {
    write_failed = true;
  }

  if (write_failed) {
    std::fprintf(stderr, "render_wav: failed while writing %s\n",
                 options.output.c_str());
    return 1;
  }

  std::printf(
      "render_wav: %s -> %s\n"
      "  %u frames (%.3f s), 32000 Hz 16-bit stereo\n"
      "  mode Grain, quality %s, density %.2f, dry/wet %.2f, pitch %.2f\n",
      options.input.c_str(), options.output.c_str(), frames_written,
      static_cast<double>(frames_written) / kSampleRate,
      quality_name(options.quality), options.density, options.dry_wet,
      options.pitch);
  if (options.freeze) {
    std::fprintf(stderr,
                 "render_wav: warning: --freeze from the first block leaves "
                 "the Recording Buffer empty, so the wet path is silent; dry "
                 "still passes at the Dry/Wet setting.\n");
  }
  return 0;
}
