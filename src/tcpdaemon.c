#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

/* 版本串 */
char		__version_tcpdaemon[] = "1.1.0" ;

unsigned int WINAPI worker( void *pv );

struct TcpdaemonServerEnv	*g_pse = NULL ;
int				g_exit_flag = 0 ;

#if ( defined _WIN32 )
CRITICAL_SECTION		accept_critical_section; /* accept临界区 */
#endif

/* 检查命令行参数 */
int CheckCommandParameter( struct TcpdaemonEntryParam *pep )
{
#if ( defined __linux__ ) || ( defined __unix )
	if( STRCMP( pep->server_model , == , "IF" ) )
	{
		return 0;
	}
	if( STRCMP( pep->server_model , == , "LF" ) )
	{
		if( pep->max_process_count <= 0 )
		{
			ErrorLog( __FILE__ , __LINE__ , "worker poll size[%ld] invalid\n" , pep->max_process_count );
			return 1;
		}
		
		return 0;
	}
#elif ( defined _WIN32 )
	if( STRCMP( pep->server_model , == , "WIN-TLF" ) )
	{
		if( pep->max_process_count <= 0 )
		{
			ErrorLog( __FILE__ , __LINE__ , "worker poll size[%ld] invalid\n" , pep->max_process_count );
			return 1;
		}
		
		return 0;
	}
#endif
	
	ErrorLog( __FILE__ , __LINE__ , "server mode[%s] invalid\n" , pep->server_model );
	return 1;
}

struct TcpdaemonServerEnv *DuplicateServerEnv( struct TcpdaemonServerEnv *pse )
{
	struct TcpdaemonServerEnv	*pse_new = NULL ;

	pse_new = (struct TcpdaemonServerEnv *)malloc( sizeof(struct TcpdaemonServerEnv) );
	if( pse_new == NULL )
		return NULL;
	
	memcpy( pse_new , pse , sizeof(struct TcpdaemonServerEnv) );
	
	return pse_new;
}
#if ( defined __linux__ ) || ( defined __unix )

/* TERM信号回调函数 */
void sigproc_SIGTERM( int signo )
{
	g_exit_flag = 1 ;
	return;
}

/* CHLD信号回调函数 */
void sigproc_SIGCHLD( int signo )
{
	PID_T		pid ;
	int		status ;
	
	do
	{
		pid = waitpid( -1 , & status , WNOHANG ) ;
		if( pid > 0 )
		{
			g_pse->process_count--;
		}
	}
	while( pid > 0 );
	
	return;
}

#endif

/* 初始化守护环境 */
static int InitDaemonEnv( struct TcpdaemonServerEnv *pse )
{
	int		nret = 0 ;
	
	pse->listen_sock = -1 ;
	
	/* 得到通讯数据协议及应用处理回调函数指针 */
	if( pse->pep->call_mode == TCPDAEMON_CALLMODE_DAEMON )
	{
#if ( defined __linux__ ) || ( defined __unix )
		pse->so_handle = dlopen( pse->pep->so_pathfilename , RTLD_LAZY ) ;
#elif ( defined _WIN32 )
		pse->so_handle = LoadLibrary( pse->pep->so_pathfilename ) ;
#endif
		if( pse->so_handle == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "dlopen[%s]failed[%s]\n" , pse->pep->so_pathfilename , DLERROR );
			return -1;
		}
		
#if ( defined __linux__ ) || ( defined __unix )
		pse->pfunc_tcpmain = (func_tcpmain*)dlsym( pse->so_handle , TCPMAIN ) ;
#elif ( defined _WIN32 )
		pse->pfunc_tcpmain = (func_tcpmain*)GetProcAddress( pse->so_handle , TCPMAIN ) ;
#endif
		if( pse->pfunc_tcpmain == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "dlsym[%s]failed[%s]\n" , TCPMAIN , DLERROR );
			return -1;
		}
	}
	else if( pse->pep->call_mode == TCPDAEMON_CALLMODE_MAIN )
	{
		pse->pfunc_tcpmain = pse->pep->pfunc_tcpmain ;
		pse->param_tcpmain = pse->pep->param_tcpmain ;
	}
	else
	{
		ErrorLog( __FILE__ , __LINE__ , "call_mode[%d]invalid\n" , pse->pep->call_mode );
		return -1;
	}
	
	/* 创建侦听socket */
	pse->listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( pse->listen_sock == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "socket failed , ERRNO[%d]\n" , ERRNO );
		return -1;
	}
	
	{
	int	on = 1 ;
	setsockopt( pse->listen_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & on , sizeof(on) );
	}
	
	{
	struct sockaddr_in	inaddr ;
	memset( & inaddr , 0x00 , sizeof(struct sockaddr_in) );
	inaddr.sin_family = AF_INET ;
	inaddr.sin_addr.s_addr = inet_addr( pse->pep->ip ) ;
	inaddr.sin_port = htons( (unsigned short)pse->pep->port ) ;
	nret = bind( pse->listen_sock , (struct sockaddr *) & inaddr, sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "bind[%s:%ld]failed , ERRNO[%d]\n" , pse->pep->ip , pse->pep->port , ERRNO );
		return -1;
	}
	}
	
	nret = listen( pse->listen_sock , pse->pep->max_process_count * 2 ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "listen failed , ERRNO[%d]\n" , ERRNO );
		return -1;
	}
	
	return 0;
}

