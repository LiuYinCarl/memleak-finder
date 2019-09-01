#include <stdlib.h>
#include "memleak.h"


void func(int a, double b) {
    int _a = a;
    double _b = b;
    int tmp = 10;
    int* p_tmp = (int*)malloc(sizeof(int));
    *p_tmp = tmp;
    // free(p_tmp);
    // p_tmp = NULL;
}

int main() {
    int a = 5;
    double b = 10.0;
    func(a, b);
    return 0;
}