#pragma once

#include <cstdint>
#include <vector>

namespace veneer
{
class SlidingMin
{
public:
    void prepare (int windowSize)
    {
        window = windowSize;
        capacity = windowSize + 1;
        values.assign ((size_t) capacity, 1.0f);
        indices.assign ((size_t) capacity, 0);
        reset();
    }

    void reset() noexcept
    {
        head = 0;
        count = 0;
        counter = 0;
    }

    float push (float value) noexcept
    {
        while (count > 0 && values[(size_t) back()] >= value)
            --count;

        const auto slot = (size_t) ((head + count) % capacity);
        values[slot] = value;
        indices[slot] = counter;
        ++count;

        while (indices[(size_t) head] <= counter - window)
        {
            head = (head + 1) % capacity;
            --count;
        }

        ++counter;
        return values[(size_t) head];
    }

private:
    int back() const noexcept { return (head + count - 1) % capacity; }

    std::vector<float> values;
    std::vector<std::int64_t> indices;
    std::int64_t counter = 0;
    int window = 1, capacity = 2, head = 0, count = 0;
};

class MovingAverage
{
public:
    void prepare (int windowSize, float fill = 1.0f)
    {
        buffer.assign ((size_t) windowSize, fill);
        reset (fill);
    }

    void reset (float fill = 1.0f) noexcept
    {
        std::fill (buffer.begin(), buffer.end(), fill);
        sum = (double) fill * (double) buffer.size();
        position = 0;
    }

    float push (float value) noexcept
    {
        sum += (double) value - (double) buffer[position];
        buffer[position] = value;
        position = (position + 1) % buffer.size();
        return (float) (sum / (double) buffer.size());
    }

private:
    std::vector<float> buffer;
    double sum = 0.0;
    size_t position = 0;
};
}
