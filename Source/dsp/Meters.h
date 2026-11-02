#pragma once

#include <array>
#include <atomic>

namespace veneer
{
struct Meters
{
    static constexpr float silenceDb = -100.0f;

    std::atomic<float> inputRmsDb { silenceDb };
    std::array<std::atomic<float>, 3> bandGrDb {};
    std::atomic<float> limiterGrDb { 0.0f };
    std::atomic<float> truePeakDb { silenceDb };
    std::atomic<float> momentaryLufs { silenceDb };
    std::atomic<float> shortTermLufs { silenceDb };
    std::atomic<float> integratedLufs { silenceDb };

    // the fields above hold the last block; these hold the largest value since takeHeld(), so a reader that
    // polls slower than the block rate still sees every peak
    struct Held
    {
        std::array<float, 3> bandGrDb {};
        float limiterGrDb = 0.0f, truePeakDb = silenceDb;
    };

    void hold (const Held& block) noexcept
    {
        for (size_t b = 0; b < heldBandGrDb.size(); ++b)
            raise (heldBandGrDb[b], block.bandGrDb[b]);

        raise (heldLimiterGrDb, block.limiterGrDb);
        raise (heldTruePeakDb, block.truePeakDb);
    }

    Held takeHeld() noexcept
    {
        Held taken;

        for (size_t b = 0; b < heldBandGrDb.size(); ++b)
            taken.bandGrDb[b] = heldBandGrDb[b].exchange (0.0f, std::memory_order_relaxed);

        taken.limiterGrDb = heldLimiterGrDb.exchange (0.0f, std::memory_order_relaxed);
        taken.truePeakDb = heldTruePeakDb.exchange (silenceDb, std::memory_order_relaxed);
        return taken;
    }

private:
    static void raise (std::atomic<float>& target, float value) noexcept
    {
        auto current = target.load (std::memory_order_relaxed);

        while (value > current && ! target.compare_exchange_weak (current, value, std::memory_order_relaxed))
        {}
    }

    std::array<std::atomic<float>, 3> heldBandGrDb {};
    std::atomic<float> heldLimiterGrDb { 0.0f }, heldTruePeakDb { silenceDb };
};
}
