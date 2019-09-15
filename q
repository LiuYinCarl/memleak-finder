[33mcommit 4ca3b3e33e273f7ab1808d53b1e12b1acfa20232[m[33m ([m[1;36mHEAD -> [m[1;32mmaster[m[33m, [m[1;31morigin/master[m[33m, [m[1;31morigin/HEAD[m[33m)[m
Author: LiuYinCarl <1427518212@qq.com>
Date:   Tue Sep 3 12:58:43 2019 +0800

    实现了对C++内存泄漏检测的支持

[33mcommit 7c1033ae7d8fc27a30b8caea4086aeaf4e566d3d[m
Author: LiuYinCarl <1427518212@qq.com>
Date:   Sun Sep 1 19:13:38 2019 +0800

    修改readme

[33mcommit 090a25e53b9560bbf83a787232937a4a3c41f3d4[m
Author: LiuYinCarl <1427518212@qq.com>
Date:   Sun Sep 1 19:05:43 2019 +0800

    修改readme

[33mcommit c5642d195d911f9576ae5914ac080cc6e22e1e6b[m
Author: LiuYinCarl <1427518212@qq.com>
Date:   Sun Sep 1 18:45:29 2019 +0800

    编写完成 memleak.h

[33mcommit 9bb72686aeeca94d07c8ba54b2cf966b53daaa7a[m
Author: LiuYinCarl <1427518212@qq.com>
Date:   Sun Sep 1 16:23:42 2019 +0800

    去除了所有使用函数指针的代码，改为了直接的函数调用

[33mcommit a5aca5d43a946cde40764ca20ffd514ff5433fdb[m
Author: LiuYinCarl <1427518212@qq.com>
Date:   Sun Sep 1 15:00:14 2019 +0800

    利用 hook 实现了内存泄漏监测

[33mcommit 76d48deee356b2a5a13bf3372a51f29399aa33a2[m
Author: LiuYinCarl <1427518212@qq.com>
Date:   Sun Sep 1 01:21:29 2019 +0800

    注释文件memleak_finder.c

[33mcommit d9e6d9a57fda6d3ad73016b2b3137997aac9c7ba[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Tue Aug 6 21:41:15 2013 -0400

    Fix: static_calloc should be thread-safe
    
    Reported-by: Stefan Seefeld <stefan_seefeld@mentor.com>
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit a9d702b129d361ccb6c61d810b737e9beca6e29f[m
Author: Stefan Seefeld <stefan_seefeld@mentor.com>
Date:   Tue Aug 6 21:33:06 2013 -0400

    Fix typo in static_calloc
    
    Signed-off-by: Stefan Seefeld <stefan_seefeld@mentor.com>
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 07b2e0cd6297e5c7349793f5bfc9cc28d6c83a6b[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Mon Aug 5 13:42:29 2013 -0400

    Handle realloc/free of static_calloc
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit dc7e0aab2880640a612eed4dd65cd6bb8f4e7bd8[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Thu Jul 11 08:04:11 2013 -0400

    Add errno missing include in fdleak finder
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 451dd3d096df7fe8603e2b8110b159be87c69f0e[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Wed Jul 10 09:04:12 2013 -0400

    update: fdleak: default backtrace len = 3
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 23c0e1ab318ceba692d34387693bfdce70c07498[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Tue Jul 9 19:11:04 2013 -0400

    add memleak_finder_export_fd
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit f22edffbfcfefae293c3502aa4bdc7207d138078[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Tue Jul 9 18:11:27 2013 -0400

    Add missing fdl_printf
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 76c829f2ff48f6f9c440517558260721a306bfca[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Tue Jul 9 18:03:15 2013 -0400

    fdleak: verbose output
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit e9f1c8665506d51b4e3b9e2f4d5c85efbcfd1d53[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Tue Jul 9 17:35:55 2013 -0400

    Add epoll
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 1458dc710dc4485d82b849e801edd1db73a6b366[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Tue Jul 9 17:27:00 2013 -0400

    Add missing pipe2/pipe
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit b531bda026c8d66dad5f9291becbbbca533ef02d[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Tue Jul 9 17:09:20 2013 -0400

    Add fdleak finder
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit d4d4148679a83f5d3e6907ab7ecf39bfa0e2a20d[m
Merge: 872594c 16c83ac
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Fri Jun 21 09:10:30 2013 -0700

    Merge pull request #1 from ctuffli/master
    
    Add support for posix_memalign
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 16c83ac8940ef0ae81b47ed15127db867f5ce028[m
Author: Chuck Tuffli <chuck.tuffli@oakgatetech.com>
Date:   Fri Jun 21 08:30:36 2013 -0700

    Rename pmemalignp to match existing style
    
    Renamed pmemalignp to posix_memalign

[33mcommit 1c617297614fb9d59bf42e99621f3f9954fa2d55[m
Merge: 6077fb3 872594c
Author: Chuck Tuffli <chuck.tuffli@oakgatetech.com>
Date:   Fri Jun 21 07:09:17 2013 -0700

    Merge remote branch 'upstream/master'

[33mcommit 6077fb394e03a06314e831c8159758a1c215ba61[m
Author: Chuck Tuffli <chuck.tuffli@oakgatetech.com>
Date:   Fri Jun 21 07:01:23 2013 -0700

    Add support for the POSIX variant of memalign

[33mcommit 872594c21120f9e9385572d4591163aaae886e60[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Wed Jun 19 14:36:05 2013 -0400

    Update README
    
    Hint at debug version of libraries.
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 81f1dbc3ec75862d8c58e7fd06b1374f74d99d59[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Fri Jun 14 17:15:24 2013 -0400

    update readme
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 0cb3ad41d605810a6715dba7cbb6d86409cdf883[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Fri Jun 14 17:06:18 2013 -0400

    Add credits to README
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 89781147fa5ba5e759aa103a11f72937eb5b12d6[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Fri Jun 14 17:02:12 2013 -0400

    Remove test file
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>

[33mcommit 9b5c4fec0620830e9cc70966ee0c6373a178d197[m
Author: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
Date:   Fri Jun 14 17:00:59 2013 -0400

    Initial commit
    
    Signed-off-by: Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
