#pragma once

#include <vector>

struct AsgCounterConfig
{
    float sampleRate;       // samples per second

    // TODO: these should be in seconds
    int minPeakDistance;  // minimum twin peaks distance (in samples)
    int maxPeakDistance;  // maximum twin peaks distance (in samples)

    AsgCounterConfig()
    {
        sampleRate = 44100.0f;
        minPeakDistance = 30;
        maxPeakDistance = 300;
    }
};

struct AsgReport
{

};

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

    float averageRMS;
    State state;
    int peakSearchStart[2];

    std::vector<float> buffer;
    size_t bufferPtr;

    size_t samplePos;  // samples passed since Reset()
    size_t sampleInCurState;  // samples passed since last state change

    int reportsNum;

    float firstPeakEstimation;
    std::vector<float> history;

public:

    AsgCounter()
    {
        buffer.resize(BUFFER_SIZE);
        history.resize(config.minPeakDistance);

        Reset();
    }

    void Reset()
    {
        bufferPtr = 0;
        state = State::BeforePeak;
        samplePos = 0;
        averageRMS = 0.0f;

        reportsNum = 0;
    }

    void ReportPeaksGroup(float peakA, float peakB)
    {
        printf("#%i:\t A = %.1f,\t B = %.1f", reportsNum, peakA, peakB);

        if (peakB > 0.0f)
            printf(",\t dist = %.1f", peakB - peakA);

        reportsNum++;
        printf("\n");
    }

    float FindPeakInHistory()
    {
        int result = 0;
        float tmp = 0.0f;
        for (int i = 0; i < config.minPeakDistance; ++i)
        {
            float val = fabsf(history[i]);
            if (val > tmp)
            {
                tmp = val;
                result = i;
            }
        }

        // TODO: Hermite interpolation?
        return static_cast<float>(result);
    }

    void Analyze()
    {
        // calculate buffer RMS
        float rms = 0.0f;
        for (size_t i = 0; i < BUFFER_SIZE; ++i)
        {
            float sample = buffer[i];
            rms += sample * sample;
        }
        rms = sqrtf(rms / static_cast<float>(BUFFER_SIZE));

        // RMS smoothing
        if (rms > averageRMS)
            averageRMS = rms;
        else
            averageRMS += 0.5f * (rms - averageRMS);

        rms = averageRMS;
        const float tresholdOffset = 0.001f;
        const float sigma = 7.0f;
        float treshold = sigma * rms + tresholdOffset;



        for (size_t i = 0; i < BUFFER_SIZE; ++i)
        {
            float sample = buffer[i];
            bool above = (sample > treshold) || (-sample > treshold);

            if (above && state == State::BeforePeak) // before any peak
            {
                peakSearchStart[0] = samplePos;
                state = State::FirstPeak;
                sampleInCurState = 0;
                history[0] = sample;
            }
            else if (state == State::FirstPeak)  // first peak
            {
                if (sampleInCurState >= config.minPeakDistance)
                {
                    state = State::BetweenPeaks;
                    sampleInCurState = 0;
                }
                else
                    history[sampleInCurState] = sample;
            }
            else if (state == State::BetweenPeaks)  // between peaks
            {
                if (above)
                {
                    peakSearchStart[1] = samplePos;
                    state = State::SecondPeak;
                    sampleInCurState = 0;

                    firstPeakEstimation = peakSearchStart[0] + FindPeakInHistory();

                    history[0] = sample;
                }
                else if (sampleInCurState >= config.maxPeakDistance)
                {
                    // distance between peaks is too large - ignore it
                    firstPeakEstimation = peakSearchStart[0] + FindPeakInHistory();
                    ReportPeaksGroup(firstPeakEstimation, -1.0f);
                    state = State::BeforePeak;
                }
            }
            else if (state == State::SecondPeak)  // second peak
            {
                if (sampleInCurState >= config.minPeakDistance)
                {
                    float secondPeakEstimation = peakSearchStart[1] + FindPeakInHistory();
                    ReportPeaksGroup(firstPeakEstimation, secondPeakEstimation);
                    state = State::BeforePeak;
                }
                else
                    history[sampleInCurState] = sample;
            }

            samplePos++;
            sampleInCurState++;
        }

        // printf("RMS = %f, treshold = %f\n", rms, treshold);
    }

    /**
     * Process samples buffer (detect and count peaks).
     */
    void ProcessBuffer(float* samples, size_t samplesNum)
    {
        for (size_t i = 0; i < samplesNum; ++i)
        {
            buffer[bufferPtr++] = samples[i];
            if (bufferPtr == BUFFER_SIZE)
            {
                Analyze();
                bufferPtr = 0;
            }
        }
    }
};