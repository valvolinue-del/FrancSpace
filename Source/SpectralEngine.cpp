#include "SpectralEngine.h"

#include <algorithm>
#include <cmath>

namespace franc
{
namespace
{
constexpr double twoPi = 6.28318530717958647692;

float smoothstep (float edge0, float edge1, float x) noexcept
{
    const float span = edge1 - edge0;
    if (span <= 1.0e-6f)
        return x < edge0 ? 0.0f : 1.0f;

    const float t = std::clamp ((x - edge0) / span, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
} // namespace

void SpectralEngine::Fft::prepare (int order)
{
    size = 1 << order;
    rev.resize (static_cast<size_t> (size));
    for (int i = 0; i < size; ++i)
    {
        int reversed = 0;
        for (int bit = 0; bit < order; ++bit)
            if ((i & (1 << bit)) != 0)
                reversed |= 1 << (order - 1 - bit);
        rev[static_cast<size_t> (i)] = reversed;
    }

    twiddle.resize (static_cast<size_t> (size / 2));
    for (int k = 0; k < size / 2; ++k)
    {
        const double angle = -twoPi * static_cast<double> (k) / static_cast<double> (size);
        twiddle[static_cast<size_t> (k)] = { std::cos (angle), std::sin (angle) };
    }
}

void SpectralEngine::Fft::forward (std::complex<double>* data) const noexcept
{
    for (int i = 0; i < size; ++i)
    {
        const int j = rev[static_cast<size_t> (i)];
        if (i < j)
            std::swap (data[i], data[j]);
    }

    for (int length = 2; length <= size; length <<= 1)
    {
        const int half = length >> 1;
        const int step = size / length;
        for (int start = 0; start < size; start += length)
        {
            for (int j = 0; j < half; ++j)
            {
                const auto w = twiddle[static_cast<size_t> (j * step)];
                const auto u = data[start + j];
                const auto v = data[start + j + half] * w;
                data[start + j] = u + v;
                data[start + j + half] = u - v;
            }
        }
    }
}

void SpectralEngine::Fft::inverse (std::complex<double>* data) const noexcept
{
    for (int i = 0; i < size; ++i)
        data[i] = std::conj (data[i]);

    forward (data);

    const double inv = 1.0 / static_cast<double> (size);
    for (int i = 0; i < size; ++i)
        data[i] = std::conj (data[i]) * inv;
}

void SpectralEngine::Stream::prepare (int fft, int hopCount)
{
    ring.assign (static_cast<size_t> (fft), 0.0f);
    ola.assign (static_cast<size_t> (fft), 0.0f);
    queue.assign (static_cast<size_t> (hopCount * 2), 0.0f);
    qRead = 0;
    qWrite = 0;
    qCount = 0;
}

void SpectralEngine::Stream::push (float sample) noexcept
{
    queue[static_cast<size_t> (qWrite)] = sample;
    qWrite = (qWrite + 1) % static_cast<int> (queue.size());
    ++qCount;
}

float SpectralEngine::Stream::pop() noexcept
{
    if (qCount <= 0)
        return 0.0f;

    const float sample = queue[static_cast<size_t> (qRead)];
    qRead = (qRead + 1) % static_cast<int> (queue.size());
    --qCount;
    return sample;
}

void SpectralEngine::prepare (double newSampleRate)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    fft.prepare (fftOrder);
    mainL.prepare (fftSize, hop);
    mainR.prepare (fftSize, hop);
    sideL.assign (static_cast<size_t> (fftSize), 0.0f);
    sideR.assign (static_cast<size_t> (fftSize), 0.0f);
    specL.assign (static_cast<size_t> (fftSize), {});
    specR.assign (static_cast<size_t> (fftSize), {});
    detL.assign (static_cast<size_t> (fftSize), {});
    detR.assign (static_cast<size_t> (fftSize), {});
    window.resize (static_cast<size_t> (fftSize));
    binBand.assign (static_cast<size_t> (fftSize / 2 + 1), 0);

    for (int i = 0; i < fftSize; ++i)
        window[static_cast<size_t> (i)] = 0.5 * (1.0 - std::cos (twoPi * static_cast<double> (i) / static_cast<double> (fftSize)));

    std::vector<double> overlap (static_cast<size_t> (fftSize), 0.0);
    for (int shift = 0; shift < fftSize; shift += hop)
        for (int i = 0; i < fftSize; ++i)
            overlap[static_cast<size_t> ((i + shift) % fftSize)] += window[static_cast<size_t> (i)] * window[static_cast<size_t> (i)];

    scale = overlap[0] > 0.0 ? 1.0 / overlap[0] : 1.0;
    refMagnitude = 0.25 * static_cast<double> (fftSize);

    for (int band = 0; band < numBands; ++band)
        bandHz[band] = 20.0f * std::pow (1000.0f, static_cast<float> (band) / static_cast<float> (numBands - 1));

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const float frequency = static_cast<float> (bin) * static_cast<float> (sampleRate) / static_cast<float> (fftSize);
        int best = 0;
        float bestDistance = 1.0e9f;
        const float safe = std::max (frequency, 1.0f);
        for (int band = 0; band < numBands; ++band)
        {
            const float distance = std::abs (std::log (safe / bandHz[band]));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = band;
            }
        }
        binBand[static_cast<size_t> (bin)] = best;
    }

