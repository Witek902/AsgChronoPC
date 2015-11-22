#include "stdafx.h"
#include "Counter.h"

int main()
{
    const int bufferSize = 256;
    float buffer[bufferSize];

    FILE* file = fopen("..\\..\\Tests\\AK.raw", "rb");
    assert(file != nullptr);

    AsgCounter counter;
    for (;;)
    {
        size_t read = fread(buffer, sizeof(float), bufferSize, file);
        if (read <= 0)
            break;

        counter.ProcessBuffer(buffer, read);
    }


    system("pause");
    return 0;
}
