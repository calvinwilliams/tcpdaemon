#ifndef _H_TCPDAEMON_IN_
#define _H_TCPDAEMON_IN_

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "LOGC.h"

#include "tcpdaemon.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 主守护模式 : tcpdaemon(main->tcpdaemon->tcpmain) + xxx.so(tcpmain) */
#define TCPDAEMON_CALLMODE_CALLBACK	1 /* 运行模式:主守护模式 */

/* EPOLL */
#define MAX_EPOLL_EVENTS		1024

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
} PIPE_T ;

struct TcpdaemonServerEnvirment
{
	struct TcpdaemonEntryParameter	*p_para ;
	
	OBJECTHANDLE			so_handle ; /* 动态库打开句柄 */
	func_tcpmain			*pfunc_tcpmain ; /* 动态库入口:通讯数据协议及应用处理回调函数 */
	void				*param_tcpmain ; /* 入口参数 */
	int				listen_sock ; /* 侦听套接字 */
	struct sockaddr_in		listen_addr ; /* 侦听网络地址 */
	
	PID_T				*pids ;
	PIPE_T				*alive_pipes ; /* 工作进程获知管理进程活存管道，或者说是管理进程通知工作进程结束的命令管道 */
					/* parent fd[1] -> child fd[0] */
	
	/* 在Instance-Fork进程模型使用 */
	long				process_count ;
	
	/* 在Leader-Follow进程池模型使用 */
	int				accept_mutex ; /* accept临界区 */
	long				index ; /* 工作进程序号 */
	long				requests_per_process ; /* 工作进程当前处理数量 */
	
	/* 在MultiplexIO进程池模型使用 */
	int				*epoll_array ;
	
	/* 在Leader-Follow线程池模型使用 */
	THANDLE_T			*thandles ;
	TID_T				*tids ;
} ;

#ifdef __cplusplus
}
#endif

#endif

