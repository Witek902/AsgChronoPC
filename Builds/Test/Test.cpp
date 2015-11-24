#include "stdafx.h"
#include "Counter.h"

#include "Windows.h"

const int bufferSize = 16 * 1024;
static float buffer[bufferSize];

void Test(const char* name)
{
    std::string path = std::string("..\\..\\Tests\\") + name + ".raw";
    FILE* file = fopen(path.c_str(), "rb");
    assert(file != nullptr);

    printf("======= %s test =======\n", name);

    AsgCounter counter;
    for (;;)
    {
        size_t read = fread(buffer, sizeof(float), bufferSize, file);
        if (read <= 0)
            break;

        counter.ProcessBuffer(buffer, read);
    }

    printf("\n");
}

int main()
{
    LARGE_INTEGER start, stop, freq;
    QueryPerformanceFrequency(&freq);

    QueryPerformanceCounter(&start);

    Test("TestSample");
    Test("AK");
    Test("G36");
    Test("G36_rev");

    QueryPerformanceCounter(&stop);
    printf("Time = %.3f ms\n", (float)(stop.QuadPart - start.QuadPart) / (float)freq.QuadPart);

    system("pause");
    return 0;
}
