#ifndef _H_TCPDAEMON_
#define _H_TCPDAEMON_

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 * LastVersion : 2014-04-27	v1.0.1		创建
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#if ( defined __linux__ ) || ( defined __unix )
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <utmpx.h>
#include <netdb.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <netinet/tcp.h>
#include <pwd.h>
#elif ( defined _WIN32 )
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>
#endif

#if ( defined __unix ) || ( defined __linux__ )
#ifndef _WINDLL_FUNC
#define _WINDLL_FUNC
#endif
#elif ( defined _WIN32 )
#ifndef _WINDLL_FUNC
#define _WINDLL_FUNC		_declspec(dllexport)
#endif
#endif

#ifndef WINAPI
#define WINAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 公共宏 */
#ifndef MAXLEN_FILENAME
#define MAXLEN_FILENAME			256
#endif

#ifndef STRCMP
#define STRCMP(_a_,_C_,_b_) ( strcmp(_a_,_b_) _C_ 0 )
#define STRNCMP(_a_,_C_,_b_,_n_) ( strncmp(_a_,_b_,_n_) _C_ 0 )
#endif

#ifndef MEMCMP
#define MEMCMP(_a_,_C_,_b_,_n_) ( memcmp(_a_,_b_,_n_) _C_ 0 )
#endif

/* 跨平台宏 */
#if ( defined __linux__ ) || ( defined __unix )
#define RECV			recv
#define SEND			send
#elif ( defined _WIN32 )
#define RECV			recv
#define SEND			send
#endif

/* 简单日志函数 */
#ifndef LOGLEVEL_DEBUG
#define LOGLEVEL_DEBUG		0
#define LOGLEVEL_INFO		1
#define LOGLEVEL_WARN		2
#define LOGLEVEL_ERROR		3
#define LOGLEVEL_FATAL		4
#endif

/*
*	主守护模式 : tcpdaemon(main->tcpdaemon->tcpmain) + xxx.so(tcpmain)
*/

#define TCPMAIN		"tcpmain"

/* 通讯数据协议及应用处理回调函数原型 */
typedef int func_tcpmain( void *param_tcpmain , int sock , struct sockaddr *addr );

/*
*	函数调用模式 : xxx.exe(main->tcpmain) + tcpdaemon.so(tcpdaemon)
*/

#define TCPDAEMON_CALLMODE_MAIN	2 /* 运行模式2:函数调用模式 */

/* 主入口参数结构 */
struct TcpdaemonEntryParam
{
	int		daemon_level ;	/* 是否转化为守护服务 1:转化 0:不转化（缺省） */
					/* Whether into daemon service 1:YES 0:NO(DEFAULT) */
	
	char		log_pathfilename[ MAXLEN_FILENAME + 1 ] ;	/* 日志输出文件名，不设置则输出到标准输出上 */
									/* default to standard output  */
	int		log_level ;	/* 日志等级，不设置则缺省DEBUG等级 */
					/* default the DEBUG level */
	
	int		call_mode ;	/* 应用接口模式
					   TCPDAEMON_CALLMODE_DAEMON:主守护模式
					   TCPDAEMON_CALLMODE_MAIN:函数调用模式
					*/
					/* Application interface pattern
					   TCPDAEMON_CALLMODE_DAEMON:Main guardian mode
					   TCPDAEMON_CALLMODE_MAIN:Call function mode
					*/
	
	char		server_model[ 10 + 1 ] ;	/* TCP连接管理模型
							LF:领导者-追随者预派生进程池模型 for UNIX,Linux
							IF:即时派生进程模型 for UNIX,Linux
							WIN-TLF:领导者-追随者预派生线程池模型 for win32
							*/
							/* TCP connection management model
							LF:Leader-Follower process pool model for UNIX,Linux
							IF:Instant derived process model for UNIX,Linux
							WIN-TLF:Leader-Follower thread pool model for win32
							*/
	long		max_process_count ;	/* 当为领导者-追随者预派生进程池模型时为工作进程池进程数量，当为即时派生进程模型时为最大子进程数量 */
						/* work process amount on Leader-Follower process pool model , or maximum work process amount on instant derived process model */
	long		max_requests_per_process ;	/* 当为领导者-追随者预派生进程池模型时为单个工作进程最大处理应用次数 */
							/* work process maximum application processing times on Leader-Follower process pool model */
	char		ip[ 20 + 1 ] ;	/* 本地侦听IP */
					/* local listen IP */
	long		port ;	/* 本地侦听PORT */
				/* local listen PORT */
	char		so_pathfilename[ MAXLEN_FILENAME + 1 ] ;	/* 用绝对路径或相对路径表达的应用动态库文件名 */
									/* file name of dynamic link library */
	
	char		work_user[ 64 + 1 ] ;
	char		work_path[ MAXLEN_FILENAME + 1 ] ;
	
	func_tcpmain	*pfunc_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数指针 */
						/* tcpmain function pointer */
	void		*param_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数的参数指针 */
						/* tcpmain function parameter */
	
	int		tcp_nodelay ;	/* 启用TCP_NODELAY选项 1:启用 0:不启用（缺省） */
					/* Enable TCP_NODELAY option 1:enable 0:disable(DEFAULT) */
	int		tcp_linger ;	/* 启用TCP_LINGER选项 1:启用并设置成参数值-1值 0:不启用（缺省） */
					/* Enable TCP_LINGER option 1:enable 0:disable(DEFAULT) */
	
	/* 以下为内部使用 */
	/* The following for internal use */
	int		install_winservice ;
	int		uninstall_winservice ;
} ;

/* 主入口函数 */
_WINDLL_FUNC int tcpdaemon( struct TcpdaemonEntryParam *pcp );

#ifdef __cplusplus
}
#endif

#endif

