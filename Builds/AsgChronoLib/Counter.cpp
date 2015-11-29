#include "stdafx.h"
#include "Counter.h"

// for Hermite interpolation
const size_t HISTORY_SAMPLES_BEFORE = 2;

AsgCounterConfig::AsgCounterConfig()
{
    sampleRate = 44100.0f;
    length = 0.2f;

    minPeakDistance = 30;
    maxPeakDistance = 300;

    mass = 0.0002f;
    fireRateTreshold = 1.25f;
    detectionSigma = 7.0f;
}


AsgStats::AsgStats()
{
    Reset();
}

void AsgStats::Reset()
{
    history.clear();
    fireRateMin = -1.0f;
    fireRateMax = -1.0f;
    fireRateAvg = -1.0f;
    fireRateStdDev = -1.0f;
}

void AsgStats::Print() const
{
    printf("Stats (based on %i samples):\n", (int)history.size());
    printf("Velocity:  avg = %.1f, min = %.1f, max = %.1f, std. dev. = %.2f\n",
           velocityAvg, velocityMin, velocityMax, velocityStdDev);
    printf("Fire rate: avg = %.2f, min = %.2f, max = %.2f, std. dev. = %.2f\n",
           fireRateAvg, fireRateMin, fireRateMax, fireRateStdDev);
}

void AsgStats::Calc(const AsgCounterConfig& cfg)
{
    velocityMin = FLT_MAX;
    velocityMax = FLT_MIN;
    velocityAvg = -1.0f;
    velocityStdDev = -1.0f;

    int validVelocitySamples = 0;
    float sum = 0.0f;

    // calculate average velocity
    for (size_t i = 0; i < history.size(); ++i)
    {
        if (history[i].velocity > 0.0f)
        {
            sum += history[i].velocity;
            if (history[i].velocity > velocityMax)
                velocityMax = history[i].velocity;
            if (history[i].velocity < velocityMin)
                velocityMin = history[i].velocity;
            validVelocitySamples++;
        }
    }

    if (validVelocitySamples > 0)
    {
        velocityAvg = sum / (float)validVelocitySamples;
        float stdDevSum = 0.0f;

        // calculate standard deviation
        for (size_t i = 0; i < history.size(); ++i)
        {
            if (history[i].velocity > 0.0f)
            {
                float difference = velocityAvg - history[i].velocity;
                stdDevSum += difference * difference;
            }
        }

        velocityStdDev = sqrtf(stdDevSum / (float)validVelocitySamples);
    }

    // calculate fire rate stats
    if (history.size() > 2)
    {
        float lastDeltaTime = history[history.size() - 1].deltaTime;
        float minDeltaTime = lastDeltaTime;
        float maxDeltaTime = lastDeltaTime;
        float deltaTimeSum = lastDeltaTime;
        int samples = 1;

        for (size_t i = history.size() - 2; i > 0; --i)
        {
            float dt = history[i].deltaTime;

            if (dt > lastDeltaTime * cfg.fireRateTreshold)
            {
                break;
            }

            // we are in semi-auto mode - do not calculate RoF
            if (dt * cfg.fireRateTreshold < lastDeltaTime)
            {
                samples = 0;
                break;
            }

            // TODO: support for minimum fire rate (config)

            if (dt > maxDeltaTime)
                maxDeltaTime = dt;
            if (dt < minDeltaTime)
                minDeltaTime = dt;
            deltaTimeSum += dt;
            samples++;
        }

        if (samples > 0)
        {
            fireRateMin = 1.0f / maxDeltaTime;
            fireRateMax = 1.0f / minDeltaTime;
            fireRateAvg = (float)samples / deltaTimeSum;
        }
        else
        {
            fireRateMin = -1.0f;
            fireRateMax = -1.0f;
            fireRateAvg = -1.0f;
            fireRateStdDev = -1.0f;
        }
    }
}

void AsgStats::AddSample(float velocity, float deltaTime)
{
    AsgStatsSample sample;
    sample.velocity = velocity;
    sample.deltaTime = deltaTime;
    history.push_back(sample);
}

AsgCounter::AsgCounter()
{
    buffer.resize(BUFFER_SIZE);
    Reset();
}

void AsgCounter::Reset()
{
    warmup = true;
    bufferPtr = 0;
    state = State::BeforePeak;
    samplePos = 0;
    averageRMS = 0.0f;

    reportsNum = 0;
    prevPeakA = -1.0f;

    history.resize(config.minPeakDistance + HISTORY_SAMPLES_BEFORE);

    stats.Reset();
}

AsgStats& AsgCounter::GetStats()
{
    return stats;
}

AsgCounterConfig& AsgCounter::GetConfig()
{
    return config;
}

void AsgCounter::ReportPeaksGroup(float peakA, float peakB)
{
#ifdef _DEBUG
    printf("#%i:\t A = %.2f,\t B = %.2f", reportsNum, peakA, peakB);
#endif

    float velocity = -1.0f;
    if (peakB > 0.0f)
    {
        float sampleDist = peakB - peakA;
        velocity = config.length * config.sampleRate / sampleDist;
#ifdef _DEBUG
        printf(",\t d = %.2f,\t m/s = %.1f", sampleDist, velocity);
#endif
    }

#ifdef _DEBUG
    printf("\n");
#endif

    float dt = -1.0f;
    if (prevPeakA > 0)
        dt = (peakA - prevPeakA) / config.sampleRate;
    stats.AddSample(velocity, dt);

    prevPeakA = peakA;
    reportsNum++;

    if (callback)
        callback();
}

