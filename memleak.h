#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <malloc.h>
#include "hlist.h"
#include "jhash.h"

#define MEMLEAK_FOR_CPP // cpp 编译选项, 编译 C 项目时注释掉
#define PRINT_TO_CONSOLE 1 // 0/1 是否打印内存申请释放信息

static pthread_mutex_t mh_mutex = PTHREAD_MUTEX_INITIALIZER;
static __thread int thread_in_hook = 0; // 用于多线程的 hook 标志，

#define MH_HASH_BITS 20 /* 1 M entries, hardcoded for now */
#define MH_TABLE_SIZE (1 << MH_HASH_BITS)
static struct cds_hlist_head mh_table[MH_TABLE_SIZE]; // 哈希表

void* _malloc(size_t size, const char* file, const int line, const char* func);

#ifdef MEMLEAK_FOR_CPP

struct call_counter {
    size_t calloc_cnt;
    size_t malloc_cnt;
    size_t realloc_cnt;
    size_t memalign_cnt;
    size_t posix_memalign_cnt;
    size_t free_cnt;
    size_t new_cnt;
    size_t new_array_cnt;
    size_t delete_cnt;
    size_t delete_array_cnt;
};

struct call_counter counter = {
    .calloc_cnt = 0,
    .malloc_cnt = 0,
    .realloc_cnt = 0,
    .memalign_cnt = 0,
    .posix_memalign_cnt = 0,
    .free_cnt = 0,
    .new_cnt = 0,
    .new_array_cnt = 0,
    .delete_cnt = 0,
    .delete_array_cnt = 0
};

#else

struct call_counter {
    size_t calloc_cnt;
    size_t malloc_cnt;
    size_t realloc_cnt;
    size_t memalign_cnt;
    size_t posix_memalign_cnt;
    size_t free_cnt;
};

struct call_counter counter = {
    .calloc_cnt = 0,
    .malloc_cnt = 0,
    .realloc_cnt = 0,
    .memalign_cnt = 0,
    .posix_memalign_cnt = 0,
    .free_cnt = 0,
};

#endif

// hash 表节点
struct mh_entry { // memory hash entry
    struct cds_hlist_node hlist;
    void* ptr;
    size_t alloc_size;
    const char* file;
    int line;
    const char* function;
};

/*	函数功能：从 hash 表中查询元素
*   计算元素 hash, 检查元素是否存在于 hash 表
*   如果存在，返回该 hash 表元素， 否则返回 NULL
*/
static struct mh_entry*
get_mh(const void* ptr)
{
    struct cds_hlist_head* head;
    struct cds_hlist_node* node;
    struct mh_entry* e;
    uint32_t hash;

    hash = jhash(&ptr, sizeof(ptr), 0); // 计算 hash
    head = &mh_table[hash & (MH_TABLE_SIZE - 1)]; // 获取哈希表区块链表头节点
    cds_hlist_for_each_entry(e, node, head, hlist)
    { // 遍历链表，检查 ptr 是否在链表中
        if (ptr == e->ptr)
            return e;
    }
    return NULL; // ptr 不在链表中返回 NULL
}

/* 函数功能: 将元素添加到 hash 表中
*  首先检查元素是否已存在于 hash 表中，如果已存在，输出错误提示，
*  否则构造 hash 表元素，再将元素插入 hash 表中
*/
static void
add_mh(void* ptr, size_t alloc_size, const char* file, const int line, const char* func)
{
    struct cds_hlist_head* head;
    struct cds_hlist_node* node;
    struct mh_entry* e;
    uint32_t hash;

    if (!ptr)
        return;
    hash = jhash(&ptr, sizeof(ptr), 0);
    head = &mh_table[hash & (MH_TABLE_SIZE - 1)];
    cds_hlist_for_each_entry(e, node, head, hlist) // 遍历链表，检查 ptr 是否已在 hash 表中
    {
        if (ptr == e->ptr) {
            fprintf(stderr, "[warning] add_mh pointer %p is already there\n",
                ptr);
            //assert(0);	/* already there */
        }
    }
    // todo 选择这个 malloc 是否需要记录
    e = (struct mh_entry*)malloc(sizeof(*e)); // 开辟一个 mh_entry
    e->ptr = ptr;
    e->alloc_size = alloc_size;
    e->file = file;
    e->line = line;
    e->function = func;

    cds_hlist_add_head(&e->hlist, head); // 将节点添加到 hash 表中
}

/* 函数功能： 从 hash 表中删除元素
*  首先检查元素是否存在于 hash 表中，如果不存在，输出错误提示 
*  否则从 hash 表中删除元素并释放内存
*/
static void
del_mh(void* ptr)
{
    struct mh_entry* e;

    if (!ptr)
        return;
    e = get_mh(ptr);
    if (!e) {
        fprintf(stderr,
            "[warning] trying to free unallocated ptr %p\n",
            ptr);
        return;
    }
    cds_hlist_del(&e->hlist);
    free(e);
}

