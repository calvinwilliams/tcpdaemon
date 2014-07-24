#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#define MAX_INT(_a_,_b_)	((_a_)>(_b_)?(_a_):(_b_))

unsigned int WINAPI worker( void *pv )
{
	struct TcpdaemonServerEnv	*pse = (struct TcpdaemonServerEnv *)pv ;
#if ( defined __linux__ ) || ( defined __unix )
	struct sembuf		sb ;
#endif
	fd_set			readfds ;
	
	struct sockaddr		accept_addr ;
	SOCKLEN_T		accept_addrlen ;
	int			accept_sock ;
	
	int			nret = 0 ;
	
	/* 设置日志环境 */
	SetLogFile( pse->pep->log_pathfilename );
	SetLogLevel( pse->pep->log_level );
	
	while(1)
	{
		DebugLog( __FILE__ , __LINE__ , "WORKER(%ld) | waiting for entering accept mutex\n" , pse->index );
		
#if ( defined __linux__ ) || ( defined __unix )
		/* 进入临界区 */
		memset( & sb , 0x00 , sizeof(struct sembuf) );
		sb.sem_num = 0 ;
		sb.sem_op = -1 ;
		sb.sem_flg = SEM_UNDO ;
		nret = semop( pse->accept_mutex , & sb , 1 ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "WORKER(%ld) | enter accept mutex failed , errno[%d]\n" , pse->index , ERRNO );
			return 1;
		}
#elif ( defined _WIN32 )
		EnterCriticalSection( & accept_critical_section );
#endif
		DebugLog( __FILE__ , __LINE__ , "WORKER(%ld) | enter accept mutex ok\n" , pse->index );
		
		/* 监控侦听socket或存活管道事件 */
		FD_ZERO( & readfds );
		FD_SET( pse->listen_sock , & readfds );
#if ( defined __linux__ ) || ( defined __unix )
		FD_SET( pse->alive_pipes[pse->index-1].fd[0] , & readfds );
		nret = select( MAX_INT(pse->listen_sock,pse->alive_pipes[pse->index-1].fd[0])+1 , & readfds , NULL , NULL , NULL ) ;
#elif ( defined _WIN32 )
		nret = select( pse->listen_sock+1 , & readfds , NULL , NULL , NULL ) ;
#endif
		if( nret == -1 )
		{	
			ErrorLog( __FILE__ , __LINE__ , "WORKER(%ld) | select failed , errno[%d]\n" , pse->index , ERRNO );
			break;
		}
		
#if ( defined __linux__ ) || ( defined __unix )
		if( FD_ISSET( pse->alive_pipes[pse->index-1].fd[0] , & readfds ) )
		{
			DebugLog( __FILE__ , __LINE__ , "WORKER(%ld) | alive_pipe received quit command\n" , pse->index );
			break;
		}
#endif
		
		/* 接受新客户端连接 */
		accept_addrlen = sizeof(struct sockaddr) ;
		memset( & accept_addr , 0x00 , accept_addrlen );
		accept_sock = accept( pse->listen_sock , & accept_addr , & accept_addrlen ) ;
		if( accept_sock == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "WORKER(%ld) | accept failed , errno[%d]\n" , pse->index , ERRNO );
			break;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "WORKER(%ld) | accept ok , [%d]accept[%d]\n" , pse->index , pse->listen_sock , accept_sock );
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
		
#if ( defined __linux__ ) || ( defined __unix )
		/* 离开临界区 */
		memset( & sb , 0x00 , sizeof(struct sembuf) );
		sb.sem_num = 0 ;
		sb.sem_op = 1 ;
		sb.sem_flg = SEM_UNDO ;
		nret = semop( pse->accept_mutex , & sb , 1 ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "WORKER(%ld) | leave accept mutex failed , errno[%d]\n" , pse->index , ERRNO );
			return 1;
		}
#elif ( defined _WIN32 )
		LeaveCriticalSection( & accept_critical_section );
#endif
		DebugLog( __FILE__ , __LINE__ , "WORKER(%ld) | leave accept mutex ok\n" , pse->index );
		
		/* 调用通讯数据协议及应用处理回调函数 */
		InfoLog( __FILE__ , __LINE__ , "WORKER(%ld) | call tcpmain sock[%d]\n" , pse->index , accept_sock );
		nret = pse->pfunc_tcpmain( pse->pep->param_tcpmain , accept_sock , & accept_addr ) ;
		if( nret < 0 )
		{
			ErrorLog( __FILE__ , __LINE__ , "WORKER(%ld) | tcpmain return[%d]\n" , pse->index , nret );
			return 1;
		}
		else if( nret > 0 )
		{
			WarnLog( __FILE__ , __LINE__ , "WORKER(%ld) | tcpmain return[%d]\n" , pse->index , nret );
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER(%ld) | tcpmain return[%d]\n" , pse->index , nret );
		}
		
		/* 关闭客户端连接 */
		CLOSESOCKET( accept_sock );
		DebugLog( __FILE__ , __LINE__ , "WORKER(%ld) | close[%d]\n" , pse->index , accept_sock );
		
		/* 检查工作进程处理数量 */
		pse->requests_per_process++;
		if( pse->pep->max_requests_per_process != 0 && pse->requests_per_process >= pse->pep->max_requests_per_process )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER(%ld) | maximum number of processing[%ld][%ld] , ending\n" , pse->index , pse->requests_per_process , pse->pep->max_requests_per_process );
			return 1;
		}
	}
	
#if ( defined __linux__ ) || ( defined __unix )
	/* 最终离开临界区 */
	memset( & sb , 0x00 , sizeof(struct sembuf) );
	sb.sem_num = 0 ;
	sb.sem_op = 1 ;
	sb.sem_flg = SEM_UNDO ;
	nret = semop( pse->accept_mutex , & sb , 1 ) ;
	if( nret == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "WORKER(%ld) | leave accept mutex finally failed , errno[%d]\n" , pse->index , ERRNO );
		return 1;
	}
#elif ( defined _WIN32 )
	LeaveCriticalSection( & accept_critical_section );
#endif
	DebugLog( __FILE__ , __LINE__ , "WORKER(%ld) | leave accept mutex finally ok\n" , pse->index );
	
#if ( defined __linux__ ) || ( defined __unix )
	return 0;
#elif ( defined _WIN32 )
	free( pse );
	_endthreadex(0);
	return 0;
#endif
}

