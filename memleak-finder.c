/*
 * memleak-finder.c
 *
 * Memory leak finder
 *
 * Copyright (c) 2013 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <sys/types.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include "hlist.h"
#include "jhash.h"

static volatile int print_to_console;

static pthread_mutex_t mh_mutex = PTHREAD_MUTEX_INITIALIZER;
// 定义函数指针
static void *(*callocp)(size_t, size_t);
static void *(*mallocp)(size_t);
static void *(*reallocp)(void *, size_t);
static void *(*memalignp)(size_t, size_t);
static int  (*posix_memalignp)(void **, size_t, size_t);
static void (*freep)(void *);

static volatile int initialized;  // 是否已初始化标志
static __thread int thread_in_hook;  // 用于多线程的 hook 标志，

#define STATIC_CALLOC_LEN	4096
static char static_calloc_buf[STATIC_CALLOC_LEN];
static size_t static_calloc_len;
static pthread_mutex_t static_calloc_mutex = PTHREAD_MUTEX_INITIALIZER;

#define MH_HASH_BITS	20	/* 1 M entries, hardcoded for now */
#define MH_TABLE_SIZE	(1 << MH_HASH_BITS)
static struct cds_hlist_head mh_table[MH_TABLE_SIZE];  // 哈希表

struct mh_entry {  // memory hash entry
	struct cds_hlist_node hlist;
	void *ptr;
	const void *alloc_caller;
	char *caller_symbol;
	size_t alloc_size;
	const char* file;
	int line;
	const char* function;
};

/*	函数功能：从 hash 表中查询元素
*   计算元素 hash, 检查元素是否存在于 hash 表
*   如果存在，返回该 hash 表元素， 否则返回 NULL
*/
static struct mh_entry *
get_mh(const void *ptr)
{
	struct cds_hlist_head *head;
	struct cds_hlist_node *node;
	struct mh_entry *e;
	uint32_t hash;

	hash = jhash(&ptr, sizeof(ptr), 0);  // 计算 hash
	head = &mh_table[hash & (MH_TABLE_SIZE - 1)];  // 获取哈希表区块链表头节点
	cds_hlist_for_each_entry(e, node, head, hlist) {  // 遍历链表，检查 ptr 是否在链表中
		if (ptr == e->ptr)
			return e;
	}
	return NULL;  // ptr 不在链表中返回 NULL
}

/* 函数功能: 将元素添加到 hash 表中
*  首先检查元素是否已存在于 hash 表中，如果已存在，输出错误提示，
*  否则构造 hash 表元素，再将元素插入 hash 表中
*/
static void
add_mh(void *ptr, size_t alloc_size, const void *caller, const char *file, const int line, const char *func)
{
	struct cds_hlist_head *head;
	struct cds_hlist_node *node;
	struct mh_entry *e;
	uint32_t hash;
	Dl_info info;

	if (!ptr)
		return;
	hash = jhash(&ptr, sizeof(ptr), 0);
	head = &mh_table[hash & (MH_TABLE_SIZE - 1)];
	cds_hlist_for_each_entry(e, node, head, hlist) {  // 遍历链表，检查 ptr 是否已在 hash 表中
		if (ptr == e->ptr) {
			fprintf(stderr, "[warning] add_mh pointer %p is already there\n",
				ptr);
			//assert(0);	/* already there */
		}
	}
	e = malloc(sizeof(*e)); // 开辟一个 mh_entry
	e->ptr = ptr;
	e->alloc_caller = caller;
	e->alloc_size = alloc_size;
	e->file = file;
	e->line = line;
	e->function = func;
	if (dladdr(caller, &info) && info.dli_sname) {
		e->caller_symbol = strdup(info.dli_sname);
	} else {
		e->caller_symbol = NULL;
	}
	cds_hlist_add_head(&e->hlist, head);  // 将节点添加到 hash 表中
}

