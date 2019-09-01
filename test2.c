#include <stdio.h>

void func(int a) {
    printf("one para\n");
}

void func(int a, int b) {
    printf("two para\n");
}


int main() {
    func(1);
    func(1, 2);
    return 0;
}