/* 清理守护环境 */
static int CleanDaemonEnv( struct TcpdaemonServerEnv *pse )
{
	if( pse->pep->call_mode == TCPDAEMON_CALLMODE_DAEMON )
	{
		if( pse->so_handle )
		{
#if ( defined __linux__ ) || ( defined __unix )
			dlclose( pse->so_handle );
#elif ( defined _WIN32 )
			FreeLibrary( pse->so_handle );
#endif
		}
	}
	
	if( pse->listen_sock != -1 )
	{
		CLOSE( pse->listen_sock );
	}
	
	return 0;
}

#if ( defined __linux__ ) || ( defined __unix )

/* Instance-Fork进程模型 初始化守护环境 */
static int InitDaemonEnv_IF( struct TcpdaemonServerEnv *pse )
{
	return InitDaemonEnv( pse );
}

/* Instance-Fork进程模型 清理守护环境 */
static int CleanDaemonEnv_IF( struct TcpdaemonServerEnv *pse )
{
	return CleanDaemonEnv( pse );
}

/* Instance-Fork进程模型 入口函数 */
int tcpdaemon_IF( struct TcpdaemonServerEnv *pse )
{
	struct sigaction	act , oldact ;
	
	fd_set			readfds ;
	struct timeval		tv ;
	
	struct sockaddr		accept_addr ;
	socklen_t		accept_addrlen ;
	int			accept_sock ;
	
	PID_T			pid ;
	int			status ;
	
	int			nret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_IF( pse ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "init IF failed[%d]\n" , nret );
		CleanDaemonEnv_IF( pse );
		return nret;
	}
	
	/* 设置信号灯 */
	memset( & act , 0x00 , sizeof(struct sigaction) );
	act.sa_handler = sigproc_SIGTERM ;
	sigemptyset( & (act.sa_mask) );
#ifdef SA_INTERRUPT
	act.sa_flags = SA_INTERRUPT ;
#endif
	nret = sigaction( SIGTERM , & act , & oldact );
	
	memset( & act , 0x00 , sizeof(struct sigaction) );
	act.sa_handler = & sigproc_SIGCHLD ;
	sigemptyset( & (act.sa_mask) );
	act.sa_flags = SA_RESTART ;
	nret = sigaction( SIGCHLD , & act , NULL );
	
	InfoLog( __FILE__ , __LINE__ , "parent listen starting\n" );
	
	/* 父进程侦听开始 */
	while(1)
	{
		/* 监控侦听socket事件 */
_DO_SELECT :
		FD_ZERO( & readfds );
		FD_SET( pse->listen_sock , & readfds );
		tv.tv_sec = 1 ;
		tv.tv_usec = 0 ;
		nret = select( pse->listen_sock+1 , & readfds , NULL , NULL , & tv ) ;
		if( nret == -1 )
		{
			if( ERRNO == EINTR )
			{
				if( g_exit_flag == 1 )
				{
					break;
				}
				else
				{
					goto _DO_SELECT;
				}
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "select failed , ERRNO[%d]\n" , ERRNO );
				break;
			}
		}
		else if( nret > 0 )
		{
			/* 接受新客户端连接 */
_DO_ACCEPT :
			accept_addrlen = sizeof(struct sockaddr) ;
			memset( & accept_addr , 0x00 , accept_addrlen );
			accept_sock = accept( pse->listen_sock , & accept_addr , & accept_addrlen ) ;
			if( accept_sock == -1 )
			{
				if( ERRNO == EINTR )
				{
					if( g_exit_flag == 1 )
					{
						break;
					}
					else
					{
						goto _DO_ACCEPT;
					}
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ , "accept failed , ERRNO[%d]\n" , ERRNO );
					break;
				}
			}
			else
			{
				
				DebugLog( __FILE__ , __LINE__ , "accept ok , [%d]accept[%d]\n" , pse->listen_sock , accept_sock );
			}
			
			if( pse->pep->max_process_count != 0 && pse->process_count + 1 > pse->pep->max_process_count )
			{
				ErrorLog( __FILE__ , __LINE__ , "too many sockets\n" );
				CLOSE( accept_sock );
				continue;
			}
			
			if( pse->pep->tcp_nodelay > 0 )
			{
				setsockopt( accept_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & (pse->pep->tcp_nodelay) , sizeof(int) );
			}
			
			if( pse->pep->tcp_linger > 0 )
			{
				struct linger	lg ;
				lg.l_onoff = 1 ;
				lg.l_linger = pse->pep->tcp_linger - 1 ;
				setsockopt( accept_sock , SOL_SOCKET , SO_LINGER , (void *) & lg , sizeof(struct linger) );
			}
			
			/* 创建子进程 */
