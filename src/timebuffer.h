#include <Arduino.h>
#include <ArduinoLog.h>

template <typename T, int SAMPLES_PER_SECOND, int SAMPLE_BUFFER_DURATION_SECS>
struct TimeBuffer
{
    T samples[SAMPLES_PER_SECOND * SAMPLE_BUFFER_DURATION_SECS];
    unsigned int nextIdx, filledSize;

    TimeBuffer()
    {
        nextIdx = 0;
        filledSize = 0;
        memset(samples, 0, sizeof(samples));
    }

    void add(T v)
    {
        samples[nextIdx++] = v;
        nextIdx %= sizeof(samples) / sizeof(T);
        filledSize = min(filledSize + 1, (unsigned int)(sizeof(samples) / sizeof(T)));
    }

    T lastValue() {
        if (nextIdx == 0) return samples[sizeof(samples) / sizeof(T) - 1];
        else return samples[nextIdx - 1];
    }

    /* views the buffer as a time snapshot, can be read like a normal array */
    struct View
    {
        uint32_t startIdx, range;
        TimeBuffer *buf;

        View() {}

        View(TimeBuffer *buf, uint32_t start, uint32_t range) : buf(buf), startIdx(start), range(range)
        {
            Log.verboseln("View start=%i range=%i", start, range);

            if (range >= buf->filledSize)
            {
                const char err[] = "FATAL ERROR - range exceeds buf size";
                Log.fatalln(err);
                throw err;
            }
        }

        T &operator[](int i)
        {
            i += startIdx;
            while (i < 0)
                i += buf->filledSize;

            return buf->samples[i % buf->filledSize];
        }

        T getMax()
        {
            T vmax = -1e10;
            for (int i = 0; i < range; i++) vmax = max(vmax, (*this)[i]);
            return vmax;
        }

        T getMin()
        {
            T vmin = 1e10;
            for (int i = 0; i < range; i++) vmin = min(vmin, (*this)[i]);
            return vmin;
        }

        float getAverage()
        {
            float sum = 0;
            for (int i = 0; i < range; i++) sum += (*this)[i];
            return sum / range;
        }

        float getStdev()
        {
            float avg = getAverage();
            float sum = 0;
            for (int i = 0; i < range; i++)
            {
                sum += ((*this)[i] - avg) * ((*this)[i] - avg);
            }
            return sqrt(sum / range);
        }

        float toStdev(float v)
        {
            return (v - getAverage()) / getStdev();
        }

        float getSlope()
        {
            float diffSum = 0.f;
            for (int i = 0; i < range - 1; i++)
            {
                diffSum += (*this)[i + 1] - (*this)[i];
            }
            return diffSum / (range - 1);
        }
    };

    bool isFull()
    {
        return filledSize == sizeof(samples) / sizeof(T);
    }

    bool viewDuration(
        View *out,
        float startTimeOffset, // should always be negative
        float endTimeOffset = 0.f)
    {
        if (startTimeOffset < 0 && (endTimeOffset - startTimeOffset) * SAMPLES_PER_SECOND <= filledSize && filledSize == sizeof(samples) / sizeof(T))
        {
            *out = View(this, /*start*/ nextIdx + startTimeOffset * SAMPLES_PER_SECOND, /*range*/ (endTimeOffset - startTimeOffset) * SAMPLES_PER_SECOND);
            return true;
        }
        else
        {
            return false;
        }
    }

    void view(
        float activeTimeOffset, // should always be negative
        View *inactiveView,
        View *activeView)
    {
        int inactiveStart = nextIdx - filledSize;
        int inactiveRange = (SAMPLE_BUFFER_DURATION_SECS + activeTimeOffset) * SAMPLES_PER_SECOND;

        int activeStart = nextIdx + activeTimeOffset * SAMPLES_PER_SECOND;
        int activeRange = -activeTimeOffset * SAMPLES_PER_SECOND;

        *inactiveView = View(this, inactiveStart, inactiveRange);
        *activeView = View(this, activeStart, activeRange);
    }
};