#include "stdafx.h"
#include "Counter.h"

AsgCounterConfig::AsgCounterConfig()
{
    sampleRate = 44100.0f;
    length = 0.2f;

    minPeakDistance = 30;
    maxPeakDistance = 300;
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

void AsgStats::Calc()
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

    // TODO: move to config
    const float FIRE_RATE_TRESHOLD = 1.25f;

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

            if (dt > lastDeltaTime * FIRE_RATE_TRESHOLD)
            {
                break;
            }

            // we are in semi-auto mode - do not calculate RoF
            if (dt * FIRE_RATE_TRESHOLD < lastDeltaTime)
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
    history.resize(config.minPeakDistance);
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
    // printf("#%i:\t A = %.1f,\t B = %.1f", reportsNum, peakA, peakB);

    float velocity = -1.0f;
    if (peakB > 0.0f)
    {
        float sampleDist = peakB - peakA;
        velocity = config.length * config.sampleRate / sampleDist;

        // printf(",\t d = %.1f,\t FPS = %.1f", sampleDist, velocity);
    }

    float dt = -1.0f;
    if (prevPeakA > 0)
        dt = (peakA - prevPeakA) / config.sampleRate;
    stats.AddSample(velocity, dt);

    prevPeakA = peakA;
    reportsNum++;
    //printf("\n");

    if (callback)
        callback();
}

float AsgCounter::FindPeakInHistory()
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
        return;
    }

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