_DO_FORK :
			pid = fork() ;
			if( pid == -1 )
			{
				if( ERRNO == EINTR )
				{
					if( g_exit_flag == 1 )
					{
						break;
					}
					else
					{
						goto _DO_FORK;
					}
				}
				else
				{
					ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]\n" , ERRNO );
					CLOSE( accept_sock );
					continue;
				}
			}
			else if( pid == 0 )
			{
				signal( SIGTERM , SIG_DFL );
				signal( SIGCHLD , SIG_DFL );
				
				CLOSESOCKET( pse->listen_sock );
				
				/* 调用通讯数据协议及应用处理回调函数 */
				InfoLog( __FILE__ , __LINE__ , "call tcpmain sock[%d]\n" , accept_sock );
				nret = pse->pfunc_tcpmain( pse->pep->param_tcpmain , accept_sock , & accept_addr ) ;
				if( nret < 0 )
				{
					ErrorLog( __FILE__ , __LINE__ , "tcpmain return[%d]\n" , nret );
				}
				else if( nret > 0 )
				{
					WarnLog( __FILE__ , __LINE__ , "tcpmain return[%d]\n" , nret );
				}
				else
				{
					InfoLog( __FILE__ , __LINE__ , "tcpmain return[%d]\n" , nret );
				}
				
				/* 关闭客户端连接 */
				CLOSESOCKET( accept_sock );
				DebugLog( __FILE__ , __LINE__ , "CLOSE[%d]\n" , accept_sock );
				
				/* 子进程退出 */
				DebugLog( __FILE__ , __LINE__ , "child exit\n" );
				exit(0);
			}
			else
			{
				CLOSESOCKET( accept_sock );
				pse->process_count++;
			}
		}
		else
		{
			do
			{
				pid = waitpid( -1 , & status , WNOHANG ) ;
				if( pid > 0 )
				{
					g_pse->process_count--;
				}
			}
			while( pid > 0 );
		}
	}
	
	sigaction( SIGTERM , & oldact , NULL );
	
	InfoLog( __FILE__ , __LINE__ , "parent listen ended\n" );
	
	InfoLog( __FILE__ , __LINE__ , "waiting for all children exit starting\n" );
	
	while( pse->process_count > 0 )
	{
		waitpid( -1 , & status , 0 );
	}
	
	InfoLog( __FILE__ , __LINE__ , "waiting for all chdilren exit ended\n" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_IF( pse );
	
	return 0;
}

