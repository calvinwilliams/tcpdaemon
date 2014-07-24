#ifndef _H_INC_
#define _H_INC_

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifdef __cplusplus
extern "C" {
#endif

#define TCPDAEMON_CALLMODE_DAEMON		1 /* 运行模式1:主守护模式 */

#include "LOGC.h"
#include "service.h"

#include "tcpdaemon.h"

/* 版本串 */
extern char		__version_tcpdaemon[] ;

/* 跨平台宏 */
#if ( defined __linux__ ) || ( defined __unix )
#define PID_T			pid_t
#define TID_T			pthread_t
#define THANDLE_T		pthread_t
#define OBJECTHANDLE		void *
#define ERRNO			errno
#define DLERROR			dlerror()
#define OPEN			open
#define CLOSE			close
#define CLOSESOCKET		close
#define VSNPRINTF		vsnprintf
#define SNPRINTF		snprintf
#define TLS			__thread
#define SOCKLEN_T		socklen_t
#define PIPE(_pipes_)		pipe(_pipes_)
#define SLEEP(_seconds_)	sleep(_seconds_)
#elif ( defined _WIN32 )
#define PID_T			long
#define TID_T			unsigned long
#define THANDLE_T		HANDLE
#define OBJECTHANDLE		HINSTANCE
#define ERRNO			GetLastError()
#define DLERROR			""
#define OPEN			_open
#define CLOSE			_close
#define CLOSESOCKET		closesocket
#define VSNPRINTF		_vsnprintf
#define SNPRINTF		_snprintf
#define TLS			__declspec( thread )
#define SOCKLEN_T		long
#define PIPE(_pipes_)		_pipe((_pipes_),256,_O_BINARY)
#define SLEEP(_seconds_)	Sleep(_seconds_*1000)
#endif

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

struct TcpdaemonServerEnv
{
	struct TcpdaemonEntryParam	*pep ;
	
	OBJECTHANDLE			so_handle ; /* 动态库打开句柄 */
	func_tcpmain			*pfunc_tcpmain ; /* 动态库入口:通讯数据协议及应用处理回调函数 */
	void				*param_tcpmain ; /* 入口参数 */
	int				listen_sock ; /* 侦听套接字 */
	
	/* 在Instance-Fork进程模型使用 */
	long				process_count ;
	
	/* 在Leader-Follow进程池模型使用 */
	int				accept_mutex ; /* accept临界区 */
	
	long				index ; /* 工作进程序号 */
	
	long				requests_per_process ; /* 工作进程当前处理数量 */
	
	PID_T				*pids ;
	PIPE_T				*alive_pipes ; /* 工作进程获知管理进程活存管道，或者说是管理进程通知工作进程结束的命令管道 */
					/* parent fd[1] -> child fd[0] */
	
	/* 在Leader-Follow线程池模型使用 */
	THANDLE_T			*thandles ;
	TID_T				*tids ;
} ;

int ParseCommandParameter( int argc , char *argv[] , struct TcpdaemonEntryParam *pep );
int CheckCommandParameter( struct TcpdaemonEntryParam *pep );

struct TcpdaemonServerEnv *DuplicateServerEnv( struct TcpdaemonServerEnv *pse );

void usage();
void version();

#if ( defined _WIN32 )
extern CRITICAL_SECTION			accept_critical_section; /* accept临界区 */
#endif

#ifdef __cplusplus
}
#endif

#endif

