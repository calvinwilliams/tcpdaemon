#ifndef _H_TCPDAEMON_
#define _H_TCPDAEMON_

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#if ( defined __linux__ ) || ( defined __unix )
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
#include <unistd.h>
#include <dlfcn.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <utmpx.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <pwd.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/stat.h>
#elif ( defined _WIN32 )
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <windows.h>
#include <io.h>
#include <process.h>
#include <direct.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if ( defined __unix ) || ( defined __linux__ )
#define _WINDLL_FUNC
#elif ( defined _WIN32 )
#define _WINDLL_FUNC		_declspec(dllexport)
#endif

/* 版本号字符串 */
extern char		*__TCPDAEMON_VERSION ;

/*
 * 工具层
 */


/*
 * 通讯层
 */

/* 主守护模式 : tcpdaemon(main->tcpdaemon->tcpmain) + xxx.so(tcpmain) */

#define TCPMAIN		"tcpmain"

/* 通讯数据协议及应用处理回调函数原型 */
typedef int func_tcpmain( void *param_tcpmain , int sock , struct sockaddr *addr );

/* 函数调用模式 : xxx.exe(main->tcpmain) + tcpdaemon.so(tcpdaemon) */

#define TCPDAEMON_CALLMODE_MAIN		2 /* 运行模式2:函数调用模式 */

/* 主入口参数结构 */
struct TcpdaemonEntryParameter
{
	int		daemon_level ;	/* 是否转化为守护服务 1:转化 0:不转化（缺省） */
	
	char		log_pathfilename[ MAXLEN_FILENAME + 1 ] ;	/* 日志输出文件名，不设置则输出到标准输出上 */
	int		log_level ;	/* 日志等级，不设置则缺省DEBUG等级 */
	
	int		call_mode ;	/* 应用接口模式
					   TCPDAEMON_CALLMODE_DAEMON:主守护模式
					   TCPDAEMON_CALLMODE_MAIN:函数调用模式
					*/
	
	char		server_model[ 10 + 1 ] ;	/* TCP连接管理模型
							LF:领导者-追随者预派生进程池模型 for UNIX,Linux
							IF:即时派生进程模型 for UNIX,Linux
							WIN-TLF:领导者-追随者预派生线程池模型 for win32
							*/
	long		max_process_count ;	/* 当为领导者-追随者预派生进程池模型时为工作进程池进程数量，当为即时派生进程模型时为最大子进程数量 */
	long		max_requests_per_process ;	/* 当为领导者-追随者预派生进程池模型时为单个工作进程最大处理应用次数 */
	char		ip[ 20 + 1 ] ;	/* 本地侦听IP */
	long		port ;	/* 本地侦听PORT */
	char		so_pathfilename[ MAXLEN_FILENAME + 1 ] ;	/* 用绝对路径或相对路径表达的应用动态库文件名 */
	
	char		work_user[ 64 + 1 ] ;
	char		work_path[ MAXLEN_FILENAME + 1 ] ;
	
	func_tcpmain	*pfunc_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数指针 */
	void		*param_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数的参数指针，特别注意：自己保证线程安全 */
	
	int		tcp_nodelay ;	/* 启用TCP_NODELAY选项 1:启用 0:不启用（缺省） */
	int		tcp_linger ;	/* 启用TCP_LINGER选项 1:启用并设置成参数值-1值 0:不启用（缺省） */
	
	/* 以下为内部使用 */
	int		install_winservice ;
	int		uninstall_winservice ;
} ;

/* 主入口函数 */
_WINDLL_FUNC int tcpdaemon( struct TcpdaemonEntryParameter *p_para );

/* WINDOWS服务名 */
#define TCPDAEMON_SERVICE		"TcpDaemon Service"

#ifdef __cplusplus
}
#endif

#endif