/* Leader-Follower进程池模型 初始化守护环境 */
static int InitDaemonEnv_LF( struct TcpdaemonServerEnv *pse )
{
	int		nret = 0 ;
	
	nret = InitDaemonEnv( pse ) ;
	if( nret )
		return nret;
	
	/* 创建accept临界区 */
	pse->accept_mutex = semget( IPC_PRIVATE , 1 , IPC_CREAT | 00777 ) ;
	if( pse->accept_mutex == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "create mutex failed\n" );
		return -1;
	}
	
	{
	union semun	semopts ;
	semopts.val = 1 ;
	nret = semctl( pse->accept_mutex , 0 , SETVAL , semopts ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "set mutex failed\n" );
		return -1;
	}
	}
	
	/* 创建pid跟踪信息数组 */
	pse->pids = (pid_t*)malloc( sizeof(pid_t) * (pse->pep->max_process_count+1) ) ;
	if( pse->pids == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]\n" , ERRNO );
		return -1;
	}
	memset( pse->pids , 0x00 , sizeof(pid_t) * (pse->pep->max_process_count+1) );
	
	/* 创建存活管道信息数组 */
	pse->alive_pipes = (PIPE_T*)malloc( sizeof(PIPE_T) * (pse->pep->max_process_count+1) ) ;
	if( pse->alive_pipes == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]\n" , ERRNO );
		return -1;
	}
	memset( pse->alive_pipes , 0x00 , sizeof(PIPE_T) * (pse->pep->max_process_count+1) );
	
	return 0;
}

/* Leader-Follower进程池模型 清理守护环境 */
static int CleanDaemonEnv_LF( struct TcpdaemonServerEnv *pse )
{
	/* 释放内存 */
	if( pse->accept_mutex )
	{
		semctl( pse->accept_mutex , 0 , IPC_RMID , 0 );
	}
	
	if( pse->pids )
	{
		free( pse->pids );
	}
	
	if( pse->alive_pipes )
	{
		free( pse->alive_pipes );
	}
	
	return CleanDaemonEnv( pse );
}

/* Leader-Follow进程池模型 入口函数 */
int tcpdaemon_LF( struct TcpdaemonServerEnv *pse )
{
	PID_T			pid ;
	int			status ;
	
	struct sigaction	act , oldact ;
	
	int			nret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_LF( pse ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "init LF failed[%d]\n" , nret );
		CleanDaemonEnv_LF( pse );
		return nret;
	}
	
	/* 设置信号灯 */
	memset( & act , 0x00 , sizeof(struct sigaction) );
	act.sa_handler = & sigproc_SIGTERM ;
#ifdef SA_INTERRUPT
	act.sa_flags = SA_INTERRUPT ;
#endif
	sigaction( SIGTERM , & act , & oldact );
	
	signal( SIGCHLD , SIG_DFL );
	
	/* 创建工作进程池 */
	InfoLog( __FILE__ , __LINE__ , "create worker pool starting\n" );
	
	for( pse->index = 1 ; pse->index <= pse->pep->max_process_count ; pse->index++ )
	{
		nret = pipe( pse->alive_pipes[pse->index-1].fd ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "pipe failed , ERRNO[%d]\n" , ERRNO );
			CleanDaemonEnv_LF( pse );
			return -11;
		}
		
		pse->pids[pse->index-1] = fork() ;
		if( pse->pids[pse->index-1] == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]\n" , ERRNO );
			CleanDaemonEnv_LF( pse );
			return -11;
		}
		else if( pse->pids[pse->index-1] == 0 )
		{
			signal( SIGTERM , SIG_DFL );
			
			CLOSE( pse->alive_pipes[pse->index-1].fd[1] );
			worker( pse );
			CLOSE( pse->alive_pipes[pse->index-1].fd[0] );
			
			CLOSESOCKET( pse->listen_sock );
			
			exit(0);
		}
		else
		{
			CLOSE( pse->alive_pipes[pse->index-1].fd[0] );
		}
		
		SLEEP(1);
	}
	
	InfoLog( __FILE__ , __LINE__ , "create worker pool ended\n" );
	
	/* 监控工作进程池 */
	InfoLog( __FILE__ , __LINE__ , "monitoring all children starting\n" );
	
	while(1)
	{
		/* 监控工作进程结束事件 */
		pid = waitpid( -1 , & status , 0 ) ;
		if( pid == -1 )
		{
			break;
		}
		
		for( pse->index = 1 ; pse->index <= pse->pep->max_process_count ; pse->index++ )
		{
			if( pse->pids[pse->index-1] == pid )
			{
				/* 重启工作进程 */
				CLOSE( pse->alive_pipes[pse->index-1].fd[1] );
				
				InfoLog( __FILE__ , __LINE__ , "detecting child[%ld]pid[%ld] exit , rebooting\n" , pse->index , (long)pid );
				
				sleep(1);
				
				pse->requests_per_process = 0 ;
				
				nret = pipe( pse->alive_pipes[pse->index-1].fd ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "pipe failed , ERRNO[%d]\n" , ERRNO );
					CleanDaemonEnv_LF( pse );
					return -11;
				}
				
				pse->pids[pse->index-1] = fork() ;
				if( pse->pids[pse->index-1] == -1 )
				{
					ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]\n" , ERRNO );
					CleanDaemonEnv_LF( pse );
					return -11;
				}
				else if( pse->pids[pse->index-1] == 0 )
				{
					signal( SIGTERM , SIG_DFL );
					
					CLOSE( pse->alive_pipes[pse->index-1].fd[1] );
					worker( pse );
					CLOSE( pse->alive_pipes[pse->index-1].fd[0] );
					
					CLOSESOCKET( pse->listen_sock );
					
					exit(0);
				}
				else
				{
					CLOSE( pse->alive_pipes[pse->index-1].fd[0] );
				}
				
				break;
			}
		}
		if( pse->index > pse->pep->max_process_count )
		{
			InfoLog( __FILE__ , __LINE__ , "detecting unknow child pid[%ld] exit\n" , (long)pid );
			continue;
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "monitoring all children ended\n" );
	
	sigaction( SIGTERM , & oldact , NULL );
	
	/* 销毁进程池 */
	InfoLog( __FILE__ , __LINE__ , "destroy worker poll starting\n" );
	
	for( pse->index = 1 ; pse->index <= pse->pep->max_process_count ; pse->index++ )
	{
		write( pse->alive_pipes[pse->index-1].fd[1] , "\0" , 1 );
	}
	
	for( pse->index = 1 ; pse->index <= pse->pep->max_process_count ; pse->index++ )
	{
		waitpid( -1 , & status , 0 );
		CLOSE( pse->alive_pipes[pse->index-1].fd[1] );
	}
	
	InfoLog( __FILE__ , __LINE__ , "destroy worker poll ended\n" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_LF( pse );
	
	return 0;
}