/* 下面几个函数基本上是对原本申请/释放内存的函数的一个包装，基本结构如下
*  1 获取当前函数返回地址
*  2 执行 do_init() 初始化
*  3 检查 thread_in_hook，如果为真说明当前线程在被 hook(执行其他内存操作函数)
*    直接执行相应内存操作函数并返回（好像不可能出现这种情况，需要测试一下）
*  4 否则，设置 thread_in_hook = 1，锁定互斥量
*  5 执行相应内存操作函数并将结果保存到 result 指针所指向地址
*  6 执行 add_mh 函数
*  7 检查是否需要打印到终端
*  8 解锁互斥量
*  9 thread_in_hook 设置为 0
*  10 返回 result
*/
void* _calloc(size_t nmemb, size_t size, const char* file, const int line, const char* func)
{
    void* result;

    if (thread_in_hook) {
        counter.calloc_cnt += 1;
        return calloc(nmemb, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = calloc(nmemb, size);
    counter.calloc_cnt += 1;
    add_mh(result, nmemb * size, file, line, func);

    /* printf might call malloc, so protect it too. */
    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "calloc(%zu,%zu) returns %p\n", nmemb, size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

void* _malloc(size_t size, const char* file, const int line, const char* func)
{
    void* result;

    if (thread_in_hook) {
        counter.malloc_cnt += 1;
        return malloc(size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = malloc(size);
    counter.malloc_cnt += 1;

    add_mh(result, size, file, line, func);

    /* printf might call malloc, so protect it too. */
    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "malloc(%zu) returns %p\n", size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

void* _realloc(void* ptr, size_t size, const char* file, const int line, const char* func)
{
    void* result;

    if (thread_in_hook) {
        counter.realloc_cnt += 1;
        return realloc(ptr, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = realloc(ptr, size);
    counter.realloc_cnt += 1;

    if (size == 0 && ptr) {
        /* equivalent to free() */
        del_mh(ptr);
    } else if (result) {
        del_mh(ptr);
        add_mh(result, size, file, line, func);
    }

    /* printf might call malloc, so protect it too. */
    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "realloc(%p,%zu) returns %p\n", ptr, size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

void* _memalign(size_t alignment, size_t size, const char* file, const int line, const char* func)
{
    void* result;

    if (thread_in_hook) {
        counter.memalign_cnt += 1;
        return memalign(alignment, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = memalign(alignment, size);
    counter.memalign_cnt += 1;

    add_mh(result, size, file, line, func);

    /* printf might call malloc, so protect it too. */
    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "memalign(%zu,%zu) returns %p\n",
            alignment, size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

int _posix_memalign(void** memptr, size_t alignment, size_t size, const char* file, const int line, const char* func)
{
    int result;

    if (thread_in_hook) {
        counter.posix_memalign_cnt += 1;
        return posix_memalign(memptr, alignment, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = posix_memalign(memptr, alignment, size);
    counter.posix_memalign_cnt += 1;

    add_mh(*memptr, size, file, line, func);

    /* printf might call malloc, so protect it too. */
    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "posix_memalign(%p,%zu,%zu) returns %d\n",
            memptr, alignment, size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

void _free(void* ptr)
{
    if (thread_in_hook) {
        counter.free_cnt += 1;
        free(ptr);
        return;
    }

    thread_in_hook = 1;
    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    free(ptr);
    counter.free_cnt += 1;

    del_mh(ptr);

    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "freed pointer %p\n", ptr);

    pthread_mutex_unlock(&mh_mutex);
    thread_in_hook = 0;
}

/* 函数功能: 依次检查 hash 表的每个节点，获取每个节点的链表头
*  遍历链表，如果发现链表上有元素，说明存在申请了但未释放的内存单元，
*  输出相应的提示信息
*/
static __attribute__((destructor)) void print_leaks(void)
{
    unsigned long i;

    for (i = 0; i < MH_TABLE_SIZE; i++) {
        struct cds_hlist_head* head;
        struct cds_hlist_node* node;
        struct mh_entry* e;

        head = &mh_table[i];
        cds_hlist_for_each_entry(e, node, head, hlist)
        {
            fprintf(stderr, "[leak]\tfile: %s line: %d function: %s\n\tptr: %p size: %zu\n\n",
                e->file, e->line, e->function, e->ptr,
                e->alloc_size);
        }
    }

#ifdef MEMLEAK_FOR_CPP
    fprintf(stderr, "[Function called information(times)]\ncalloc: %d\nmalloc: %d\nrealloc: %d\nmemalign: %d\nposix_memalign: %d\nfree: %d\nnew: %d\nnew[]: %d\ndelete: %d\ndelete[]: %d\n",
        counter.calloc_cnt, counter.malloc_cnt, counter.realloc_cnt,
        counter.memalign_cnt, counter.posix_memalign_cnt, counter.free_cnt,
        counter.new_cnt, counter.new_array_cnt, counter.delete_cnt, counter.delete_array_cnt);
#else
    fprintf(stderr, "[Function called information(times)]\ncalloc: %d\nmalloc: %d\nrealloc: %d\nmemalign: %d\nposix_memalign: %d\nfree: %d\n",
        counter.calloc_cnt, counter.malloc_cnt, counter.realloc_cnt,
        counter.memalign_cnt, counter.posix_memalign_cnt, counter.free_cnt);

#endif
}

#ifdef MEMLEAK_FOR_CPP
#include <exception>

struct call_recorder {
    const char* file;
    int line;

    call_recorder(const char* _file, int _line)
        : file(_file)
        , line(_line)
    {
    }
};

template <typename T>
inline T* operator*(const call_recorder& caller, T* ptr)
{
    if (thread_in_hook) {
        return ptr;
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    struct mh_entry* e;

    if (!ptr)
        return NULL;

    e = get_mh(ptr);

    // todo:下面是直接修改内存地址的代码，但这是一个非常不好的做法，
    // 但我现在没有能够想出任何其他的方法。
    // 在使用 new[] 的时候，如果使用的是内置类型, 如
    //      int* p = new int[10];
    // 那么 new() 申请内存返回的地址会和 new 返回的地址一致，
    // 但是如果不是内置类型，如：
    //      string* p = new string[10];
    // new() 申请内存返回的地址会比 new 返回的地址小 8
    // 具体原因还不清楚
    if (!e) {
#define ll long long
        ll tmp_ptr = (ll)ptr;
        tmp_ptr -= 8;
        void* _ptr = (void*)tmp_ptr;
        e = get_mh(_ptr);
    }

    if (!e) {
        fprintf(stderr,
            "[warning] trying to init info into unallocated ptr block %p\n",
            ptr);
        return NULL;
    }
    // 添加 call_recorder 信息到节点
    e->file = caller.file;
    e->line = caller.line;

    thread_in_hook = 0;

    return ptr;
}

void* operator new(size_t size)
{
    void* result;

    if (thread_in_hook) {
        counter.new_cnt += 1;
        result = malloc(size);
        if (result == NULL)
            throw std::bad_alloc();
        return result;
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    result = malloc(size);
    counter.new_cnt += 1;

    if (result == NULL)
        throw std::bad_alloc();

    //init mh_entry info
    const char* file = "blank file";
    int line = 0;
    const char* func = "new";

    add_mh(result, size, file, line, func);

    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "new(%zu) returns %p\n", size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

void* operator new[](size_t size)
{
    void* result;

    if (thread_in_hook) {
        counter.new_array_cnt += 1;
        result = malloc(size);
        if (result == NULL)
            throw std::bad_alloc();
        return result;
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    result = malloc(size);
    counter.new_array_cnt += 1;

    if (result == NULL)
        throw std::bad_alloc();

    //init mh_entry info
    const char* file = "blank file";
    int line = 0;
    const char* func = "new[]";

    add_mh(result, size, file, line, func);

    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "new[](%zu) returns %p\n", size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

void operator delete(void* ptr)
{
    if (thread_in_hook) {
        counter.delete_cnt += 1;
        free(ptr);
        return;
    }

    thread_in_hook = 1;
    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    free(ptr);
    counter.delete_cnt += 1;

    del_mh(ptr);

    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "delete pointer %p\n", ptr);

    pthread_mutex_unlock(&mh_mutex);
    thread_in_hook = 0;
}

void operator delete[](void* ptr)
{
    if (thread_in_hook) {
        counter.delete_array_cnt += 1;
        free(ptr);
        return;
    }

    thread_in_hook = 1;
    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    free(ptr);
    counter.delete_array_cnt += 1;

    del_mh(ptr);

    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "delete[] pointer %p\n", ptr);

    pthread_mutex_unlock(&mh_mutex);
    thread_in_hook = 0;
}

#define new call_recorder(__FILE__, __LINE__) * new

#endif // MEMLEAK_FOR_CPP

#define malloc(size) _malloc(size, __FILE__, __LINE__, __func__)
#define posix_memalign(memptr, alignment, size) _posix_memalign(memptr, alignment, size, __FILE__, __LINE__, __func__)
#define memalign(alignment, size) _memalign(alignment, size, __FILE__, __LINE__, __func__)
#define realloc(ptr, size) _realloc(ptr, size, __FILE__, __LINE__, __func__)
#define calloc(nmemb, size) _calloc(nmemb, size, __FILE__, __LINE__, __func__)
#define free(ptr) _free(ptr)
