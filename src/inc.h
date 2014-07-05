#ifndef _H_INC_
#define _H_INC_

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 * LastVersion : 2014-06-29	v1.0.0		创建
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define TCPDAEMON_CALLMODE_DAEMON		1 /* 运行模式:主守护模式 */

#include "tcpdaemon.h"

/* 信号量值结构 */
union semun
{
	int		val ;
	struct semid_ds	*buf ;
	unsigned short	*array ;
	struct seminfo	*__buf ;
};

/* 守护环境结构 */
typedef struct
{
	int		fd[ 2 ] ;
} pipe_t ;

struct TcpdaemonServerEnv
{
	void		*so_handle ; /* 动态库打开句柄 */
	func_tcpmain	*pfunc_tcpmain ; /* 动态库入口:通讯数据协议及应用处理回调函数 */
	void		*param_tcpmain ; /* 入口参数 */
	int		listen_sock ; /* 侦听套接字 */
	
	/* 在Instance-Fork模型使用 */
	long		process_count ;
	
	/* 在Leader-Follow模型使用 */
	int		accept_mutex ; /* accept临界区 */
	
	long		index ; /* 工作进程序号 */
	pipe_t		*alive_pipe ; /* 工作进程获知管理进程活存管道，或者说是管理进程通知工作进程结束的命令管道 */
					/* parent fd[1] -> child fd[0] */
	
	long		requests_per_process ; /* 工作进程当前处理数量 */
	
	pid_t		*pids ;
	pipe_t		*alive_pipes ;
} ;

int _main( int argc , char *argv[] , struct TcpdaemonEntryParam *pep , func_tcpmain *pfunc_tcpmain , void *param_tcpmain );

#ifdef __cplusplus
}
#endif

#endif

