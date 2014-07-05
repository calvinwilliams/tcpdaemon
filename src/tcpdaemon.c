#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 * LastVersion : 2014-06-29	v1.0.0		创建
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

/* 版本串 */
char		__version_tcpdaemon[] = "1.0.0" ;

int worker( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse );

/* TERM信号回调函数 */
void sigproc_SIGTERM( int signo )
{
	InfoLog( __FILE__ , __LINE__ , "signal[%d] received\n" , signo );
	return;
}

/* 初始化守护环境 */
static int InitDaemonEnv( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	int		nret = 0 ;
	
	memset( pse , 0x00 , sizeof(struct TcpdaemonServerEnv) );
	pse->listen_sock = -1 ;
	
	/* 得到通讯数据协议及应用处理回调函数指针 */
	if( pep->call_mode == TCPDAEMON_CALLMODE_DAEMON )
	{
		pse->so_handle = dlopen( pep->so_pathfilename , RTLD_LAZY ) ;
		if( pse->so_handle == NULL )
		{
			InfoLog( __FILE__ , __LINE__ , "dlopen[%s]failed[%s]\n" , pep->so_pathfilename , dlerror() );
			return -1;
		}
		
		pse->pfunc_tcpmain = dlsym( pse->so_handle , TCPMAIN ) ;
		if( pse->pfunc_tcpmain == NULL )
		{
			InfoLog( __FILE__ , __LINE__ , "dlsym[%s]failed[%s]\n" , TCPMAIN , dlerror() );
			return -1;
		}
	}
	else if( pep->call_mode == TCPDAEMON_CALLMODE_MAIN )
	{
		pse->pfunc_tcpmain = pep->pfunc_tcpmain ;
		pse->param_tcpmain = pep->param_tcpmain ;
	}
	else if( pep->call_mode == TCPDAEMON_CALLMODE_NOMAIN )
	{
		pse->pfunc_tcpmain = pep->pfunc_tcpmain ;
		pse->param_tcpmain = pep->param_tcpmain ;
	}
	else
	{
		InfoLog( __FILE__ , __LINE__ , "run mode[%d]invalid\n" , pep->call_mode );
		return -1;
	}
	
	/* 创建侦听socket */
	pse->listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( pse->listen_sock == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "socket failed , errno[%d]\n" , errno );
		return -1;
	}
	
	{
	int	on ;
	setsockopt( pse->listen_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & on , sizeof(on) );
	}
	
	{
	struct sockaddr_in	inaddr ;
	memset( & inaddr , 0x00 , sizeof(struct sockaddr_in) );
	inaddr.sin_family = AF_INET ;
	inaddr.sin_addr.s_addr = inet_addr( pep->ip ) ;
	inaddr.sin_port = htons( (unsigned short)pep->port ) ;
	nret = bind( pse->listen_sock , (struct sockaddr *) & inaddr, sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "bind[%s:%ld]failed , errno[%d]\n" , pep->ip , pep->port , errno );
		return -1;
	}
	}
	
	nret = listen( pse->listen_sock , pep->max_process_count * 2 ) ;
	if( nret == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "listen failed , errno[%d]\n" , errno );
		return -1;
	}
	
	return 0;
}

/* Instance-Fork模型 初始化守护环境 */
/* 清理守护环境 */
static int CleanDaemonEnv( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	if( pep->call_mode == TCPDAEMON_CALLMODE_DAEMON )
	{
		if( pse->so_handle )
		{
			dlclose( pse->so_handle );
		}
	}
	
	if( pse->listen_sock != -1 )
	{
		close( pse->listen_sock );
	}
	
	return 0;
}

/* 转换为守护进程 */
static int ConvertToDaemonMode( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	pid_t		pid ;
	int		sig ;
	int		fd ;
	
	pid = fork() ;
	if( pid == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "fork failed , errno[%d]\n" , errno );
		return -11;
	}
	else if( pid > 0 )
	{
		exit(0);
	}
	
	setsid();
	signal( SIGHUP,SIG_IGN );
	
	pid = fork() ;
	if( pid == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "fork failed , errno[%d]\n" , errno );
		return -11;
	}
	else if( pid > 0 )
	{
		if( pep->call_mode == TCPDAEMON_CALLMODE_DAEMON )
		{
			InfoLog( __FILE__ , __LINE__ , "tcpdaemon startup\n" );
		}
		exit(0);
	}
	else
	{
		SetLogFile( "%s/log/tcpdaemon.log" , getenv("HOME") );
		
		InfoLog( __FILE__ , __LINE__ , "tcpdaemon startup\n" );
	}
	
	setuid( getpid() ) ;
	chdir("/tmp");
	umask( 0 ) ;
	
	for( sig = 0 ; sig < 30 ; sig++ )
	{
		signal( sig , SIG_IGN );
	}
	
	for( fd = 0 ; fd <= 2 ; fd++ )
	{
		close( fd );
	}
	
	return 0;
}