float AsgCounter::FindPeakInHistory()
{
    int maxID = 0;
    float tmp = -1.0f;
    for (int i = 0; i < config.minPeakDistance + HISTORY_SAMPLES_BEFORE; ++i)
    {
        float val = fabsf(history[i]);
        if (val > tmp)
        {
            tmp = val;
            maxID = i;
        }
    }

    if (maxID < HISTORY_SAMPLES_BEFORE || maxID >= config.minPeakDistance)
        return static_cast<float>(maxID - (int)HISTORY_SAMPLES_BEFORE);

    // Hermite interpolation

    int offset;
    if (fabsf(history[maxID - 1]) < fabsf(history[maxID + 1]))
        offset = -1;
    else
        offset = -2;

    float y0 = history[maxID + offset];
    float y1 = history[maxID + offset + 1];
    float y2 = history[maxID + offset + 2];
    float y3 = history[maxID + offset + 3];

    float c0 = y1;
    float c1 = 0.5f * (y2 - y0);
    float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    float c3 = 1.5f * (y1 - y2) + 0.5f * (y3 - y0);

    float dc0 = c1;
    float dc1 = 2.0f * c2;
    float dc2 = 3.0f * c3;

    float delta = dc1 * dc1 - 4.0f * dc0 * dc2;

    float x = 0.5f;
    float y = 0.0f;
    if (delta >= 0.0f)
    {
        float x0 = (-dc1 - sqrtf(delta)) / (2.0f * dc2);
        float x1 = (-dc1 + sqrtf(delta)) / (2.0f * dc2);

        float iy0 = ((c3 * x0 + c2) * x0 + c1) * x0 + c0;
        float iy1 = ((c3 * x1 + c2) * x1 + c1) * x1 + c0;


        if ((x0 < 0.0f || x0 > 1.0f) && (x1 < 0.0f || x1 > 1.0f))
        {
            // both maxima are beyond (0, 1) range
            x = 0.5f;
            y = 0.0f;
        }
        else if (x0 < 0.0f || x0 > 1.0f)
        {
            x = x1;
            y = iy1;
        }
        else if (x1 < 0.0f || x1 > 1.0f)
        {
            x = x0;
            y = iy0;
        }
        else if (fabsf(iy0) > fabsf(iy1))
        {
            x = x0;
            y = iy0;
        }
        else
        {
            x = x1;
            y = iy1;
        }
    }

    // printf("  y0 = %6.3f, y1 = %6.3f, y2 = %6.3f, y3 = %6.3f  =>  y(%.2f) = %.2f\n", y0, y1, y2, y3, x, y);

    return static_cast<float>(maxID + offset) + x;
}

void AsgCounter::Analyze()
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

    if (warmup)
    {
        warmup = false;
        samplePos += BUFFER_SIZE;
        sampleInCurState += BUFFER_SIZE;
        return;
    }

    rms = averageRMS;
    const float tresholdOffset = 0.001f;
    float treshold = config.detectionSigma * rms + tresholdOffset;

    for (size_t i = 0; i < BUFFER_SIZE; ++i)
    {
        float sample = buffer[i];
        bool above = (sample > treshold) || (-sample > treshold);

        if (above && state == State::BeforePeak) // before any peak
        {
            peakSearchStart[0] = samplePos;
            state = State::FirstPeak;
            sampleInCurState = 0;

            for (size_t j = 0; j < HISTORY_SAMPLES_BEFORE; ++j)
            {
                int id = (int)(i + j) - (int)HISTORY_SAMPLES_BEFORE;
                history[j] = (id >= 0) ? buffer[id] : 0.0f;  // TODO
            }
            history[HISTORY_SAMPLES_BEFORE] = sample;
        }
        else if (state == State::FirstPeak)  // first peak
        {
            if (sampleInCurState >= config.minPeakDistance)
            {
                state = State::BetweenPeaks;
                sampleInCurState = 0;
            }
            else
                history[sampleInCurState + HISTORY_SAMPLES_BEFORE] = sample;
        }
        else if (state == State::BetweenPeaks)  // between peaks
        {
            if (above)
            {
                peakSearchStart[1] = samplePos;
                state = State::SecondPeak;
                sampleInCurState = 0;

                firstPeakEstimation = peakSearchStart[0] + FindPeakInHistory();

                // TODO
                for (size_t j = 0; j < HISTORY_SAMPLES_BEFORE; ++j)
                {
                    int id = (int)(i + j) - (int)HISTORY_SAMPLES_BEFORE;
                    history[j] = (id >= 0) ? buffer[id] : 0.0f;  // TODO
                }
                history[HISTORY_SAMPLES_BEFORE] = sample;
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
                history[sampleInCurState + HISTORY_SAMPLES_BEFORE] = sample;
        }

        samplePos++;
        sampleInCurState++;
    }

    // printf("RMS = %f, treshold = %f\n", rms, treshold);
}

void AsgCounter::ProcessBuffer(float* samples, size_t samplesNum)
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

void AsgCounter::SetCallback(AsgEventCallback callback)
{
    this->callback = callback;
}