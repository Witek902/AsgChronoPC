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
    static const size_t BUFFER_SIZE = 4096;

    AsgCounterConfig config;

    int peakCounter;
    int peakStart[2];
    int peakEnd[2];

    std::vector<float> buffer;
    size_t bufferPtr;

    size_t samplePos;

    int reportsNum;
    float prevReportPeak;

public:

    AsgCounter()
    {
        buffer.resize(BUFFER_SIZE);
        Reset();
    }

    void Reset()
    {
        bufferPtr = 0;
        peakCounter = 0;
        samplePos = 0;

        reportsNum = 0;
        prevReportPeak = -1.0f;
    }

    void Report(float peakA, float peakB)
    {
        printf("#%i distance = %.1f samples", reportsNum, peakB - peakA);

        if (prevReportPeak > 0.0f)
        {
            printf(", prev peak = %.1f samples", peakA - prevReportPeak);
        }

        prevReportPeak = peakA;
        reportsNum++;
        printf("\n");
    }

    void Analyze()
    {
        // calculate buffer RMS
        float rms = 0.0f;
        for (size_t i = 0; i < BUFFER_SIZE; ++i)
        {
            float sample = buffer[i];
            // rms += fabsf(sample);
            rms += sample * sample;
        }
        // rms = rms / static_cast<float>(BUFFER_SIZE);
        rms = sqrtf(rms / static_cast<float>(BUFFER_SIZE));

        const float sigma = 6.0f;
        float treshold = sigma * rms;

        for (size_t i = 0; i < BUFFER_SIZE; ++i)
        {
            bool above = (buffer[i] > treshold) || (-buffer[i] > treshold);

            if (above && peakCounter == 0) // before any peak
            {
                peakStart[0] = samplePos;
                peakCounter = 1;
            }
            else if (peakCounter == 1)  // first peak
            {
                if (samplePos - peakStart[0] > config.minPeakDistance)
                {
                    peakCounter = 2;
                }
            }
            else if (peakCounter == 2)  // between peaks
            {
                if (above)
                {
                    peakStart[1] = samplePos;
                    peakCounter = 3;

                    // TODO: peakStart are only beggings of peak search region
                    Report(peakStart[0], peakStart[1]);
                }
            }
            else if (peakCounter == 3)  // second peak
            {
                if (samplePos - peakStart[1] > config.maxPeakDistance)
                {
                    peakCounter = 0;
                }
            }

            samplePos++;
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