/* 函数功能： 从 hash 表中删除元素
*  首先检查元素是否存在于 hash 表中，如果不存在，输出错误提示 
*  否则从 hash 表中删除元素并释放内存
*/
static void
del_mh(void *ptr, const void *caller)
{
	struct mh_entry *e;

	if (!ptr)
		return;
	e = get_mh(ptr);
	if (!e) {
		fprintf(stderr,
			"[warning] trying to free unallocated ptr %p caller %p\n",
			ptr, caller);
		return;
	}
	cds_hlist_del(&e->hlist);
	free(e->caller_symbol);
	free(e);
}

/* 函数功能：初始化
*  定义函数指针，根据环境变量设置全局参数
*/
static void __attribute__((constructor))
do_init(void)
{
	char *env;

	if (initialized)
		return;
	// 定义函数指针
	callocp = (void *(*) (size_t, size_t)) dlsym (RTLD_NEXT, "calloc");
	mallocp = (void *(*) (size_t)) dlsym (RTLD_NEXT, "malloc");
	reallocp = (void *(*) (void *, size_t)) dlsym (RTLD_NEXT, "realloc");
	memalignp = (void *(*)(size_t, size_t)) dlsym (RTLD_NEXT, "memalign");
	posix_memalignp = (int (*)(void **, size_t, size_t)) dlsym (RTLD_NEXT, "posix_memalign");
	freep = (void (*) (void *)) dlsym (RTLD_NEXT, "free");
	// 读取环境变量，设置是否打印
	env = getenv("MEMLEAK_FINDER_PRINT");
	if (env && strcmp(env, "1") == 0)
		print_to_console = 1;

	initialized = 1;
}

