#pragma once

#include <complex>
#include <vector>

// 32-band sidechain carve. A windowed STFT (periodic Hann, 8x overlap) keeps a
// constant delay and reconstructs the input exactly while every band gain is 1.
// The sidechain magnitude of each band drives that gain. No heap traffic after prepare.
namespace franc
{
struct SpectralParams
{
    float amount = 0.0f;
    float lowCutHz = 20.0f;
    float highCutHz = 20000.0f;
    float attackMs = 5.0f;
    float releaseMs = 80.0f;
    int mode = 0;
    bool analyse = false;
};

struct SpectralVisuals
{
    float spectrum[32] {};
    float reductionDb[32] {};
};

class SpectralEngine
{
public:
    static constexpr int numBands = 32;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int hop = 256;
    static constexpr int latencySamples = fftSize - 1;
    static constexpr float maxReductionDb = 24.0f;

    void prepare (double sampleRate);
    void reset() noexcept;

    struct Out
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    Out processSample (float inLeft, float inRight, float sideLeft, float sideRight, const SpectralParams& params) noexcept;

    int getLatencySamples() const noexcept { return latencySamples; }
    const SpectralVisuals& getVisuals() const noexcept { return visuals; }
    float bandFrequency (int index) const noexcept { return bandHz[static_cast<size_t> (index)]; }

private:
    struct Fft
    {
        void prepare (int order);
        void forward (std::complex<double>* data) const noexcept;
        void inverse (std::complex<double>* data) const noexcept;

        int size = 0;
        std::vector<int> rev;
        std::vector<std::complex<double>> twiddle;
    };

    struct Stream
    {
        std::vector<float> ring;
        std::vector<float> ola;
        std::vector<float> queue;
        int qRead = 0;
        int qWrite = 0;
        int qCount = 0;

        void prepare (int fft, int hopCount);
        void push (float sample) noexcept;
        float pop() noexcept;
    };

    void processHop (const SpectralParams& params) noexcept;
    void loadFrame (const std::vector<float>& ring, std::complex<double>* dest) const noexcept;
    static float maskFor (float frequency, float lowCut, float highCut) noexcept;

    Fft fft;
    Stream mainL;
    Stream mainR;
    std::vector<float> sideL;
    std::vector<float> sideR;
    std::vector<std::complex<double>> specL;
    std::vector<std::complex<double>> specR;
    std::vector<std::complex<double>> detL;
    std::vector<std::complex<double>> detR;
    std::vector<double> window;
    std::vector<int> binBand;
    float bandHz[numBands] {};
    float envLinked[numBands] {};
    float envLeft[numBands] {};
    float envRight[numBands] {};
    float envMid[numBands] {};
    float envSide[numBands] {};
    float modeWeight[3] { 1.0f, 0.0f, 0.0f };

    int write = 0;
    int sinceHop = 0;
    double sampleRate = 48000.0;
    double scale = 1.0;
    double refMagnitude = 1.0;
    float modeCoeff = 1.0f;
    SpectralVisuals visuals;
};
} // namespace franc
