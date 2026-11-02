#include "TruePeak.h"

#include <algorithm>
#include <cmath>

#include "Kaiser.h"

#if defined(__aarch64__) && defined(__ARM_NEON)
 #include <arm_neon.h>
 #define VENEER_DOT_NEON 1
#elif defined(__SSE2__) || defined(_M_X64)
 #include <emmintrin.h>
 #define VENEER_DOT_SSE 1
#endif

namespace veneer
{
namespace
{
constexpr int oversampling = TruePeakDetector::oversampling;
constexpr int tapsPerPhase = TruePeakDetector::tapsPerPhase;
constexpr int numTaps = oversampling * tapsPerPhase + 1;
constexpr double kaiserBeta = 3.0;
constexpr double pi = 3.14159265358979323846;

// Phase zero of this filter is exactly the delayed input, so only phases 1 to 7 are stored.
// Coefficients are ordered oldest sample first to match the contiguous history window.
struct Coefficients
{
    Coefficients()
    {
        const auto centre = (numTaps - 1) / 2;

        for (int m = 0; m < numTaps; ++m)
        {
            const auto phase = m % oversampling;
            const auto k = m / oversampling;

            if (phase == 0 || k >= tapsPerPhase)
                continue;

            const auto offset = m - centre;
            const auto t = (double) offset / oversampling;
            const auto window = kaiser ((double) offset / centre, kaiserBeta);

            table[(size_t) phase][(size_t) (tapsPerPhase - 1 - k)] = (float) (std::sin (pi * t) / (pi * t) * window);
        }
    }

    std::array<std::array<float, tapsPerPhase>, oversampling> table {};
};

const Coefficients& coefficients()
{
    static const Coefficients instance;
    return instance;
}

static_assert (tapsPerPhase % 16 == 0);

// four independent accumulators of four lanes each: one chain of dependent adds would leave the
// multiply-add units idle
float dot (const float* a, const float* b) noexcept
{
#if defined(VENEER_DOT_NEON)
    auto s0 = vdupq_n_f32 (0.0f), s1 = s0, s2 = s0, s3 = s0;

    for (int j = 0; j < tapsPerPhase; j += 16)
    {
        s0 = vfmaq_f32 (s0, vld1q_f32 (a + j), vld1q_f32 (b + j));
        s1 = vfmaq_f32 (s1, vld1q_f32 (a + j + 4), vld1q_f32 (b + j + 4));
        s2 = vfmaq_f32 (s2, vld1q_f32 (a + j + 8), vld1q_f32 (b + j + 8));
        s3 = vfmaq_f32 (s3, vld1q_f32 (a + j + 12), vld1q_f32 (b + j + 12));
    }

    return vaddvq_f32 (vaddq_f32 (vaddq_f32 (s0, s1), vaddq_f32 (s2, s3)));
#elif defined(VENEER_DOT_SSE)
    auto s0 = _mm_setzero_ps(), s1 = s0, s2 = s0, s3 = s0;

    for (int j = 0; j < tapsPerPhase; j += 16)
    {
        s0 = _mm_add_ps (s0, _mm_mul_ps (_mm_loadu_ps (a + j), _mm_loadu_ps (b + j)));
        s1 = _mm_add_ps (s1, _mm_mul_ps (_mm_loadu_ps (a + j + 4), _mm_loadu_ps (b + j + 4)));
        s2 = _mm_add_ps (s2, _mm_mul_ps (_mm_loadu_ps (a + j + 8), _mm_loadu_ps (b + j + 8)));
        s3 = _mm_add_ps (s3, _mm_mul_ps (_mm_loadu_ps (a + j + 12), _mm_loadu_ps (b + j + 12)));
    }

    const auto sum = _mm_add_ps (_mm_add_ps (s0, s1), _mm_add_ps (s2, s3));
    const auto pair = _mm_add_ps (sum, _mm_movehl_ps (sum, sum));
    return _mm_cvtss_f32 (_mm_add_ss (pair, _mm_shuffle_ps (pair, pair, 1)));
#else
    float s[16] = {};

    for (int j = 0; j < tapsPerPhase; j += 16)
        for (int k = 0; k < 16; ++k)
            s[k] += a[j + k] * b[j + k];

    float sum = 0.0f;

    for (int k = 0; k < 16; k += 4)
        sum += (s[k] + s[k + 1]) + (s[k + 2] + s[k + 3]);

    return sum;
#endif
}
}

void TruePeakDetector::reset() noexcept
{
    for (auto& s : state)
        s = {};
}

float TruePeakDetector::process (int channel, float sample) noexcept
{
    auto& s = state[(size_t) channel];
    s.position = (s.position + 1) & (historySize - 1);
    s.history[(size_t) s.position] = sample;
    s.history[(size_t) (s.position + historySize)] = sample;

    // window[tapsPerPhase - 1] is the newest sample
    const float* window = &s.history[(size_t) (s.position + historySize - (tapsPerPhase - 1))];
    const auto& table = coefficients().table;

    auto peak = std::abs (window[tapsPerPhase - 1 - groupDelaySamples]);

    for (int phase = 1; phase < oversampling; ++phase)
        peak = std::max (peak, std::abs (dot (table[(size_t) phase].data(), window)));

    return peak;
}
}
