#include <limits.h>

int main(void)
{
    volatile int largest = INT_MAX;
    return largest + 1;
}