static int InitDaemonEnv_IF( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	return InitDaemonEnv( pep , pse );
}

/* Instance-Fork模型 清理守护环境 */
static int CleanDaemonEnv_IF( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	return CleanDaemonEnv( pep , pse );
}

/* Instance-Fork模型 入口函数 */
int tcpdaemon_IF( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	struct timeval		tv ;
	
	struct sigaction	act , oldact ;
	
	fd_set			readfds ;
	
	struct sockaddr		accept_addr ;
	socklen_t		accept_addrlen ;
	int			accept_sock ;
	
	pid_t			pid ;
	int			status ;
	
	int			nret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_IF( pep , pse ) ;
	if( nret )
	{
		InfoLog( __FILE__ , __LINE__ , "init IF failed[%d]\n" , nret );
		CleanDaemonEnv_IF( pep , pse );
		return nret;
	}
	
	/* 转换为守护进程 */
	nret = ConvertToDaemonMode( pep , pse ) ;
	if( nret )
	{
		InfoLog( __FILE__ , __LINE__ , "convert to daemon failed[%d]\n" , nret );
		CleanDaemonEnv_IF( pep , pse );
		return nret;
	}
	
	/* 父进程侦听开始 */
	memset( & act , 0x00 , sizeof(struct sigaction) );
	act.sa_handler = & sigproc_SIGTERM ;
	act.sa_flags = 0 ;
	sigaction( SIGTERM , & act , & oldact );
	
	signal( SIGCHLD , SIG_DFL );
	
	InfoLog( __FILE__ , __LINE__ , "parent listen starting\n" );
	
	while(1)
	{
		/* 监控侦听socket事件 */
		FD_ZERO( & readfds );
		FD_SET( pse->listen_sock , & readfds );
		tv.tv_sec = 1 ;
		tv.tv_usec = 0 ;
		nret = select( pse->listen_sock+1 , & readfds , NULL , NULL , & tv ) ;
		if( nret == -1 )
		{	
			if( errno == EINTR )
			{
				break;
			}
			else
			{
				InfoLog( __FILE__ , __LINE__ , "select failed , errno[%d]\n" , errno );
				break;
			}
		}
		else if( nret > 0 )
		{
			/* 接受新客户端连接 */
			accept_addrlen = sizeof(struct sockaddr) ;
			memset( & accept_addr , 0x00 , accept_addrlen );
			accept_sock = accept( pse->listen_sock , & accept_addr , & accept_addrlen ) ;
			if( accept_sock == -1 )
			{
				InfoLog( __FILE__ , __LINE__ , "accept failed , errno[%d]\n" , errno );
				break;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "accept ok , [%d]accept[%d]\n" , pse->listen_sock , accept_sock );
			}
			
			if( pep->max_process_count != -1 && pse->process_count + 1 > pep->max_process_count )
			{
				close( accept_sock );
				continue;
			}
			
			setsockopt( accept_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & (pep->tcp_nodelay) , sizeof(int) );
			
			{
			struct linger	lg ;
			if( pep->tcp_linger > 0 )
			{
				lg.l_onoff = 1 ;
				lg.l_linger = pep->tcp_linger - 1 ;
				setsockopt( accept_sock , SOL_SOCKET , SO_LINGER , (void *) & lg , sizeof(struct linger) );
			}
			}
			
			/* 创建子进程 */
			pid = fork() ;
			if( pid == -1 )
			{
				InfoLog( __FILE__ , __LINE__ , "fork failed , errno[%d]\n" , errno );
				continue;
			}
			else if( pid == 0 )
			{
				signal( SIGTERM , SIG_DFL );
				
				close( pse->listen_sock );
				
				/* 调用通讯数据协议及应用处理回调函数 */
				DebugLog( __FILE__ , __LINE__ , "调用tcpmain,sock[%d]\n" , accept_sock );
				nret = pse->pfunc_tcpmain( pep->param_tcpmain , accept_sock , & accept_addr ) ;
				if( nret < 0 )
				{
					InfoLog( __FILE__ , __LINE__ , "tcpmain return[%d]\n" , nret );
				}
				else if( nret > 0 )
				{
					InfoLog( __FILE__ , __LINE__ , "tcpmain return[%d]\n" , nret );
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "tcpmain return[%d]\n" , nret );
				}
				
				/* 关闭客户端连接 */
				close( accept_sock );
				DebugLog( __FILE__ , __LINE__ , "close[%d]\n" , accept_sock );
				
				/* 子进程退出 */
				DebugLog( __FILE__ , __LINE__ , "child exit\n" );
				exit(0);
			}
			else
			{
				close( accept_sock );
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
					DebugLog( __FILE__ , __LINE__ , "waitpid ok , pid[%ld]\n" , (long)pid );
					pse->process_count--;
				}
			}
			while( pid > 0 );
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "parent listen ended\n" );
	
	InfoLog( __FILE__ , __LINE__ , "waiting for all children exit starting\n" );
	
	while( pse->process_count > 0 )
	{
		waitpid( -1 , & status , 0 );
	}
	
	InfoLog( __FILE__ , __LINE__ , "waiting for all chdilren exit ended\n" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_IF( pep , pse );
	
	return 0;
}