/* 函数功能： 请求从已经预先分配的 4096 字节内存中分配内存，
*  如果申请的内存大于剩余的内存，则不进行分配，返回 NULL，
*  否则返回分配内存的首地址
*/
static
void *static_calloc(size_t nmemb, size_t size)
{
	size_t prev_len;

	pthread_mutex_lock(&static_calloc_mutex);
	// 
	if (nmemb * size > sizeof(static_calloc_buf) - static_calloc_len) {
		pthread_mutex_unlock(&static_calloc_mutex);
		return NULL;
	}
	prev_len = static_calloc_len;
	static_calloc_len += nmemb * size;
	pthread_mutex_unlock(&static_calloc_mutex);
	return &static_calloc_buf[prev_len];
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

void *
_calloc(size_t nmemb, size_t size, const char* file, const int line, const char* func)
{
	void *result;
	const void *caller = __builtin_return_address(0);
	// const char *func_name = __func__;

	if (callocp == NULL) {
		return static_calloc(nmemb, size);
	}

	do_init();

	if (thread_in_hook) {
		return callocp(nmemb, size);
	}

	thread_in_hook = 1;

	pthread_mutex_lock(&mh_mutex);

	/* Call resursively */
	result = callocp(nmemb, size);

	add_mh(result, nmemb * size, caller, file, line, func);

	/* printf might call malloc, so protect it too. */
	if (print_to_console)
		fprintf(stderr, "calloc(%zu,%zu) returns %p\n", nmemb, size, result);

	pthread_mutex_unlock(&mh_mutex);

	thread_in_hook = 0;

	return result;
}

void *
_malloc(size_t size, const char* file, const int line, const char* func)
{
	void *result;
	const void *caller = __builtin_return_address(0);
	// const char *func_name = __func__;

	do_init();

	if (thread_in_hook) {
		return mallocp(size);
	}

	thread_in_hook = 1;

	pthread_mutex_lock(&mh_mutex);

	/* Call resursively */
	result = mallocp(size);

	add_mh(result, size, caller, file, line, func);

	/* printf might call malloc, so protect it too. */
	if (print_to_console)
		fprintf(stderr, "malloc(%zu) returns %p\n", size, result);

	pthread_mutex_unlock(&mh_mutex);

	thread_in_hook = 0;

	return result;
}

void *
_realloc(void *ptr, size_t size, const char* file, const int line, const char* func)
{
	void *result;
	const void *caller = __builtin_return_address(0);
	// const char *func_name = __func__;

	/*
	 * Return NULL if called on an address returned by
	 * static_calloc(). TODO: mimick realloc behavior instead.
	 */
	if ((char *) ptr >= static_calloc_buf &&
			(char *) ptr < static_calloc_buf + STATIC_CALLOC_LEN) {
		return NULL;
	}

	do_init();

	if (thread_in_hook) {
		return reallocp(ptr, size);
	}

	thread_in_hook = 1;

	pthread_mutex_lock(&mh_mutex);

	/* Call resursively */
	result = reallocp(ptr, size);

	if (size == 0 && ptr) {
		/* equivalent to free() */
		del_mh(ptr, caller);
	} else if (result) {
		del_mh(ptr, caller);
		add_mh(result, size, caller, file, line, func);
	}

	/* printf might call malloc, so protect it too. */
	if (print_to_console)
		fprintf(stderr, "realloc(%p,%zu) returns %p\n", ptr, size, result);

	pthread_mutex_unlock(&mh_mutex);

	thread_in_hook = 0;

	return result;
}

void *
_memalign(size_t alignment, size_t size, const char* file, const int line, const char* func)
{
	void *result;
	const void *caller = __builtin_return_address(0);
	// const char *func_name = __func__;

	do_init();

	if (thread_in_hook) {
		return memalignp(alignment, size);
	}

	thread_in_hook = 1;

	pthread_mutex_lock(&mh_mutex);

	/* Call resursively */
	result = memalignp(alignment, size);

	add_mh(result, size, caller, file, line, func);

	/* printf might call malloc, so protect it too. */
	if (print_to_console)
		fprintf(stderr, "memalign(%zu,%zu) returns %p\n",
			alignment, size, result);

	pthread_mutex_unlock(&mh_mutex);

	thread_in_hook = 0;

	return result;
}

int
_posix_memalign(void **memptr, size_t alignment, size_t size, const char* file, const int line, const char* func)
{
	int result;
	const void *caller = __builtin_return_address(0);
	// const char *func_name = __func__;

	do_init();

	if (thread_in_hook) {
		return posix_memalignp(memptr, alignment, size);
	}

	thread_in_hook = 1;

	pthread_mutex_lock(&mh_mutex);

	/* Call resursively */
	result = posix_memalignp(memptr, alignment, size);

	add_mh(*memptr, size, caller, file, line, func);

	/* printf might call malloc, so protect it too. */
	if (print_to_console)
		fprintf(stderr, "posix_memalign(%p,%zu,%zu) returns %d\n",
			memptr, alignment, size, result);

	pthread_mutex_unlock(&mh_mutex);

	thread_in_hook = 0;

	return result;
}

void
free(void *ptr)
{
	const void *caller = __builtin_return_address(0);

	/*
	 * Ensure that we skip memory allocated by static_calloc().
	 */
	// 确保我们跳过 static_calloc() 分配的内存。
	if ((char *) ptr >= static_calloc_buf &&
			(char *) ptr < static_calloc_buf + STATIC_CALLOC_LEN) {
		return;
	}

	do_init();

	if (thread_in_hook) {
		freep(ptr);
		return;
	}

	thread_in_hook = 1;
	pthread_mutex_lock(&mh_mutex);

	/* Call resursively */
	freep(ptr);

	del_mh(ptr, caller);

	/* printf might call free, so protect it too. */
	if (print_to_console)
		fprintf(stderr, "freed pointer %p\n", ptr);

	pthread_mutex_unlock(&mh_mutex);
	thread_in_hook = 0;
}

/* 函数功能: 依次检查 hash 表的每个节点，获取每个节点的链表头
*  遍历链表，如果发现链表上有元素，说明存在申请了但未释放的内存单元，
*  输出相应的提示信息
*/
static __attribute__((destructor))
void print_leaks(void)
{
	unsigned long i;

	for (i = 0; i < MH_TABLE_SIZE; i++) {
		struct cds_hlist_head *head;
		struct cds_hlist_node *node;
		struct mh_entry *e;

		head = &mh_table[i];
		cds_hlist_for_each_entry(e, node, head, hlist) {
			fprintf(stderr, "[leak] file: %s line: %5d function: %s ptr: %p size: %zu caller: %p <%s>\n",
				e->file, e->line, e->function, e->ptr,
				e->alloc_size, e->alloc_caller, e->caller_symbol);
		}
	}
}