#endif

#if ( defined _WIN32 )

/* Leader-Follower线程池模型 初始化守护环境 */
static int InitDaemonEnv_WIN_TLF( struct TcpdaemonServerEnv *pse )
{
	int		nret = 0 ;
	
	nret = InitDaemonEnv( pse ) ;
	if( nret )
		return nret;
	
	/* 初始化临界区 */
	InitializeCriticalSection( & accept_critical_section ); 
	
	/* 创建tid跟踪信息数组 */
	pse->thandles = (THANDLE_T*)malloc( sizeof(THANDLE_T) * (pse->pep->max_process_count+1) ) ;
	if( pse->thandles == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]\n" , ERRNO );
		return -1;
	}
	memset( pse->thandles , 0x00 , sizeof(THANDLE_T) * (pse->pep->max_process_count+1) );
	
	pse->tids = (TID_T*)malloc( sizeof(TID_T) * (pse->pep->max_process_count+1) ) ;
	if( pse->tids == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]\n" , ERRNO );
		return -1;
	}
	memset( pse->tids , 0x00 , sizeof(TID_T) * (pse->pep->max_process_count+1) );
	
	return 0;
}

/* Leader-Follower线程池模型 清理守护环境 */
static int CleanDaemonEnv_WIN_TLF( struct TcpdaemonServerEnv *pse )
{
	/* 初始化临界区 */
	DeleteCriticalSection( & accept_critical_section );
	
	/* 释放内存 */
	if( pse->thandles )
	{
		free( pse->thandles );
	}

	if( pse->tids )
	{
		free( pse->tids );
	}
	
	return CleanDaemonEnv( pse );
}