/* Leader-Follower模型 初始化守护环境 */
static int InitDaemonEnv_LF( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	int		nret = 0 ;
	
	nret = InitDaemonEnv( pep , pse ) ;
	if( nret )
		return nret;
	
	/* 创建accept临界区 */
	pse->accept_mutex = semget( IPC_PRIVATE , 1 , IPC_CREAT | 00777 ) ;
	if( pse->accept_mutex == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "create mutex failed\n" );
		return -1;
	}
	
	{
	union semun	semopts ;
	semopts.val = 1 ;
	nret = semctl( pse->accept_mutex , 0 , SETVAL , semopts ) ;
	if( nret == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "set mutex failed\n" );
		return -1;
	}
	}
	
	/* 创建pid跟踪信息数组 */
	pse->pids = (pid_t*)malloc( sizeof(pid_t) * (pep->max_process_count+1) ) ;
	if( pse->pids == NULL )
	{
		InfoLog( __FILE__ , __LINE__ , "alloc failed , errno[%d]\n" , errno );
		return -1;
	}
	memset( pse->pids , 0x00 , sizeof(pid_t) * (pep->max_process_count+1) );
	
	/* 创建存活管道信息数组 */
	pse->alive_pipes = (pipe_t*)malloc( sizeof(pipe_t) * (pep->max_process_count+1) ) ;
	if( pse->alive_pipes == NULL )
	{
		InfoLog( __FILE__ , __LINE__ , "alloc failed , errno[%d]\n" , errno );
		return -1;
	}
	memset( pse->alive_pipes , 0x00 , sizeof(pipe_t) * (pep->max_process_count+1) );
	
	return 0;
}

/* Leader-Follower模型 清理守护环境 */
static int CleanDaemonEnv_LF( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
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
	
	return CleanDaemonEnv( pep , pse );
}