    const double hopSeconds = static_cast<double> (hop) / sampleRate;
    modeCoeff = static_cast<float> (1.0 - std::exp (-hopSeconds / 0.04));
    reset();
}

void SpectralEngine::reset() noexcept
{
    std::fill (mainL.ring.begin(), mainL.ring.end(), 0.0f);
    std::fill (mainR.ring.begin(), mainR.ring.end(), 0.0f);
    std::fill (mainL.ola.begin(), mainL.ola.end(), 0.0f);
    std::fill (mainR.ola.begin(), mainR.ola.end(), 0.0f);
    std::fill (mainL.queue.begin(), mainL.queue.end(), 0.0f);
    std::fill (mainR.queue.begin(), mainR.queue.end(), 0.0f);
    std::fill (sideL.begin(), sideL.end(), 0.0f);
    std::fill (sideR.begin(), sideR.end(), 0.0f);
    mainL.qRead = mainL.qWrite = mainL.qCount = 0;
    mainR.qRead = mainR.qWrite = mainR.qCount = 0;
    write = 0;
    sinceHop = 0;
    modeWeight[0] = 1.0f;
    modeWeight[1] = 0.0f;
    modeWeight[2] = 0.0f;
    std::fill (std::begin (envLinked), std::end (envLinked), 0.0f);
    std::fill (std::begin (envLeft), std::end (envLeft), 0.0f);
    std::fill (std::begin (envRight), std::end (envRight), 0.0f);
    std::fill (std::begin (envMid), std::end (envMid), 0.0f);
    std::fill (std::begin (envSide), std::end (envSide), 0.0f);
    visuals = {};
}

float SpectralEngine::maskFor (float frequency, float lowCut, float highCut) noexcept
{
    float low = lowCut;
    float high = highCut;
    if (high < low)
        std::swap (low, high);

    constexpr float octave = 1.109569f; // 2^(0.15)
    const float lowMask = smoothstep (low / octave, low * octave, frequency);
    const float highMask = 1.0f - smoothstep (high / octave, high * octave, frequency);
    return lowMask * highMask;
}

void SpectralEngine::loadFrame (const std::vector<float>& ring, std::complex<double>* dest) const noexcept
{
    for (int i = 0; i < fftSize; ++i)
    {
        const float sample = ring[static_cast<size_t> ((write + i) % fftSize)];
        dest[i] = { window[static_cast<size_t> (i)] * static_cast<double> (sample), 0.0 };
    }
}

