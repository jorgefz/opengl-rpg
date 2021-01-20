#include <stdio.h>

#include <string.h>


void triple(int *num)
{
    *num = *num * 3;
}


int main()
{
    int n = 4;
    triple(&n);
    printf("%d\n", n);
    return 0;
}


/*
    Arrays in C
    Pointers in C
    Strings in C
    Functions in C
*/
