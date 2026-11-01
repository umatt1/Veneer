#pragma once

#include <array>
#include <atomic>

namespace veneer
{
struct Meters
{
    std::atomic<float> inputRmsDb { -100.0f };
    std::array<std::atomic<float>, 3> bandGrDb {};
    std::atomic<float> limiterGrDb { 0.0f };
    std::atomic<float> truePeakDb { -100.0f };
    std::atomic<float> momentaryLufs { -100.0f };
    std::atomic<float> shortTermLufs { -100.0f };
    std::atomic<float> integratedLufs { -100.0f };
};
}
