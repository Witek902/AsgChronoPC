#pragma once

#include <vector>
#include <functional>

struct AsgCounterConfig
{
    // TODO: these should be in seconds
    size_t minPeakDistance;  // minimum twin peaks distance (in samples)
    size_t maxPeakDistance;  // maximum twin peaks distance (in samples)

    float sampleRate;       // samples per second
    float length;           // photocell length in meters

    float mass;             // BB mass in kg

    float detectionSigma;
    float fireRateTreshold;

    AsgCounterConfig();
};

struct AsgStatsSample
{
    float velocity;
    float deltaTime;
};

struct AsgStats
{
    std::vector<AsgStatsSample> history;

    // velocity in meters per second
    float velocityAvg, velocityMin, velocityMax, velocityStdDev;

    // fire rate in rounds per second
    float fireRateAvg, fireRateMin, fireRateMax, fireRateStdDev;

    AsgStats();
    void Reset();
    void AddSample(float velocity, float deltaTime);
    void Calc(const AsgCounterConfig& cfg);
    void Print() const;
};

typedef std::function<void()> AsgEventCallback;

class AsgCounter
{
    enum class State
    {
        BeforePeak,
        FirstPeak,
        BetweenPeaks,
        SecondPeak
    };

    static const size_t BUFFER_SIZE = 8192;

    AsgCounterConfig config;
    AsgStats stats;
    AsgEventCallback callback;

    bool warmup;
    float averageRMS;
    State state;
    int peakSearchStart[2];

    std::vector<float> buffer;
    size_t bufferPtr;

    size_t samplePos;  // samples passed since Reset()
    size_t sampleInCurState;  // samples passed since last state change

    int reportsNum;
    float prevPeakA;

    float firstPeakEstimation;
    std::vector<float> history;

    void ReportPeaksGroup(float peakA, float peakB);
    float FindPeakInHistory();
    void Analyze();

public:
    AsgCounter();
    void Reset();

    AsgStats& GetStats();
    AsgCounterConfig& GetConfig();

    void SetCallback(AsgEventCallback callback);

    /**
     * Process samples buffer (detect and count peaks).
     */
    void ProcessBuffer(float* samples, size_t samplesNum);
};