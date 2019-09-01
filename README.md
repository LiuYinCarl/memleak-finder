## 简介

​		这个项目是一个适用于 C 项目的内存泄漏检查工具。

原作者是 [https://github.com/efficios/memleak-finder](https://github.com/efficios/memleak-finder) 我对原作者的代码进行了部分注释，并仿照思路编写了 `memleak.c`工具。

## 原理

​		主要代码：

```bash
├── fdleak-finder.c 	# 文件描述符泄漏检测工具
├── hlist.h				# hash 表结构相关文件
├── jhash.h				# hash 算法实现
├── malloc-stats.c		# 内存状态查看工具
├── memleak-finder.c	# 内存泄漏检测工具 1
├── memleak.h 			# 内存泄漏检测工具 2
├── test.c 				# 测试文件
```

​		三个检测工具的基本原理都是在程序执行内存（文件描述符）申请/释放的过程中执行一个记录操作。记录保存在一个 hash 表中，解决 hash 冲突使用的方法是开链法。

​		以 `malloc` 和 `free` 为例，当执行 `malloc` 的时候，程序会记录 `malloc` 函数的指针，申请内存的大小等信息，记录到一个`mh_entry` 结构体中，然后根据指针地址计算 hash，然后将该结构体保存到 hash 表中。当执行 `free` 的时候，程序会计算 `free(ptr)` 的指针 `ptr` 的 hash 地址，然后从 hash 表中找到该块，并删除。

​		但是三个检测工具的具体处理方法不同。

​		`memleak-finder.c` 和 `fd-finder.c`使用的方法是 `LD_PRELOAD`。

> `LD_PRELOAD` 是 Linux 系统的一个环境变量，它可以影响程序的运行时的链接（Runtime linker），它允许你定义在程序运行前优先加载的动态链接库。这个功能主要就是用来有选择性的载入不同动态链接库中的相同函数。通过这个环境变量，我们可以在主程序和其动态链接库的中间加载别的动态链接库，甚至覆盖正常的函数库。一方面，我们可以以此功能来使用自己的或是更好的函数（无需别人的源码），而另一方面，**我们也可以以向别人的程序注入程序，从而达到特定的目的**。[1]

​		`memleak-finder.c` 和 `fd-finder.c` 就是通过编写 `malloc`, `free` 等的同名函数来实现对原本的函数的替换。在被重写的函数中实现记录内存操作的功能，并通过使用函数指针调用系统动态库来实现 `malloc`, `free` 原本的功能。**这个方法可以实现对源代码的无侵入内存检测，即不需要对需要检测的代码实施任何的修改。**

​		`memleak.h` 则是一种入侵式的修改，需要在要被检测的源文件的头部添加一个 `#include "memleak.h"`再进行编译。`memleak.h` 相比于 `memleak-finder.c` 能提供更多有价值的信息，比如造成内存泄漏的语句所在的文件、行数和函数名以及统计在程序执行过程中调用了各类内存操作函数的次数。

​		`memleak.h` 使用的具体方法是通过宏定义将原本的 `malloc`, `free` 函数调用变为了 `_malloc`, `_free` 函数调用。宏定义的格式如下：

```c++
#define malloc(size) _malloc(size, __FILE__, __LINE__, __func__)
```

​		然后在 `_malloc`, `_free` 等函数中进行记录内存操作并调用真正的 `malloc`, `free` 等函数。

## 使用

假设我们需要检测内存泄漏的文件叫做 `test1.c`

`memleak-finder.c` 和 `fdleak-finder.c` 的使用，以`memleak-finder` 为例子：

```bash
# 编译项目
make
# 编译 test.c
gcc test.c -o test.o -g -O0
# 执行程序
LD_PRELOAD=<path_to>/memleak-finder.so ./test.o
# 结果
[leak] ptr: 0x558779d03260 size: 4 caller: 0x55877843d16a <(null)>
```

`memleak.h` 的使用：

先在 `test.c` 文件头部加上 `#inclde "memleak.h"`

```bash
# 编译 test.c
gcc test.c -o test.o -g -O0
# 执行程序
./test.o
# 结果
[leak]  file: test.c line: 9 function: func
        ptr: 0x5581d0f72260 size: 4

[Function called information(times)]
calloc: 0
malloc: 1
realloc: 0
memalign: 0
posix_memalign: 0
free: 0
```

想了解更多信息可访问原作者项目。

## 参考



