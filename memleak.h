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

#define callocp calloc
#define mallocp malloc
#define reallocp realloc
#define memalignp memalign
#define posix_memalignp posix_memalign
#define freep free

#define PRINT_TO_CONSOLE 0 // 0/1 print/no print

static pthread_mutex_t mh_mutex = PTHREAD_MUTEX_INITIALIZER;

static volatile int initialized; // 是否已初始化标志
static __thread int thread_in_hook; // 用于多线程的 hook 标志，

#define MH_HASH_BITS 20 /* 1 M entries, hardcoded for now */
#define MH_TABLE_SIZE (1 << MH_HASH_BITS)
static struct cds_hlist_head mh_table[MH_TABLE_SIZE]; // 哈希表

void* _malloc(size_t size, const char* file, const int line, const char* func);

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
    e = _malloc(sizeof(*e), __FILE__, __LINE__, __func__); // 开辟一个 mh_entry
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

/* 函数功能：初始化
*  定义函数指针，根据环境变量设置全局参数
*/
static void __attribute__((constructor))
do_init(void)
{
    char* env;

    if (initialized)
        return;

    initialized = 1;
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

    do_init();

    if (thread_in_hook) {
        return calloc(nmemb, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = calloc(nmemb, size);

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

    do_init();

    if (thread_in_hook) {
        return malloc(size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = malloc(size);

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

    do_init();

    if (thread_in_hook) {
        return realloc(ptr, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = realloc(ptr, size);

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

    do_init();

    if (thread_in_hook) {
        return memalign(alignment, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = memalign(alignment, size);

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

    do_init();

    if (thread_in_hook) {
        return posix_memalign(memptr, alignment, size);
    }

    thread_in_hook = 1;

    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    result = posix_memalign(memptr, alignment, size);

    add_mh(*memptr, size, file, line, func);

    /* printf might call malloc, so protect it too. */
    if (PRINT_TO_CONSOLE)
        fprintf(stderr, "posix_memalign(%p,%zu,%zu) returns %d\n",
            memptr, alignment, size, result);

    pthread_mutex_unlock(&mh_mutex);

    thread_in_hook = 0;

    return result;
}

void _free(void* ptr, size_t useless)
{
    do_init();

    if (thread_in_hook) {
        free(ptr);
        return;
    }

    thread_in_hook = 1;
    pthread_mutex_lock(&mh_mutex);

    /* Call resursively */
    free(ptr);

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
}


#define malloc(size) _malloc(size, __FILE__, __LINE__, __func__)
#define posix_memalign(memptr, alignment, size) _posix_memalign(memptr, alignment, size, __FILE__, __LINE__, __func__)
#define memalign(alignment, size) _memalign(alignment, size, __FILE__, __LINE__, __func__)
#define realloc(ptr, size) _realloc(ptr, size, __FILE__, __LINE__, __func__)
#define calloc(nmemb, size) _calloc(nmemb, size, __FILE__, __LINE__, __func__)