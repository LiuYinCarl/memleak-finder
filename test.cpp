#include <stdlib.h>
#include <string>
#include "memleak.h"
using namespace std;

class Base {

public:
    Base() {}
    ~Base(){};
};

void func(int a, double b)
{
    int _a = a;
    double _b = b;
    int tmp = 10;

    int* p_tmp = (int*)malloc(sizeof(int));
    *p_tmp = tmp;

    free(p_tmp);


    int* i = new int;
    char* chr = new char;
    int* i2 = new int[10];
    char* chr2 = new char[10];
    string* str = new string[2];
    Base* base = new Base[10];

    delete i;
    delete chr;
    delete[] i2;
    delete[] chr2;
    delete[] str;
    delete[] base;

}

int main()
{
    int a = 5;
    double b = 10.0;
    func(a, b);
    return 0;
}