/* Leader-Follow线程池模型for win32 入口函数 */
int tcpdaemon_WIN_TLF( struct TcpdaemonServerEnv *pse )
{
	struct TcpdaemonServerEnv	*pse_new = NULL ;
	unsigned long			index ;
	
	int				nret = 0 ;
	long				lret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_WIN_TLF( pse ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "init WIN-TLF failed[%d]\n" , nret );
		CleanDaemonEnv_WIN_TLF( pse );
		return nret;
	}
	
	/* 创建工作进程池 */
	InfoLog( __FILE__ , __LINE__ , "create worker pool starting\n" );
	
	for( pse->index = 1 ; pse->index <= pse->pep->max_process_count ; pse->index++ )
	{
		pse_new = DuplicateServerEnv( pse ) ;
		if( pse_new == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "DuplicateServerEnv failed , ERRNO[%d]\n" , ERRNO );
			CleanDaemonEnv_WIN_TLF( pse );
			return -11;
		}
		
		pse->thandles[pse->index-1] = (THANDLE_T)_beginthreadex( NULL , 0 , worker , pse_new , 0 , & (pse->tids[pse->index-1]) ) ;
		if( pse->thandles[pse->index-1] == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "_beginthreadex failed , ERRNO[%d]\n" , ERRNO );
			CleanDaemonEnv_WIN_TLF( pse );
			return -12;
		}
		
		SLEEP(1);
	}
	
	InfoLog( __FILE__ , __LINE__ , "create worker pool ended\n" );
	
	/* 监控工作线程池 */
	InfoLog( __FILE__ , __LINE__ , "monitoring all children starting\n" );
	
	while(1)
	{
		/* 监控工作线程结束事件 */
		index = WaitForMultipleObjects( pse->pep->max_process_count , pse->thandles , FALSE , INFINITE ) ;
		if( index == WAIT_FAILED )
		{
			ErrorLog( __FILE__ , __LINE__ , "WaitForMultipleObjects failed , errno[%d]\n" , ERRNO );
			break;
		}
		index = index - WAIT_OBJECT_0 + 1 ;
		CloseHandle( pse->thandles[index-1] );
		
		/* 重启线程进程 */
		InfoLog( __FILE__ , __LINE__ , "detecting child[%ld]tid[%ld] exit , rebooting\n" , index , pse->tids[index-1] );
		
		Sleep(1000);
		
		pse->requests_per_process = 0 ;
		
		pse_new = DuplicateServerEnv( pse ) ;
		if( pse_new == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "DuplicateServerEnv failed , ERRNO[%d]\n" , ERRNO );
			CleanDaemonEnv_WIN_TLF( pse );
			return -11;
		}
		
		pse->thandles[index-1] = (THANDLE_T)_beginthreadex( NULL , 0 , worker , pse_new , 0 , & (pse->tids[index-1]) ) ;
		if( pse->thandles[index-1] == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "_beginthreadex failed , ERRNO[%d]\n" , ERRNO );
			CleanDaemonEnv_WIN_TLF( pse );
			return -12;
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "monitoring all children ended\n" );
	
	/* 销毁线程池 */
	InfoLog( __FILE__ , __LINE__ , "destroy worker poll starting\n" );
	
	for( pse->index = 1 ; pse->index <= pse->pep->max_process_count ; pse->index++ )
	{
		lret = WaitForMultipleObjects( pse->pep->max_process_count , pse->thandles , TRUE , INFINITE ) ;
		if( lret == WAIT_FAILED )
		{
			break;
		}	
	}
	
	InfoLog( __FILE__ , __LINE__ , "destroy worker poll ended\n" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_WIN_TLF( pse );
	
	return 0;
}

#endif

/* 主入口函数 */
int tcpdaemon( struct TcpdaemonEntryParam *pep )
{
	struct TcpdaemonServerEnv	se ;
	
	int				nret = 0 ;
	
	memset( & se , 0x00 , sizeof(struct TcpdaemonServerEnv) );
	se.pep = pep ;
	g_pse = & se ;
	
	/* 设置日志环境 */
	SetLogFile( pep->log_pathfilename );
	SetLogLevel( pep->log_level );
	InfoLog( __FILE__ , __LINE__ , "tcpdaemon startup - version %s\n" , __version_tcpdaemon );
	
	/* 检查入口参数 */
	nret = CheckCommandParameter( pep ) ;
	if( nret )
	{
		return nret;
	}
	
#if ( defined __linux__ ) || ( defined __unix )
	if( STRCMP( pep->server_model , == , "IF" ) )
	{
		/* 进入 Instance-Fork进程模型 入口函数 */
		nret = tcpdaemon_IF( & se ) ;
	}
	if( STRCMP( pep->server_model , == , "LF" ) )
	{
		/* 进入 Leader-Follow进程池模型 入口函数 */
		nret = tcpdaemon_LF( & se ) ;
	}
#elif ( defined _WIN32 )
	if( STRCMP( pep->server_model , == , "WIN-TLF" ) )
	{
		/* 进入 Leader-Follow线程池模型 入口函数 */
		nret = tcpdaemon_WIN_TLF( & se ) ;
	}
#endif
	
	InfoLog( __FILE__ , __LINE__ , "tcpdaemon ended - version %s\n" , __version_tcpdaemon );
	
	return nret;
}

