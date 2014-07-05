#ifndef _H_TCPDAEMON_
#define _H_TCPDAEMON_

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 * LastVersion : 2014-06-29	v1.0.0		创建
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <time.h>
#include <sys/time.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <utmpx.h>
#include <fcntl.h>
#include <netdb.h>
#include <errno.h>
#include <signal.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <netinet/tcp.h>

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

/* 版本串 */
extern char		__version_tcpdaemon[] ;

/* 简单日志函数 */
#define LOGLEVEL_DEBUG		0
#define LOGLEVEL_INFO		1
#define LOGLEVEL_WARN		2
#define LOGLEVEL_ERROR		3
#define LOGLEVEL_FATAL		4

void SetLogFile( char *format , ... );
void SetLogLevel( int log_level );
int FatalLog( char *c_filename , long c_fileline , char *format , ... );
int ErrorLog( char *c_filename , long c_fileline , char *format , ... );
int WarnLog( char *c_filename , long c_fileline , char *format , ... );
int InfoLog( char *c_filename , long c_fileline , char *format , ... );
int DebugLog( char *c_filename , long c_fileline , char *format , ... );

/*
*	主守护模式 : tcpdaemon(main->tcpdaemon->tcpmain) + xxx.so(tcpmain)
*/

#define TCPMAIN		"tcpmain"

/* 通讯数据协议及应用处理回调函数原型 */
typedef int func_tcpmain( void *param_tcpmain , int sock , struct sockaddr *addr );

/*
*	函数调用模式 : xxx.exe(main->tcpmain) + tcpdaemon.so(tcpdaemon)
*/

#define TCPDAEMON_CALLMODE_MAIN	2 /* 运行模式:函数调用模式 */

/* 主入口参数结构 */
struct TcpdaemonEntryParam
{
	int		call_mode ;
	
	char		server_model[ 2 + 1 ] ;
	long		max_process_count ;
	long		max_requests_per_process ;
	char		ip[ 20 + 1 ] ;
	long		port ;
	char		so_pathfilename[ MAXLEN_FILENAME + 1 ] ;
	
	func_tcpmain	*pfunc_tcpmain ;
	void		*param_tcpmain ;
	
	int		log_level ;
	int		tcp_nodelay ;
	int		tcp_linger ;
} ;

/* 主入口函数 */
int tcpdaemon( struct TcpdaemonEntryParam *pcp );

/*
*	异构模式 : xxx.exe(tcpmain) + tcpdaemon.so(main->tcpdaemon->tcpmain)
*/

#define TCPDAEMON_CALLMODE_NOMAIN	3 /* 运行模式:异构模式 */

/* 入口参数结构全局配置 */
extern struct TcpdaemonEntryParam	g_TcpdaemonEntryParameter ;
extern func_tcpmain			*g_pfunc_tcpmain ;
extern void				*g_param_tcpmain ;

#ifdef __cplusplus
}
#endif

#endif