SpectralEngine::Out SpectralEngine::processSample (float inLeft, float inRight, float sideLeftSample,
                                                   float sideRightSample, const SpectralParams& params) noexcept
{
    mainL.ring[static_cast<size_t> (write)] = inLeft;
    mainR.ring[static_cast<size_t> (write)] = inRight;
    sideL[static_cast<size_t> (write)] = sideLeftSample;
    sideR[static_cast<size_t> (write)] = sideRightSample;
    write = (write + 1) % fftSize;

    if (++sinceHop == hop)
    {
        sinceHop = 0;
        processHop (params);
    }

    return { mainL.pop(), mainR.pop() };
}

void SpectralEngine::processHop (const SpectralParams& params) noexcept
{
    const double hopSeconds = static_cast<double> (hop) / sampleRate;
    const auto coefficient = [&] (float ms) noexcept
    {
        const double seconds = std::max (0.0001, static_cast<double> (ms) * 0.001);
        return static_cast<float> (1.0 - std::exp (-hopSeconds / seconds));
    };
    const float attack = coefficient (params.attackMs);
    const float release = coefficient (params.releaseMs);
    const float amount = std::clamp (params.amount, 0.0f, 1.0f);

    loadFrame (sideL, detL.data());
    loadFrame (sideR, detR.data());
    if (params.analyse)
    {
        fft.forward (detL.data());
        fft.forward (detR.data());
    }

    float peakLinked[numBands] {};
    float peakLeft[numBands] {};
    float peakRight[numBands] {};
    float peakMid[numBands] {};
    float peakSide[numBands] {};

    if (params.analyse)
    {
        for (int bin = 0; bin <= fftSize / 2; ++bin)
        {
            const int band = binBand[static_cast<size_t> (bin)];
            const auto left = detL[static_cast<size_t> (bin)];
            const auto right = detR[static_cast<size_t> (bin)];
            const float magLeft = static_cast<float> (std::abs (left));
            const float magRight = static_cast<float> (std::abs (right));
            const auto mid = 0.5 * (left + right);
            const auto side = 0.5 * (left - right);
            peakLeft[band] = std::max (peakLeft[band], magLeft);
            peakRight[band] = std::max (peakRight[band], magRight);
            peakLinked[band] = std::max (peakLinked[band], std::max (magLeft, magRight));
            peakMid[band] = std::max (peakMid[band], static_cast<float> (std::abs (mid)));
            peakSide[band] = std::max (peakSide[band], static_cast<float> (std::abs (side)));
        }
    }

    const auto follow = [&] (float& envelope, float peak) noexcept
    {
        float target = params.analyse ? static_cast<float> (static_cast<double> (peak) / refMagnitude) : 0.0f;
        if (target < 1.0e-8f)
            target = 0.0f;
        const float coeff = target > envelope ? attack : release;
        envelope += coeff * (target - envelope);
        if (envelope < 1.0e-8f)
            envelope = 0.0f;
    };

    float gainLinked[numBands];
    float gainLeft[numBands];
    float gainRight[numBands];
    float gainMid[numBands];
    float gainSide[numBands];
    float reductionLinked[numBands];
    float reductionLeft[numBands];
    float reductionRight[numBands];
    float reductionMid[numBands];
    float reductionSide[numBands];

    for (int band = 0; band < numBands; ++band)
    {
        follow (envLinked[band], peakLinked[band]);
        follow (envLeft[band], peakLeft[band]);
        follow (envRight[band], peakRight[band]);
        follow (envMid[band], peakMid[band]);
        follow (envSide[band], peakSide[band]);

        const float mask = maskFor (bandHz[band], params.lowCutHz, params.highCutHz);
        const auto toGain = [&] (float envelope, float& reduction) noexcept
        {
            const float depth = std::clamp (envelope / 0.5f, 0.0f, 1.0f);
            reduction = -maxReductionDb * amount * depth * mask;
            return std::pow (10.0f, reduction / 20.0f);
        };

        gainLinked[band] = toGain (envLinked[band], reductionLinked[band]);
        gainLeft[band] = toGain (envLeft[band], reductionLeft[band]);
        gainRight[band] = toGain (envRight[band], reductionRight[band]);
        gainMid[band] = toGain (envMid[band], reductionMid[band]);
        gainSide[band] = toGain (envSide[band], reductionSide[band]);
    }

    float targetWeight[3] { 0.0f, 0.0f, 0.0f };
    targetWeight[std::clamp (params.mode, 0, 2)] = 1.0f;
    for (int index = 0; index < 3; ++index)
        modeWeight[index] += modeCoeff * (targetWeight[index] - modeWeight[index]);

    const float wLink = modeWeight[0];
    const float wLr = modeWeight[1];
    const float wMs = modeWeight[2];

    loadFrame (mainL.ring, specL.data());
    loadFrame (mainR.ring, specR.data());
    fft.forward (specL.data());
    fft.forward (specR.data());

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const int band = binBand[static_cast<size_t> (bin)];
        const auto left = specL[static_cast<size_t> (bin)];
        const auto right = specR[static_cast<size_t> (bin)];
        const auto mid = (0.5 * (left + right)) * static_cast<double> (gainMid[band]);
        const auto side = (0.5 * (left - right)) * static_cast<double> (gainSide[band]);
        const auto msLeft = mid + side;
        const auto msRight = mid - side;
        const double leftGain = static_cast<double> (wLink * gainLinked[band] + wLr * gainLeft[band]);
        const double rightGain = static_cast<double> (wLink * gainLinked[band] + wLr * gainRight[band]);
        specL[static_cast<size_t> (bin)] = left * leftGain + msLeft * static_cast<double> (wMs);
        specR[static_cast<size_t> (bin)] = right * rightGain + msRight * static_cast<double> (wMs);
    }

    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        specL[static_cast<size_t> (fftSize - bin)] = std::conj (specL[static_cast<size_t> (bin)]);
        specR[static_cast<size_t> (fftSize - bin)] = std::conj (specR[static_cast<size_t> (bin)]);
    }
    specL[0] = { specL[0].real(), 0.0 };
    specR[0] = { specR[0].real(), 0.0 };
    specL[static_cast<size_t> (fftSize / 2)] = { specL[static_cast<size_t> (fftSize / 2)].real(), 0.0 };
    specR[static_cast<size_t> (fftSize / 2)] = { specR[static_cast<size_t> (fftSize / 2)].real(), 0.0 };

    fft.inverse (specL.data());
    fft.inverse (specR.data());

    auto emit = [&] (Stream& stream, const std::complex<double>* spectrum) noexcept
    {
        for (int i = 0; i < fftSize; ++i)
            stream.ola[static_cast<size_t> (i)] += static_cast<float> (window[static_cast<size_t> (i)] * spectrum[i].real() * scale);

        for (int i = 0; i < hop; ++i)
            stream.push (stream.ola[static_cast<size_t> (i)]);

        std::move (stream.ola.begin() + hop, stream.ola.end(), stream.ola.begin());
        std::fill (stream.ola.end() - hop, stream.ola.end(), 0.0f);
    };

    emit (mainL, specL.data());
    emit (mainR, specR.data());

    for (int band = 0; band < numBands; ++band)
    {
        visuals.spectrum[band] = wLink * envLinked[band]
                               + wLr * 0.5f * (envLeft[band] + envRight[band])
                               + wMs * 0.5f * (envMid[band] + envSide[band]);
        visuals.reductionDb[band] = wLink * reductionLinked[band]
                                  + wLr * 0.5f * (reductionLeft[band] + reductionRight[band])
                                  + wMs * 0.5f * (reductionMid[band] + reductionSide[band]);
    }
}
} // namespace franc
