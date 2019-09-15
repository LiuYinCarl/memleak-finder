#include <stdlib.h>
#include <string>
#include <vector>
#include <iostream>
#include "memleak.h"
using namespace std;

class Base {

public:
    // 类内重载 new delete, 暂不支持
    // void* operator new(size_t size) {
    //     cout << "Base::new" << endl;
    //     return malloc(size);
    // }
    
    // void operator delete(void* ptr) {
    //     cout << "Base::delete" << endl;
    //     free(ptr);
    // }


    Base() {}
    ~Base(){};
};

void func(int a, double b)
{
    // int _a = a;
    // double _b = b;
    // int tmp = 10;

    // int* p_tmp = (int*)malloc(sizeof(int));
    // *p_tmp = tmp;

    // free(p_tmp);


    // int* i = new int;
    // char* chr = new char;
    // int* i2 = new int[10];
    // char* chr2 = new char[10];
    // string* str = new string[2];
    // Base* base = new Base[10];

    // delete i;
    // delete chr;
    // delete[] i2;
    // delete[] chr2;
    // delete[] str;
    // delete[] base;

    // 测试 stl
    vector<int> v;
    v.push_back(1);
    v.push_back(2);
    for (auto i: v)
        cout << i << " ";
    cout << endl;

    // 测试自定义类
    // Base* bas = new Base;
    // delete bas;

}

int main()
{
    int a = 5;
    double b = 10.0;
    func(a, b);
    return 0;
}