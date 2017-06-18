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
#include <sys/epoll.h>
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

#ifndef _WINDLL_FUNC
#if ( defined __unix ) || ( defined __linux__ )
#define _WINDLL_FUNC
#elif ( defined _WIN32 )
#define _WINDLL_FUNC		_declspec(dllexport)
#endif
#endif

/* 公共宏 */
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
#define SOCKLEN_T		socklen_t
#define PIPE(_pipes_)		pipe(_pipes_)
#define SLEEP(_seconds_)	sleep(_seconds_)
#define CHDIR			chdir
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
#define SOCKLEN_T		long
#define PIPE(_pipes_)		_pipe((_pipes_),256,_O_BINARY)
#define SLEEP(_seconds_)	Sleep(_seconds_*1000)
#define CHDIR			_chdir
#endif

/* 日志等级 */
#ifndef LOGLEVEL_DEBUG
#define LOGLEVEL_DEBUG		1
#define LOGLEVEL_INFO		2
#define LOGLEVEL_WARN		3
#define LOGLEVEL_ERROR		4
#define LOGLEVEL_FATAL		5
#endif

/* 版本号字符串 */
extern char		*__TCPDAEMON_VERSION ;

/*
 * 通讯层
 */

/* 主守护模式 : tcpdaemon(main->tcpdaemon->tcpmain) + xxx.so(tcpmain) */

#define TCPMAIN		"tcpmain"

/* 函数调用模式 : xxx.exe(main) + tcpdaemon.so(tcpdaemon) + xxx.exe(tcpmain) */

/* 通讯数据协议及应用处理回调函数原型 */
struct TcpdaemonEntryParameter ;
struct TcpdaemonServerEnvironment ;

typedef int func_tcpdaemon( struct TcpdaemonEntryParameter *p_para );

typedef int func_tcpmain( struct TcpdaemonServerEnvironment *p_env , int sock , void *p_addr );
/* 参数说明 */
	/*
	IF
					p_env , int accepted_sock , struct sockaddr *accepted_addr
	LF
					p_env , int accepted_sock , struct sockaddr *accepted_addr
	IOMP
		OnAcceptingSocket	p_env , int accepted_sock , struct sockaddr *accepted_addr
		OnClosingSocket		p_env , 0 , void *custem_data_ptr
		OnSendingSocket		p_env , 0 , void *custem_data_ptr
		OnReceivingSocket	p_env , 0 , void *custem_data_ptr
		OnClosingSocket		p_env , 0 , void *custem_data_ptr
	WIN-TLF
					p_env , int accepted_sock , struct sockaddr *accepted_addr
	*/
/* 返回值说明 */
#define TCPMAIN_RETURN_CLOSE			0
#define TCPMAIN_RETURN_WAITINGFOR_RECEIVING	1	
#define TCPMAIN_RETURN_WAITINGFOR_SENDING	2	
#define TCPMAIN_RETURN_WAITINGFOR_NEXT		3	
#define TCPMAIN_RETURN_ERROR			-1

/* 主入口参数结构 */
struct TcpdaemonEntryParameter
{
	int		daemon_level ;	/* 是否转化为守护服务 1:转化 0:不转化（缺省） */
	
	char		log_pathfilename[ 256 + 1 ] ;	/* 日志输出文件名，不设置则输出到标准输出上 */
	int		log_level ;	/* 日志等级 */
	
	char		server_model[ 10 + 1 ] ;	/* TCP连接管理模型
							LF:领导者-追随者预派生进程池模型 for UNIX,Linux
							IF:即时派生进程模型 for UNIX,Linux
							WIN-TLF:领导者-追随者预派生线程池模型 for win32
							*/
	int		process_count ;	/* 当为领导者-追随者预派生进程池模型时为工作进程池进程数量，当为即时派生进程模型时为最大子进程数量，当为IO多路复用模型时为工作进程池进程数量 */
	int		max_requests_per_process ;	/* 当为领导者-追随者预派生进程池模型时为单个工作进程最大处理应用次数 */
	char		ip[ 20 + 1 ] ;	/* 本地侦听IP */
	int		port ;	/* 本地侦听PORT */
	char		so_pathfilename[ 256 + 1 ] ;	/* 用绝对路径或相对路径表达的应用动态库文件名 */
	
	char		work_user[ 64 + 1 ] ;	/* 切换为其它用户运行。可选 */
	char		work_path[ 256 + 1 ] ;	/* 切换到指定目录运行。可选 */
	
	func_tcpmain	*pfunc_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数指针 */
	void		*param_tcpmain ;	/* 当函数调用模式时，指向把TCP连接交给应用入口函数的参数指针。特别注意：自己保证线程安全 */
	
	int		tcp_nodelay ;	/* 启用TCP_NODELAY选项 1:启用 0:不启用（缺省）。可选 */
	int		tcp_linger ;	/* 启用TCP_LINGER选项 >=1:启用并设置成参数值 0:不启用（缺省）。可选 */
	
	int		timeout_seconds ; /* 超时时间，单位：秒；目前只对IO-Multiplex模型有效 */
	int		cpu_affinity ; /* CPU亲缘性 */
	
	/* 以下为内部使用 */
	int		install_winservice ;
	int		uninstall_winservice ;
} ;

/* 主入口函数 */
_WINDLL_FUNC int tcpdaemon( struct TcpdaemonEntryParameter *p_para );

/* WINDOWS服务名 */
#define TCPDAEMON_SERVICE		"TcpDaemon Service"

/* 环境结构成员 */
void *TDGetTcpmainParameter( struct TcpdaemonServerEnvironment *p_env );
int TDGetListenSocket( struct TcpdaemonServerEnvironment *p_env );
int *TDGetListenSocketPtr( struct TcpdaemonServerEnvironment *p_env );
struct sockaddr_in TDGetListenAddress( struct TcpdaemonServerEnvironment *p_env );
struct sockaddr_in *TDGetListenAddressPtr( struct TcpdaemonServerEnvironment *p_env );
int TDGetProcessCount( struct TcpdaemonServerEnvironment *p_env );
int *TDGetEpollArrayBase( struct TcpdaemonServerEnvironment *p_env );
int TDGetThisEpoll( struct TcpdaemonServerEnvironment *p_env );

#define IOMP_ON_ACCEPTING_SOCKET	1
#define IOMP_ON_CLOSING_SOCKET		2
#define IOMP_ON_RECEIVING_SOCKET	3
#define IOMP_ON_SENDING_SOCKET		4

int TDGetIoMultiplexEvent( struct TcpdaemonServerEnvironment *p_env );
void TDSetIoMultiplexDataPtr( struct TcpdaemonServerEnvironment *p_env , void *io_multiplex_data_ptr );

#ifdef __cplusplus
}
#endif

#endif

