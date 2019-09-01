#include <stdlib.h>
#include "redefine.h"

void func(int a, double b) {
    int _a = a;
    double _b = b;
    int tmp = 10;
    int* p_tmp = (int*)malloc(sizeof(int));
    realloc(p_tmp, sizeof(int));
    p_tmp = &tmp;
}

int main() {
    int a = 5;
    double b = 10.0;
    func(a, b);
    return 0;
}

// #include <stdio.h>
// #include <string.h>

// int main(int argc, char** argv)
// {
//     printf("File    Fame: %s\n", __FILE__); //文件名
//     printf("Present Line: %d\n", __LINE__); //所在行
//     printf("Present Function: %s\n", __func__); //函数名

//     const char a[] = "hello";
//     const char *b = a;
//     printf("%s\n", b);

//     return 0;
// }