/* Leader-Follow模型 入口函数 */
int tcpdaemon_LF( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	pid_t			pid ;
	int			status ;
	
	struct sigaction	act , oldact ;
	
	int			nret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_LF( pep , pse ) ;
	if( nret )
	{
		InfoLog( __FILE__ , __LINE__ , "init LF failed[%d]\n" , nret );
		CleanDaemonEnv_LF( pep , pse );
		return nret;
	}
	
	/* 转换为守护进程 */
	nret = ConvertToDaemonMode( pep , pse ) ;
	if( nret )
	{
		InfoLog( __FILE__ , __LINE__ , "convert to daemon failed[%d]\n" , nret );
		CleanDaemonEnv_LF( pep , pse );
		return nret;
	}
	
	/* 创建工作进程池 */
	InfoLog( __FILE__ , __LINE__ , "create worker pool starting\n" );
	
	for( pse->index = 1 ; pse->index <= pep->max_process_count ; pse->index++ )
	{
		nret = pipe( pse->alive_pipes[pse->index].fd ) ;
		if( nret == -1 )
		{
			InfoLog( __FILE__ , __LINE__ , "pipe failed , errno[%d]\n" , errno );
			CleanDaemonEnv_LF( pep , pse );
			return -11;
		}
		pse->alive_pipe = & (pse->alive_pipes[pse->index]) ;
		
		pse->pids[pse->index] = fork() ;
		if( pse->pids[pse->index] == -1 )
		{
			InfoLog( __FILE__ , __LINE__ , "fork failed , errno[%d]\n" , errno );
			CleanDaemonEnv_LF( pep , pse );
			return -11;
		}
		else if( pse->pids[pse->index] == 0 )
		{
			signal( SIGTERM , SIG_DFL );
			
			close( pse->alive_pipe->fd[1] );
			worker( pep , pse );
			close( pse->alive_pipe->fd[0] );
			exit(0);
		}
		else
		{
			close( pse->alive_pipes[pse->index].fd[0] );
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "create worker pool ended\n" );
	
	/* 监控工作进程池 */
	memset( & act , 0x00 , sizeof(struct sigaction) );
	act.sa_handler = & sigproc_SIGTERM ;
	act.sa_flags = 0 ;
	sigaction( SIGTERM , & act , & oldact );
	
	signal( SIGCHLD , SIG_DFL );
	
	InfoLog( __FILE__ , __LINE__ , "monitoring all children starting\n" );
	
	while(1)
	{
		/* 监控工作进程结束事件 */
		pid = waitpid( -1 , & status , 0 ) ;
		if( pid == -1 )
		{
			break;
		}
		
		for( pse->index = 1 ; pse->index <= pep->max_process_count ; pse->index++ )
		{
			if( pse->pids[pse->index] == pid )
			{
				/* 重启工作进程 */
				close( pse->alive_pipes[pse->index].fd[1] );
				
				InfoLog( __FILE__ , __LINE__ , "detecting child[%ld]pid[%ld] exit , rebooting\n" , pse->index , (long)pid );
				
				sleep(1);
				
				pse->requests_per_process = 0 ;
				
				nret = pipe( pse->alive_pipes[pse->index].fd ) ;
				if( nret )
				{
					InfoLog( __FILE__ , __LINE__ , "pipe failed , errno[%d]\n" , errno );
					CleanDaemonEnv_LF( pep , pse );
					return -11;
				}
				pse->alive_pipe = & (pse->alive_pipes[pse->index]) ;
				
				pse->pids[pse->index] = fork() ;
				if( pse->pids[pse->index] == -1 )
				{
					InfoLog( __FILE__ , __LINE__ , "fork failed , errno[%d]\n" , errno );
					CleanDaemonEnv_LF( pep , pse );
					return -11;
				}
				else if( pse->pids[pse->index] == 0 )
				{
					signal( SIGTERM , SIG_DFL );
					
					close( pse->alive_pipe->fd[1] );
					worker( pep , pse );
					close( pse->alive_pipe->fd[0] );
					exit(0);
				}
				else
				{
					close( pse->alive_pipes[pse->index].fd[0] );
				}
				
				break;
			}
		}
		if( pse->index > pep->max_process_count )
		{
			InfoLog( __FILE__ , __LINE__ , "detecting unknow child pid[%ld] exit\n" , (long)pid );
			continue;
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "monitoring all children ended\n" );
	
	sigaction( SIGTERM , & oldact , NULL );
	
	/* 销毁进程池 */
	InfoLog( __FILE__ , __LINE__ , "destroy worker poll starting\n" );
	
	for( pse->index = 1 ; pse->index <= pep->max_process_count ; pse->index++ )
	{
		write( pse->alive_pipes[pse->index].fd[1] , "\0" , 1 );
	}
	
	for( pse->index = 1 ; pse->index <= pep->max_process_count ; pse->index++ )
	{
		waitpid( -1 , & status , 0 );
		close( pse->alive_pipes[pse->index].fd[1] );
	}
	
	InfoLog( __FILE__ , __LINE__ , "destroy worker poll ended\n" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_LF( pep , pse );
	
	return 0;
}

/* 主入口函数 */
int tcpdaemon( struct TcpdaemonEntryParam *pep )
{
	struct TcpdaemonServerEnv	se ;
	
	/* 检查入口参数 */
	SetLogLevel( pep->log_level );
	
	if( STRCMP( pep->server_model , == , "IF" ) )
	{
		return -tcpdaemon_IF( pep , & se );
	}
	else if( STRCMP( pep->server_model , == , "LF" ) )
	{
		if( pep->max_process_count <= 0 )
		{
			InfoLog( __FILE__ , __LINE__ , "worker poll size[%ld] invalid\n" , pep->max_process_count );
			return 1;
		}
		
		return -tcpdaemon_LF( pep , & se );
	}
	else
	{
		InfoLog( __FILE__ , __LINE__ , "server mode[%s] invalid\n" , pep->server_model );
		return 1;
	}
}

