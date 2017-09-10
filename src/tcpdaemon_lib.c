#if ( defined __linux__ ) || ( defined __unix )
#define _GNU_SOURCE
#include <sched.h>
#endif

#include "tcpdaemon_in.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

/* 版本字符串 */
char		__TCPDAEMON_VERSION_1_3_0[] = "1.3.0" ;
char		*__TCPDAEMON_VERSION = __TCPDAEMON_VERSION_1_3_0 ;

static struct TcpdaemonServerEnvironment	*g_p_env = NULL ;
static int				g_EXIT_flag = 0 ;

#if ( defined _WIN32 )
static CRITICAL_SECTION		accept_critical_section; /* accept临界区 */
#endif

#define MAX_INT(_a_,_b_)	((_a_)>(_b_)?(_a_):(_b_))

/* 绑定CPU亲缘性 */
static int BindCpuAffinity( int processor_no )
{
	cpu_set_t	cpu_mask ;
	
	int		nret = 0 ;
	
	CPU_ZERO( & cpu_mask );
	CPU_SET( processor_no , & cpu_mask );
	nret = sched_setaffinity( 0 , sizeof(cpu_mask) , & cpu_mask ) ;
	DebugLog( __FILE__ , __LINE__ , "sched_setaffinity[%d] return[%d]" , processor_no , nret );
	return nret;
}

/* 检查命令行参数 */
int CheckCommandParameter( struct TcpdaemonEntryParameter *p_para )
{
#if ( defined __linux__ ) || ( defined __unix )
	if( STRCMP( p_para->server_model , == , "IF" ) )
	{
		return 0;
	}
	if( STRCMP( p_para->server_model , == , "LF" ) )
	{
		if( p_para->process_count <= 0 )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker poll size[%ld] invalid" , p_para->process_count );
			return -1;
		}
		
		return 0;
	}
	if( STRCMP( p_para->server_model , == , "IOMP" ) )
	{
		if( p_para->process_count <= 0 )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker poll size[%ld] invalid" , p_para->process_count );
			return -1;
		}
		
		return 0;
	}
#elif ( defined _WIN32 )
	if( STRCMP( p_para->server_model , == , "WIN-TLF" ) )
	{
		if( p_para->process_count <= 0 )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_WIN-TLF_worker poll size[%ld] invalid" , p_para->process_count );
			return -1;
		}
		
		return 0;
	}
#endif
	
	ErrorLog( __FILE__ , __LINE__ , "server mode[%s] invalid" , p_para->server_model );
	return -2;
}

#if ( defined __unix ) || ( defined __linux__ )
static unsigned int tcpdaemon_LF_worker( void *pv )
#elif ( defined _WIN32 )
static unsigned int WINAPI tcpdaemon_LF_worker( void *pv )
#endif
{
	struct TcpdaemonServerEnvironment	*p_env = (struct TcpdaemonServerEnvironment *)pv ;
#if ( defined __linux__ ) || ( defined __unix )
	struct sembuf		sb ;
#endif
	fd_set			readfds ;
	
	struct sockaddr		accepted_addr ;
	SOCKLEN_T		accepted_addrlen ;
	int			accepted_sock ;
	
	int			nret = 0 ;
	
	signal( SIGTERM , SIG_DFL );
	
	if( p_env->p_para->cpu_affinity > 0 )
		BindCpuAffinity( p_env->p_para->cpu_affinity+p_env->index );
	
	/* 设置日志环境 */
	SetLogFile( p_env->p_para->log_pathfilename );
	SetLogLevel( p_env->p_para->log_level );
	
	DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | begin" , p_env->index );
	
	while(1)
	{
		DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | waiting for entering accept mutex" , p_env->index );
		
#if ( defined __linux__ ) || ( defined __unix )
		/* 进入临界区 */
		memset( & sb , 0x00 , sizeof(struct sembuf) );
		sb.sem_num = 0 ;
		sb.sem_op = -1 ;
		sb.sem_flg = SEM_UNDO ;
		nret = semop( p_env->accept_mutex , & sb , 1 ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | enter accept mutex failed , errno[%d]" , p_env->index , ERRNO );
			break;
		}
#elif ( defined _WIN32 )
		EnterCriticalSection( & accept_critical_section );
#endif
		DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | enter accept mutex ok" , p_env->index );
		
		/* 监控侦听socket或存活管道事件 */
		FD_ZERO( & readfds );
		FD_SET( p_env->listen_sock , & readfds );
#if ( defined __linux__ ) || ( defined __unix )
		FD_SET( p_env->alive_pipes[p_env->index].fd[0] , & readfds );
		nret = select( MAX_INT(p_env->listen_sock,p_env->alive_pipes[p_env->index].fd[0])+1 , & readfds , NULL , NULL , NULL ) ;
#elif ( defined _WIN32 )
		nret = select( p_env->listen_sock+1 , & readfds , NULL , NULL , NULL ) ;
#endif
		if( nret == -1 )
		{	
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | select failed , errno[%d]" , p_env->index , ERRNO );
			break;
		}
		
#if ( defined __linux__ ) || ( defined __unix )
		if( FD_ISSET( p_env->alive_pipes[p_env->index].fd[0] , & readfds ) )
		{
			DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | alive_pipe received quit command" , p_env->index );
			break;
		}
#endif
		
		/* 接受新客户端连接 */
		accepted_addrlen = sizeof(struct sockaddr) ;
		memset( & accepted_addr , 0x00 , accepted_addrlen );
		accepted_sock = accept( p_env->listen_sock , & accepted_addr , & accepted_addrlen ) ;
		if( accepted_sock == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | accept failed , errno[%d]" , p_env->index , ERRNO );
			break;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | accept ok , [%d]accept[%d]" , p_env->index , p_env->listen_sock , accepted_sock );
		}
		
		/* 设置侦听端口关闭nagle算法 */
		if( p_env->p_para->tcp_nodelay == 1 )
		{
			int	onoff = 1 ;
			setsockopt( accepted_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
		}
		
		/* 设置侦听端口linger值 */
		if( p_env->p_para->tcp_linger > 0 )
		{
			struct linger	lg ;
			lg.l_onoff = 1 ;
			lg.l_linger = p_env->p_para->tcp_linger ;
			setsockopt( accepted_sock , SOL_SOCKET , SO_LINGER , (void *) & lg , sizeof(struct linger) );
		}
		
#if ( defined __linux__ ) || ( defined __unix )
		/* 离开临界区 */
		memset( & sb , 0x00 , sizeof(struct sembuf) );
		sb.sem_num = 0 ;
		sb.sem_op = 1 ;
		sb.sem_flg = SEM_UNDO ;
		nret = semop( p_env->accept_mutex , & sb , 1 ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | leave accept mutex failed , errno[%d]" , p_env->index , ERRNO );
			break;
		}
#elif ( defined _WIN32 )
		LeaveCriticalSection( & accept_critical_section );
#endif
		DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | leave accept mutex ok" , p_env->index );
		
		/* 调用通讯数据协议及应用处理回调函数 */
		InfoLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | call tcpmain sock[%d]" , p_env->index , accepted_sock );
		nret = p_env->pfunc_tcpmain( p_env , accepted_sock , & accepted_addr ) ;
		if( nret == TCPMAIN_RETURN_CLOSE )
		{
			InfoLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | tcpmain return[%d]" , p_env->index , nret );
		}
		else
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | tcpmain return[%d]" , p_env->index , nret );
			break;
		}
		
		/* 关闭客户端连接 */
		CLOSESOCKET( accepted_sock );
		DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | close[%d]" , p_env->index , accepted_sock );
		
		/* 检查工作进程处理数量 */
		p_env->requests_per_process++;
		if( p_env->p_para->max_requests_per_process != 0 && p_env->requests_per_process >= p_env->p_para->max_requests_per_process )
		{
			InfoLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | maximum number of processing[%ld][%ld] , ending" , p_env->index , p_env->requests_per_process , p_env->p_para->max_requests_per_process );
			return 1;
		}
	}
	
#if ( defined __linux__ ) || ( defined __unix )
	/* 最终离开临界区 */
	memset( & sb , 0x00 , sizeof(struct sembuf) );
	sb.sem_num = 0 ;
	sb.sem_op = 1 ;
	sb.sem_flg = SEM_UNDO ;
	nret = semop( p_env->accept_mutex , & sb , 1 ) ;
	if( nret == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | leave accept mutex finally failed , errno[%d]" , p_env->index , ERRNO );
		return 1;
	}
#elif ( defined _WIN32 )
	LeaveCriticalSection( & accept_critical_section );
#endif
	DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | leave accept mutex finally ok" , p_env->index );
	
	DebugLog( __FILE__ , __LINE__ , "tcpdaemon_LF_worker(%d) | end" , p_env->index );
	
#if ( defined __linux__ ) || ( defined __unix )
	return 0;
#elif ( defined _WIN32 )
	free( p_env );
	_endthreadex(0);
	return 0;
#endif
}

/* 预分配空闲客户端会话结构 */
static int IncreaseTcpdaemonAcceptedSessions( struct TcpdaemonServerEnvironment *p_env )
{
	struct TcpdaemonAcceptedSessionArray	*p_accepted_session_array = NULL ;
	struct TcpdaemonAcceptedSession		*p_accepted_session = NULL ;
	int					i ;
	
	/* 批量增加空闲HTTP通讯会话 */
	p_accepted_session_array = (struct TcpdaemonAcceptedSessionArray *)malloc( sizeof(struct TcpdaemonAcceptedSessionArray) ) ;
	if( p_accepted_session_array == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "malloc failed , errno[%d]" , ERRNO );
		return -1;
	}
	memset( p_accepted_session_array , 0x00 , sizeof(struct TcpdaemonAcceptedSessionArray) );
	list_add_tail( & (p_accepted_session_array->prealloc_node) , & (p_env->accepted_session_array_list.prealloc_node) );
	
	for( i = 0 , p_accepted_session = p_accepted_session_array->accepted_session_array ; i < sizeof(p_accepted_session_array->accepted_session_array)/sizeof(p_accepted_session_array->accepted_session_array[0]) ; i++ , p_accepted_session++ )
	{
		list_add_tail( & (p_accepted_session->unused_node) , & (p_env->accepted_session_unused_list.unused_node) );
		DebugLog( __FILE__ , __LINE__ , "init accepted session[%p]" , p_accepted_session );
	}
	
	return 0;
}

/* 从预分配的空闲客户端会话结构中取出一个 */
static struct TcpdaemonAcceptedSession *FetchTcpdaemonAcceptedSessionUnused( struct TcpdaemonServerEnvironment *p_env )
{
	struct TcpdaemonAcceptedSession	*p_accepted_session = NULL ;
	
	int				nret = 0 ;
	
	/* 如果空闲客户端会话链表为空 */
	if( list_empty( & (p_env->accepted_session_unused_list.unused_node) ) )
	{
		nret = IncreaseTcpdaemonAcceptedSessions( p_env ) ;
		if( nret )
			return NULL;
	}
	
	/* 从空闲HTTP通讯会话链表中移出一个会话，并返回之 */
	p_accepted_session = list_first_entry( & (p_env->accepted_session_unused_list.unused_node) , struct TcpdaemonAcceptedSession , unused_node ) ;
	list_del( & (p_accepted_session->unused_node) );
	
	DebugLog( __FILE__ , __LINE__ , "fetch accepted session[%p]" , p_accepted_session );
	memset( p_accepted_session , 0x00 , sizeof(struct TcpdaemonAcceptedSession) );
	return p_accepted_session;
}

/* 把当前客户端会话放回空闲链表中去 */
static void SetTcpdaemonAcceptedSessionUnused( struct TcpdaemonServerEnvironment *p_env , struct TcpdaemonAcceptedSession *p_accepted_session )
{
	DebugLog( __FILE__ , __LINE__ , "putback accepted session[%p]" , p_accepted_session );
	
	/* 把当前工作HTTP通讯会话移到空闲HTTP通讯会话链表中 */
	list_add_tail( & (p_accepted_session->unused_node) , & (p_env->accepted_session_unused_list.unused_node) );
	
	return;
}

/* 销毁所有客户端物理会话结构 */
static void FreeAllTcpdaemonAcceptedSessionArray( struct TcpdaemonServerEnvironment *p_env )
{
	struct list_head			*p_curr = NULL , *p_next = NULL ;
	struct TcpdaemonAcceptedSessionArray	*p_accepted_session_array = NULL ;
	
	list_for_each_safe( p_curr , p_next , & (p_env->accepted_session_array_list.prealloc_node) )
	{
		p_accepted_session_array = container_of( p_curr , struct TcpdaemonAcceptedSessionArray , prealloc_node ) ;
		list_del( & (p_accepted_session_array->prealloc_node) );
		
		free( p_accepted_session_array );
	}
	
	return;
}

/* 关闭连接 */
static void CloseTcpdaemonAcceptedSession( struct TcpdaemonServerEnvironment *p_env , struct TcpdaemonAcceptedSession *p_session )
{
	InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | close sock[%d] io multiplex data ptr[%p]" , p_env->index , p_session->sock , p_session->io_multiplex_data_ptr );
	p_env->io_multiplex_event = IOMP_ON_CLOSING_SOCKET ;
	p_env->pfunc_tcpmain( p_env , 0 , p_session->io_multiplex_data_ptr );
	epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_DEL , p_session->sock , NULL );
	close( p_session->sock );
	UnlinkTcpdaemonAcceptedSessionBeginTimestampTreeNode( p_env , p_session );
	SetTcpdaemonAcceptedSessionUnused( p_env , p_session );
	
	return;
}

static unsigned int tcpdaemon_IOMP_worker( void *pv )
{
	struct TcpdaemonServerEnvironment	*p_env = (struct TcpdaemonServerEnvironment *)pv ;
	int				quit_flag ;
	int				now_timestamp ;
	struct epoll_event		event ;
	struct epoll_event		events[ MAX_IOMP_EVENTS ] ;
	int				epoll_nfds ;
	int				i ;
	struct epoll_event		*p_event = NULL ;
	struct TcpdaemonAcceptedSession	*p_session = NULL ;
	
	int				nret = 0 ;
	
	signal( SIGTERM , SIG_DFL );
	
	if( p_env->p_para->cpu_affinity > 0 )
		BindCpuAffinity( p_env->p_para->cpu_affinity+p_env->index );
	
	p_env->this_epoll_fd = p_env->epoll_array[p_env->index] ;
	
	/* 设置日志环境 */
	SetLogFile( p_env->p_para->log_pathfilename );
	SetLogLevel( p_env->p_para->log_level );
	
	DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | begin" , p_env->index );
	
	/* 加入侦听可读事件到epoll */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = & (p_env->alive_pipes[p_env->index].fd[0]) ;
	nret = epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_ADD , p_env->alive_pipes[p_env->index].fd[0] , & event ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] add pipe_session[%d] failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , p_env->alive_pipes[p_env->index].fd[0] , errno );
		return 1;
	}
	else
	{
		InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] add pipe_session[%d] ok" , p_env->index , p_env->this_epoll_fd , p_env->alive_pipes[p_env->index].fd[0] );
	}
	
	/* 事件主循环 */
	quit_flag = 0 ;
	while( ! quit_flag )
	{
		/* 等待epoll事件，或者1秒超时 */
		DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_wait[%d] ..." , p_env->index , p_env->this_epoll_fd );
		memset( events , 0x00 , sizeof(events) );
		epoll_nfds = epoll_wait( p_env->this_epoll_fd , events , MAX_IOMP_EVENTS , 1000 ) ;
		if( epoll_nfds == -1 )
		{
			if( errno == EINTR )
			{
				InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_wait[%d] interrupted" , p_env->index , p_env->this_epoll_fd );
				continue;
			}
			else
			{
				ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_wait[%d] failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , ERRNO );
			}
			
			return 1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_wait[%d] return[%d]events" , p_env->index , p_env->this_epoll_fd , epoll_nfds );
		}
		
		if( p_env->p_para->timeout_seconds > 0 )
		{
			now_timestamp = time(NULL) ;
			while(1)
			{
				p_session = GetTimeoutAcceptedSession( p_env , now_timestamp ) ;
				if( p_session == NULL )
					break;
				
				WarnLog( __FILE__ , __LINE__ , "SOCKET TIMEOUT ------ sock[%d] session[%p]" , p_session->sock , p_session );
				/* 调用关闭连接回调函数 */
				CloseTcpdaemonAcceptedSession( p_env , p_session );
			}
		}
		
		/* 处理所有事件 */
		for( i = 0 , p_event = events ; i < epoll_nfds ; i++ , p_event++ )
		{
			/* 侦听套接字事件 */
			if( p_event->data.ptr == & (p_env->listen_sock) )
			{
				/* 可读事件 */
				if( p_event->events & EPOLLIN )
				{
					int			accepted_sock ;
					struct sockaddr		accepted_addr ;
					SOCKLEN_T		accepted_addr_len ;
					
					while(1)
					{
						/* 接受新连接 */
						memset( & accepted_addr , 0x00 , sizeof(struct sockaddr_in) );
						accepted_addr_len = sizeof(struct sockaddr) ;
						accepted_sock = accept( p_env->listen_sock , (struct sockaddr *) & accepted_addr , & accepted_addr_len ) ;
						if( accepted_sock == -1 )
						{
							if( errno == EAGAIN )
								break;
							ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | accept failed , errno[%d]" , p_env->index , errno );
							return 1;
						}
						
						/* 设置非堵塞 */
						{
							fcntl( accepted_sock , F_SETFL , fcntl(accepted_sock,F_GETFL) | O_NONBLOCK );
						}
						
						/* 设置侦听端口关闭nagle算法 */
						if( p_env->p_para->tcp_nodelay == 1 )
						{
							int	onoff = 1 ;
							setsockopt( accepted_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
						}
						
						/* 设置侦听端口linger值 */
						if( p_env->p_para->tcp_linger > 0 )
						{
							struct linger	lg ;
							lg.l_onoff = 1 ;
							lg.l_linger = p_env->p_para->tcp_linger ;
							setsockopt( accepted_sock , SOL_SOCKET , SO_LINGER , (void *) & lg , sizeof(struct linger) );
						}
						
						/* 调用接受通讯连接回调函数 */
						InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnAcceptingSocket[%d]" , p_env->index , accepted_sock );
						p_env->io_multiplex_data_ptr = NULL ;
						p_env->io_multiplex_event = IOMP_ON_ACCEPTING_SOCKET ;
						nret = p_env->pfunc_tcpmain( p_env , accepted_sock , & accepted_addr ) ;
						if( p_env->io_multiplex_data_ptr == NULL )
						{
							FatalLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | need call TDSetIoMultiplexDataPtr" , p_env->index );
							close( accepted_sock );
							continue;
						}
						
						/* 挂接超时会话树和数据指针树 */
						p_session = FetchTcpdaemonAcceptedSessionUnused( p_env ) ;
						if( p_session == NULL )
						{
							FatalLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | FetchTcpdaemonAcceptedSessionUnused failed , errno[%d]" , p_env->index , errno );
							close( accepted_sock );
							continue;
						}
						memset( p_session , 0x00 , sizeof(struct TcpdaemonAcceptedSession) );
						p_session->sock = accepted_sock ;
						p_session->io_multiplex_data_ptr = p_env->io_multiplex_data_ptr ;
						p_session->begin_timestamp = time(NULL) ;
						
						LinkTcpdaemonAcceptedSessionBeginTimestampTreeNode( p_env , p_session );
						
						/* 判断返回值 */
						if( nret == TCPMAIN_RETURN_WAITINGFOR_RECEIVING )
						{
							InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnAcceptingSocket[%d] return[%d]" , p_env->index , accepted_sock , nret );
							
							memset( & event , 0x00 , sizeof(struct epoll_event) );
							event.events = EPOLLIN | EPOLLERR ;
							event.data.ptr = p_session ;
							nret = epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_ADD , accepted_sock , & event ) ;
							if( nret == -1 )
							{
								ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] add accepted sock[%d] waiting for reading failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , accepted_sock , errno );
								return 1;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] add accepted sock[%d] waiting for reading ok" , p_env->index , p_env->this_epoll_fd , accepted_sock );
							}
						}
						else if( nret == TCPMAIN_RETURN_WAITINGFOR_SENDING )
						{
							InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnAcceptingSocket[%d] return[%d]" , p_env->index , accepted_sock , nret );
							
							memset( & event , 0x00 , sizeof(struct epoll_event) );
							event.events = EPOLLOUT | EPOLLERR ;
							event.data.ptr = p_session ;
							nret = epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_ADD , accepted_sock , & event ) ;
							if( nret == -1 )
							{
								ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] add accepted sock[%d] waiting for writing failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , accepted_sock , errno );
								return 1;
							}
							else
							{
								DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] add accepted sock[%d] waiting for writing ok" , p_env->index , p_env->this_epoll_fd , accepted_sock );
							}
						}
						else if( nret == TCPMAIN_RETURN_CLOSE )
						{
							InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnAcceptingSocket[%d] return[%d]" , p_env->index , accepted_sock , nret );
							CloseTcpdaemonAcceptedSession( p_env , p_session );
							continue;
						}
						else
						{
							ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnAcceptingSocket[%d] return[%d]" , p_env->index , accepted_sock , nret );
							CloseTcpdaemonAcceptedSession( p_env , p_session );
							continue;
						}
					}
					
					if( p_env->p_para->process_count > 1 )
					{
						int		 j ;
						
						/* 转移侦听可读事件到下一个epoll */
						j = p_env->index + 1 ;
						if( j >= p_env->p_para->process_count )
							j = 0 ;
						
						memset( & event , 0x00 , sizeof(struct epoll_event) );
						event.events = EPOLLIN | EPOLLERR | EPOLLONESHOT;
						event.data.ptr = & (p_env->listen_sock) ;
						nret = epoll_ctl( p_env->epoll_array[j] , EPOLL_CTL_MOD , p_env->listen_sock , & event ) ;
						if( nret == -1 )
						{
							ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | [%d]epoll_ctl[%d] modify listen sock failed , errno[%d]" , p_env->index , j , p_env->epoll_array[j] , errno );
							return 1;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | [%d]epoll_ctl[%d] modify listen sock[%d] ok" , p_env->index , j , p_env->epoll_array[j] , p_env->listen_sock );
						}
					}
				}
				/* 出错事件 */
				else if( ( p_event->events & EPOLLERR ) || ( p_event->events & EPOLLHUP ) )
				{
					FatalLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | listen session err or hup event[0x%X]" , p_env->index , p_event->events );
					return 1;
				}
				/* 其它事件 */
				else
				{
					FatalLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | Unknow listen session event[0x%X]" , p_env->index , p_event->events );
					return 1;
				}
			}
			/* 命令管道事件 */
			else if( p_event->data.ptr == & (p_env->alive_pipes[p_env->index].fd[0]) )
			{
				quit_flag = 1 ;
			}
			/* 其它事件，即客户端连接会话事件 */
			else
			{
				p_session = p_event->data.ptr ;
				
				/* 可读事件 */
				if( p_event->events & EPOLLIN )
				{
					/* 调用接收通讯数据回调函数 */
					InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnReceivingSocket[%d] ..." , p_env->index , p_session->sock );
					p_env->io_multiplex_event = IOMP_ON_RECEIVING_SOCKET ;
					nret = p_env->pfunc_tcpmain( p_env , 0 , p_session->io_multiplex_data_ptr ) ;
					if( nret == TCPMAIN_RETURN_WAITINGFOR_RECEIVING )
					{
						InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnReceivingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						memset( & event , 0x00 , sizeof(struct epoll_event) );
						event.events = EPOLLIN | EPOLLERR ;
						event.data.ptr = p_session ;
						nret = epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_MOD , p_session->sock , & event ) ;
						if( nret == -1 )
						{
							ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for reading failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , p_session->sock , errno );
							return 1;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for reading ok" , p_env->index , p_env->this_epoll_fd , p_session->sock );
						}
					}
					else if( nret == TCPMAIN_RETURN_WAITINGFOR_SENDING )
					{
						InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnReceivingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						memset( & event , 0x00 , sizeof(struct epoll_event) );
						event.events = EPOLLOUT | EPOLLERR ;
						event.data.ptr = p_session ;
						nret = epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_MOD , p_session->sock , & event ) ;
						if( nret == -1 )
						{
							ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for writing failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , p_session->sock , errno );
							return 1;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for writing ok" , p_env->index , p_env->this_epoll_fd , p_session->sock );
						}
					}
					else if( nret == TCPMAIN_RETURN_CLOSE )
					{
						InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnReceivingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						/* 调用关闭连接回调函数 */
						CloseTcpdaemonAcceptedSession( p_env , p_session );
					}
					else
					{
						ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnReceivingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						/* 调用关闭连接回调函数 */
						CloseTcpdaemonAcceptedSession( p_env , p_session );
					}
				}
				/* 可写事件 */
				else if( p_event->events & EPOLLOUT )
				{
					/* 调用发送通讯数据回调函数 */
					InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnSendingSocket[%d] ..." , p_env->index , p_session->sock );
					p_env->io_multiplex_event = IOMP_ON_SENDING_SOCKET ;
					nret = p_env->pfunc_tcpmain( p_env , 0 , p_session->io_multiplex_data_ptr ) ;
					if( nret == TCPMAIN_RETURN_WAITINGFOR_RECEIVING )
					{
						InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnSendingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						memset( & event , 0x00 , sizeof(struct epoll_event) );
						event.events = EPOLLIN | EPOLLERR ;
						event.data.ptr = p_session ;
						nret = epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_MOD , p_session->sock , & event ) ;
						if( nret == -1 )
						{
							ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for reading failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , p_session->sock , errno );
							return 1;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for reading ok" , p_env->index , p_env->this_epoll_fd , p_session->sock );
						}
					}
					else if( nret == TCPMAIN_RETURN_WAITINGFOR_SENDING )
					{
						InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnSendingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						memset( & event , 0x00 , sizeof(struct epoll_event) );
						event.events = EPOLLOUT | EPOLLERR ;
						event.data.ptr = p_session ;
						nret = epoll_ctl( p_env->this_epoll_fd , EPOLL_CTL_MOD , p_session->sock , & event ) ;
						if( nret == -1 )
						{
							ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for writing failed , errno[%d]" , p_env->index , p_env->this_epoll_fd , p_session->sock , errno );
							return 1;
						}
						else
						{
							DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | epoll_ctl[%d] modify accepted sock[%d] waiting for writing ok" , p_env->index , p_env->this_epoll_fd , p_session->sock );
						}
					}
					else if( nret == TCPMAIN_RETURN_CLOSE )
					{
						InfoLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnSendingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						/* 调用关闭连接回调函数 */
						CloseTcpdaemonAcceptedSession( p_env , p_session );
					}
					else
					{
						ErrorLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | call tcpmain on OnSendingSocket[%d] return[%d]" , p_env->index , p_session->sock , nret );
						
						/* 调用关闭连接回调函数 */
						CloseTcpdaemonAcceptedSession( p_env , p_session );
					}
				}
				/* 出错事件 */
				else if( ( p_event->events & EPOLLERR ) || ( p_event->events & EPOLLHUP ) )
				{
					FatalLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | accepted session err or hup event[0x%X]" , p_env->index , p_event->events );
					/* 调用关闭连接回调函数 */
					CloseTcpdaemonAcceptedSession( p_env , p_event->data.ptr );
				}
				/* 其它事件 */
				else
				{
					FatalLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | Unknow accepted session event[0x%X]" , p_env->index , p_event->events );
					return 1;
				}
			}
		}
	}
	
	DebugLog( __FILE__ , __LINE__ , "tcpdaemon_IOMP_worker(%d) | end" , p_env->index );
	
	return 0;
}

struct TcpdaemonServerEnvironment *DuplicateServerEnv( struct TcpdaemonServerEnvironment *p_env )
{
	struct TcpdaemonServerEnvironment	*pse_new = NULL ;

	pse_new = (struct TcpdaemonServerEnvironment *)malloc( sizeof(struct TcpdaemonServerEnvironment) );
	if( pse_new == NULL )
		return NULL;
	
	memcpy( pse_new , p_env , sizeof(struct TcpdaemonServerEnvironment) );
	
	return pse_new;
}
#if ( defined __linux__ ) || ( defined __unix )

/* TERM信号回调函数 */
void sigproc_SIGTERM( int signo )
{
	g_EXIT_flag = 1 ;
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
			g_p_env->process_count--;
		}
	}
	while( pid > 0 );
	
	return;
}

#endif

/* 初始化守护环境 */
static int InitDaemonEnv( struct TcpdaemonServerEnvironment *p_env )
{
	int		nret = 0 ;
	
	p_env->listen_sock = -1 ;
	
	/* 得到通讯数据协议及应用处理回调函数指针 */
	if( p_env->p_para->so_pathfilename[0] )
	{
#if ( defined __linux__ ) || ( defined __unix )
		p_env->so_handle = dlopen( p_env->p_para->so_pathfilename , RTLD_NOW ) ;
#elif ( defined _WIN32 )
		p_env->so_handle = LoadLibrary( p_env->p_para->so_pathfilename ) ;
#endif
		if( p_env->so_handle == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "dlopen[%s]failed[%s]" , p_env->p_para->so_pathfilename , DLERROR );
			return -1;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "dlopen[%s]ok" , p_env->p_para->so_pathfilename );
		}
		
#if ( defined __linux__ ) || ( defined __unix )
		p_env->pfunc_tcpmain = (func_tcpmain*)dlsym( p_env->so_handle , TCPMAIN ) ;
#elif ( defined _WIN32 )
		p_env->pfunc_tcpmain = (func_tcpmain*)GetProcAddress( p_env->so_handle , TCPMAIN ) ;
#endif
		if( p_env->pfunc_tcpmain == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "dlsym[%s]failed[%s]" , TCPMAIN , DLERROR );
			return -1;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "dlsym[%s]ok" , TCPMAIN );
		}
	}
	else
	{
		p_env->pfunc_tcpmain = p_env->p_para->pfunc_tcpmain ;
	}
	
	p_env->param_tcpmain = p_env->p_para->param_tcpmain ;
	
	/* 创建侦听socket */
	p_env->listen_sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_env->listen_sock == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "socket failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	
	/* 设置侦听端口地址重用 */
	{
		int	on = 1 ;
		setsockopt( p_env->listen_sock , SOL_SOCKET , SO_REUSEADDR , (void *) & on , sizeof(on) );
	}
	
	/* 绑定侦听socket */
	{
	memset( & (p_env->listen_addr) , 0x00 , sizeof(struct sockaddr_in) );
	p_env->listen_addr.sin_family = AF_INET ;
	p_env->listen_addr.sin_addr.s_addr = inet_addr( p_env->p_para->ip ) ;
	p_env->listen_addr.sin_port = htons( (unsigned short)p_env->p_para->port ) ;
	nret = bind( p_env->listen_sock , (struct sockaddr *) & (p_env->listen_addr), sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "bind[%s:%ld]failed , ERRNO[%d]" , p_env->p_para->ip , p_env->p_para->port , ERRNO );
		return -1;
	}
	}
	
	/* 开始侦听 */
	nret = listen( p_env->listen_sock , p_env->p_para->process_count * 2 ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "listen failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	
	/* 切换用户 */
#if ( defined __linux__ ) || ( defined __unix )
	if( STRCMP( p_env->p_para->work_user , != , "" ) )
	{
	struct passwd	*pw ;
	pw = getpwnam( p_env->p_para->work_user ) ;
	if( pw )
	{
		setuid( pw->pw_uid );
		setgid( pw->pw_gid );
	}
	}
#endif
	
	/* 切换工作目录 */
	if( STRCMP( p_env->p_para->work_path , != , "" ) )
	{
		CHDIR( p_env->p_para->work_path );
	}
	
	return 0;
}

/* 清理守护环境 */
static int CleanDaemonEnv( struct TcpdaemonServerEnvironment *p_env )
{
	if( p_env->so_handle )
	{
#if ( defined __linux__ ) || ( defined __unix )
		dlclose( p_env->so_handle );
#elif ( defined _WIN32 )
		FreeLibrary( p_env->so_handle );
#endif
	}
	
	if( p_env->listen_sock != -1 )
	{
		CLOSE( p_env->listen_sock );
	}
	
	return 0;
}

#if ( defined __linux__ ) || ( defined __unix )

/* Instance-Fork进程模型 初始化守护环境 */
static int InitDaemonEnv_IF( struct TcpdaemonServerEnvironment *p_env )
{
	return InitDaemonEnv( p_env );
}

/* Instance-Fork进程模型 清理守护环境 */
static int CleanDaemonEnv_IF( struct TcpdaemonServerEnvironment *p_env )
{
	return CleanDaemonEnv( p_env );
}

/* Instance-Fork进程模型 入口函数 */
int tcpdaemon_IF( struct TcpdaemonServerEnvironment *p_env )
{
	struct sigaction	act , oldact ;
	
	fd_set			readfds ;
	struct timeval		tv ;
	
	struct sockaddr		accepted_addr ;
	socklen_t		accepted_addrlen ;
	int			accepted_sock ;
	
	PID_T			pid ;
	int			status ;
	
	int			nret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_IF( p_env ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "init IF failed[%d]" , nret );
		CleanDaemonEnv_IF( p_env );
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
	
	InfoLog( __FILE__ , __LINE__ , "parent listen starting" );
	
	/* 父进程侦听开始 */
	while(1)
	{
		/* 监控侦听socket事件 */
_DO_SELECT :
		FD_ZERO( & readfds );
		FD_SET( p_env->listen_sock , & readfds );
		tv.tv_sec = 1 ;
		tv.tv_usec = 0 ;
		nret = select( p_env->listen_sock+1 , & readfds , NULL , NULL , & tv ) ;
		if( nret == -1 )
		{
			if( ERRNO == EINTR )
			{
				if( g_EXIT_flag == 1 )
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
				ErrorLog( __FILE__ , __LINE__ , "select failed , ERRNO[%d]" , ERRNO );
				break;
			}
		}
		else if( nret == 0 )
		{
			do
			{
				pid = waitpid( -1 , & status , WNOHANG ) ;
				if( pid > 0 )
				{
					g_p_env->process_count--;
				}
			}
			while( pid > 0 );
			
			continue;
		}
		
		/* 接受新客户端连接 */
_DO_ACCEPT :
		accepted_addrlen = sizeof(struct sockaddr) ;
		memset( & accepted_addr , 0x00 , accepted_addrlen );
		accepted_sock = accept( p_env->listen_sock , & accepted_addr , & accepted_addrlen ) ;
		if( accepted_sock == -1 )
		{
			if( ERRNO == EINTR )
			{
				if( g_EXIT_flag == 1 )
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
				ErrorLog( __FILE__ , __LINE__ , "accept failed , ERRNO[%d]" , ERRNO );
				break;
			}
		}
		else
		{
			
			DebugLog( __FILE__ , __LINE__ , "accept ok , [%d]accept[%d]" , p_env->listen_sock , accepted_sock );
		}
		
		if( p_env->p_para->process_count != 0 && p_env->process_count + 1 > p_env->p_para->process_count )
		{
			ErrorLog( __FILE__ , __LINE__ , "too many sockets" );
			CLOSE( accepted_sock );
			continue;
		}
		
		/* 设置侦听端口关闭nagle算法 */
		if( p_env->p_para->tcp_nodelay == 1 )
		{
			int	onoff = 1 ;
			setsockopt( accepted_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
		}
		
		/* 设置侦听端口linger值 */
		if( p_env->p_para->tcp_linger > 0 )
		{
			struct linger	lg ;
			lg.l_onoff = 1 ;
			lg.l_linger = p_env->p_para->tcp_linger ;
			setsockopt( accepted_sock , SOL_SOCKET , SO_LINGER , (void *) & lg , sizeof(struct linger) );
		}
		
		/* 创建子进程 */
_DO_FORK :
		pid = fork() ;
		if( pid == -1 )
		{
			if( ERRNO == EINTR )
			{
				if( g_EXIT_flag == 1 )
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
				ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]" , ERRNO );
				CLOSE( accepted_sock );
				continue;
			}
		}
		else if( pid == 0 )
		{
			signal( SIGTERM , SIG_DFL );
			signal( SIGCHLD , SIG_DFL );
			
			CLOSESOCKET( p_env->listen_sock );
			
			/* 调用通讯数据协议及应用处理回调函数 */
			InfoLog( __FILE__ , __LINE__ , "call tcpmain sock[%d]" , accepted_sock );
			nret = p_env->pfunc_tcpmain( p_env , accepted_sock , & accepted_addr ) ;
			if( nret < 0 )
			{
				ErrorLog( __FILE__ , __LINE__ , "tcpmain return[%d]" , nret );
			}
			else if( nret > 0 )
			{
				WarnLog( __FILE__ , __LINE__ , "tcpmain return[%d]" , nret );
			}
			else
			{
				InfoLog( __FILE__ , __LINE__ , "tcpmain return[%d]" , nret );
			}
			
			/* 关闭客户端连接 */
			CLOSESOCKET( accepted_sock );
			DebugLog( __FILE__ , __LINE__ , "close[%d]" , accepted_sock );
			
			/* 子进程退出 */
			DebugLog( __FILE__ , __LINE__ , "child exit" );
			exit(0);
		}
		else
		{
			CLOSESOCKET( accepted_sock );
			p_env->process_count++;
		}
	}
	
	sigaction( SIGTERM , & oldact , NULL );
	
	InfoLog( __FILE__ , __LINE__ , "parent listen ended" );
	
	InfoLog( __FILE__ , __LINE__ , "waiting for all children exit starting" );
	
	while( p_env->process_count > 0 )
	{
		waitpid( -1 , & status , 0 );
	}
	
	InfoLog( __FILE__ , __LINE__ , "waiting for all chdilren exit ended" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_IF( p_env );
	
	return 0;
}

/* Leader-Follower进程池模型 初始化守护环境 */
static int InitDaemonEnv_LF( struct TcpdaemonServerEnvironment *p_env )
{
	int		nret = 0 ;
	
	/* 公共初始化环境 */
	nret = InitDaemonEnv( p_env ) ;
	if( nret )
		return nret;
	
	/* 创建pid跟踪信息数组 */
	p_env->pids = (pid_t*)malloc( sizeof(pid_t) * (p_env->p_para->process_count) ) ;
	if( p_env->pids == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->pids , 0x00 , sizeof(pid_t) * (p_env->p_para->process_count) );
	
	/* 创建存活管道信息数组 */
	p_env->alive_pipes = (PIPE_T*)malloc( sizeof(PIPE_T) * (p_env->p_para->process_count) ) ;
	if( p_env->alive_pipes == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->alive_pipes , 0x00 , sizeof(PIPE_T) * (p_env->p_para->process_count) );
	
	/* 创建accept临界区 */
	p_env->accept_mutex = semget( IPC_PRIVATE , 1 , IPC_CREAT | 00777 ) ;
	if( p_env->accept_mutex == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "create mutex failed" );
		return -1;
	}
	
	{
	union semun	semopts ;
	semopts.val = 1 ;
	nret = semctl( p_env->accept_mutex , 0 , SETVAL , semopts ) ;
	if( nret == -1 )
	{
		ErrorLog( __FILE__ , __LINE__ , "set mutex failed" );
		return -1;
	}
	}
	
	return 0;
}

/* Leader-Follower进程池模型 清理守护环境 */
static int CleanDaemonEnv_LF( struct TcpdaemonServerEnvironment *p_env )
{
	/* 释放内存 */
	if( p_env->accept_mutex )
	{
		semctl( p_env->accept_mutex , 0 , IPC_RMID , 0 );
	}
	
	if( p_env->pids )
	{
		free( p_env->pids );
	}
	
	if( p_env->alive_pipes )
	{
		free( p_env->alive_pipes );
	}
	
	return CleanDaemonEnv( p_env );
}

/* Leader-Follow进程池模型 入口函数 */
int tcpdaemon_LF( struct TcpdaemonServerEnvironment *p_env )
{
	PID_T			pid ;
	int			status ;
	
	struct sigaction	act , oldact ;
	
	int			nret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_LF( p_env ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "init LF failed[%d]" , nret );
		CleanDaemonEnv_LF( p_env );
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
	InfoLog( __FILE__ , __LINE__ , "create tcpdaemon_LF_worker pool starting" );
	
	for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
	{
		nret = pipe( p_env->alive_pipes[p_env->index].fd ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "pipe failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_LF( p_env );
			return -11;
		}
		
		p_env->pids[p_env->index] = fork() ;
		if( p_env->pids[p_env->index] == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_LF( p_env );
			return -11;
		}
		else if( p_env->pids[p_env->index] == 0 )
		{
			CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
			tcpdaemon_LF_worker( p_env );
			CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
			
			CLOSESOCKET( p_env->listen_sock );
			
			exit(0);
		}
		else
		{
			CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
		}
		
		SLEEP(1);
	}
	
	InfoLog( __FILE__ , __LINE__ , "create tcpdaemon_LF_worker pool ok" );
	
	/* 监控工作进程池 */
	InfoLog( __FILE__ , __LINE__ , "monitoring all children starting" );
	
	while(1)
	{
		/* 监控工作进程结束事件 */
		pid = waitpid( -1 , & status , 0 ) ;
		if( pid == -1 )
		{
			break;
		}
		
		for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
		{
			if( p_env->pids[p_env->index] == pid )
			{
				/* 重启工作进程 */
				CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
				
				InfoLog( __FILE__ , __LINE__ , "detecting child[%ld]pid[%ld] exit , rebooting" , p_env->index , (long)pid );
				
				SLEEP(1);
				
				p_env->requests_per_process = 0 ;
				
				nret = pipe( p_env->alive_pipes[p_env->index].fd ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "pipe failed , ERRNO[%d]" , ERRNO );
					CleanDaemonEnv_LF( p_env );
					return -11;
				}
				
				p_env->pids[p_env->index] = fork() ;
				if( p_env->pids[p_env->index] == -1 )
				{
					ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]" , ERRNO );
					CleanDaemonEnv_LF( p_env );
					return -11;
				}
				else if( p_env->pids[p_env->index] == 0 )
				{
					CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
					tcpdaemon_LF_worker( p_env );
					CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
					
					CLOSESOCKET( p_env->listen_sock );
					
					exit(0);
				}
				else
				{
					CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
				}
				
				break;
			}
		}
		if( p_env->index > p_env->p_para->process_count )
		{
			InfoLog( __FILE__ , __LINE__ , "detecting unknow child pid[%ld] exit" , (long)pid );
			continue;
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "monitoring all children ok" );
	
	sigaction( SIGTERM , & oldact , NULL );
	
	/* 销毁进程池 */
	InfoLog( __FILE__ , __LINE__ , "destroy tcpdaemon_LF_worker poll starting" );
	
	for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
	{
		write( p_env->alive_pipes[p_env->index].fd[1] , "\0" , 1 );
	}
	
	for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
	{
		waitpid( -1 , & status , 0 );
		CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
	}
	
	InfoLog( __FILE__ , __LINE__ , "destroy tcpdaemon_LF_worker poll ended" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_LF( p_env );
	
	return 0;
}

/* IOMultiplex进程池模型 初始化守护环境 */
static int InitDaemonEnv_IOMP( struct TcpdaemonServerEnvironment *p_env )
{
	struct epoll_event	event ;
	int			i ;
	
	int			nret = 0 ;
	
	/* 创建空闲客户端会话物理结构链表 */
	memset( & (p_env->accepted_session_array_list) , 0x00 , sizeof(struct TcpdaemonAcceptedSessionArray) );
	INIT_LIST_HEAD( & (p_env->accepted_session_array_list.prealloc_node) );
	
	/* 创建空闲客户端会话结构链表 */
	memset( & (p_env->accepted_session_unused_list) , 0x00 , sizeof(struct TcpdaemonAcceptedSession) );
	INIT_LIST_HEAD( & (p_env->accepted_session_unused_list.unused_node) );
	
	/* 公共初始化环境 */
	nret = InitDaemonEnv( p_env ) ;
	if( nret )
		return nret;
	
	/* 设置侦听端口非堵塞 */
	{
		fcntl( p_env->listen_sock , F_SETFL , fcntl(p_env->listen_sock,F_GETFL) | O_NONBLOCK );
	}
	
	/* 设置侦听端口关闭nagle算法 */
	if( p_env->p_para->tcp_nodelay == 1 )
	{
		int	onoff = 1 ;
		setsockopt( p_env->listen_sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
	}
	
	/* 创建pid跟踪信息数组 */
	p_env->pids = (pid_t*)malloc( sizeof(pid_t) * (p_env->p_para->process_count) ) ;
	if( p_env->pids == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->pids , 0x00 , sizeof(pid_t) * (p_env->p_para->process_count) );
	
	/* 创建存活管道信息数组 */
	p_env->alive_pipes = (PIPE_T*)malloc( sizeof(PIPE_T) * (p_env->p_para->process_count) ) ;
	if( p_env->alive_pipes == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->alive_pipes , 0x00 , sizeof(PIPE_T) * (p_env->p_para->process_count) );
	
	/* 创建epoll数组 */
	p_env->epoll_array = (int*)malloc( sizeof(int) * (p_env->p_para->process_count) ) ;
	if( p_env->epoll_array == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->epoll_array , 0x00 , sizeof(int) * (p_env->p_para->process_count) );
	
	for( i = 0 ; i < p_env->p_para->process_count ; i++ )
	{
		p_env->epoll_array[i] = epoll_create( 1024 ) ;
		if( p_env->epoll_array[i] == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_create failed , errno[%d]" , errno );
			return -1;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "epoll_create ok" );
		}
	}
	
	/* 加入侦听可读事件到epoll */
	if( p_env->p_para->process_count > 1 )
	{
		for( i = 0 ; i < p_env->p_para->process_count ; i++ )
		{
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			if( i == 0 )
				event.events = EPOLLIN | EPOLLERR | EPOLLONESHOT ;
			else
				event.events = EPOLLERR ;
			event.data.ptr = & (p_env->listen_sock) ;
			nret = epoll_ctl( p_env->epoll_array[i] , EPOLL_CTL_ADD , p_env->listen_sock , & event ) ;
			if( nret == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "[%d]epoll_ctl[%d] add listen_session failed , errno[%d]" , i , p_env->epoll_array[i] , errno );
				return -1;
			}
			else
			{
				InfoLog( __FILE__ , __LINE__ , "[%d]epoll_ctl[%d] add listen_session[%d] ok" , i , p_env->epoll_array[i] , p_env->listen_sock );
			}
		}
	}
	else
	{
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = & (p_env->listen_sock) ;
		nret = epoll_ctl( p_env->epoll_array[0] , EPOLL_CTL_ADD , p_env->listen_sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "epoll_ctl[%d] add listen_session failed , errno[%d]" , p_env->epoll_array[0] , errno );
			return -1;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "epoll_ctl[%d] add listen_session[%d] ok" , p_env->epoll_array[0] , p_env->listen_sock );
		}
	}
	
	/* 预分配已连接会话 */
	nret = IncreaseTcpdaemonAcceptedSessions( p_env ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "IncreaseTcpdaemonAcceptedSessions failed[%d]" , nret );
		return -1;
	}
	
	return 0;
}

/* IOMultiplex进程池模型 清理守护环境 */
static int CleanDaemonEnv_IOMP( struct TcpdaemonServerEnvironment *p_env )
{
	int		i ;
	
	/* 释放内存 */
	if( p_env->pids )
	{
		free( p_env->pids );
	}
	
	if( p_env->alive_pipes )
	{
		free( p_env->alive_pipes );
	}
	
	if( p_env->epoll_array )
	{
		for( i = 0 ; i < p_env->p_para->process_count ; i++ )
		{
			close( p_env->epoll_array[i] );
		}
		
		free( p_env->epoll_array );
	}
	
	FreeAllTcpdaemonAcceptedSessionArray( p_env );
	
	return CleanDaemonEnv( p_env );
}

/* IOMultiplex进程池模型 入口函数 */
int tcpdaemon_IOMP( struct TcpdaemonServerEnvironment *p_env )
{
	PID_T			pid ;
	int			status ;
	
	struct sigaction	act , oldact ;
	
	int			nret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_IOMP( p_env ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "init IOMP failed[%d]" , nret );
		CleanDaemonEnv_IOMP( p_env );
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
	InfoLog( __FILE__ , __LINE__ , "create tcpdaemon_IOMP_worker pool starting" );
	
	for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
	{
		nret = pipe( p_env->alive_pipes[p_env->index].fd ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "pipe failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_IOMP( p_env );
			return -11;
		}
		
		p_env->pids[p_env->index] = fork() ;
		if( p_env->pids[p_env->index] == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_IOMP( p_env );
			return -11;
		}
		else if( p_env->pids[p_env->index] == 0 )
		{
			CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
			tcpdaemon_IOMP_worker( p_env );
			CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
			
			CLOSESOCKET( p_env->listen_sock );
			
			exit(0);
		}
		else
		{
			CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
		}
		
		SLEEP(1);
	}
	
	InfoLog( __FILE__ , __LINE__ , "create tcpdaemon_IOMP_worker pool ok" );
	
	/* 监控工作进程池 */
	InfoLog( __FILE__ , __LINE__ , "monitoring all children starting" );
	
	while(1)
	{
		/* 监控工作进程结束事件 */
		pid = waitpid( -1 , & status , 0 ) ;
		if( pid == -1 )
		{
			break;
		}
		
		for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
		{
			if( p_env->pids[p_env->index] == pid )
			{
				/* 重启工作进程 */
				CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
				
				InfoLog( __FILE__ , __LINE__ , "detecting child[%ld]pid[%ld] exit , rebooting" , p_env->index , (long)pid );
				
				SLEEP(1);
				
				p_env->requests_per_process = 0 ;
				
				nret = pipe( p_env->alive_pipes[p_env->index].fd ) ;
				if( nret )
				{
					ErrorLog( __FILE__ , __LINE__ , "pipe failed , ERRNO[%d]" , ERRNO );
					CleanDaemonEnv_IOMP( p_env );
					return -11;
				}
				
				p_env->pids[p_env->index] = fork() ;
				if( p_env->pids[p_env->index] == -1 )
				{
					ErrorLog( __FILE__ , __LINE__ , "fork failed , ERRNO[%d]" , ERRNO );
					CleanDaemonEnv_IOMP( p_env );
					return -11;
				}
				else if( p_env->pids[p_env->index] == 0 )
				{
					CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
					tcpdaemon_IOMP_worker( p_env );
					CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
					
					CLOSESOCKET( p_env->listen_sock );
					
					exit(0);
				}
				else
				{
					CLOSE( p_env->alive_pipes[p_env->index].fd[0] );
				}
				
				break;
			}
		}
		if( p_env->index > p_env->p_para->process_count )
		{
			InfoLog( __FILE__ , __LINE__ , "detecting unknow child pid[%ld] exit" , (long)pid );
			continue;
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "monitoring all children ok" );
	
	sigaction( SIGTERM , & oldact , NULL );
	
	/* 销毁进程池 */
	InfoLog( __FILE__ , __LINE__ , "destroy tcpdaemon_IOMP_worker poll starting" );
	
	for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
	{
		write( p_env->alive_pipes[p_env->index].fd[1] , "\0" , 1 );
	}
	
	for( p_env->index = 0 ; p_env->index < p_env->p_para->process_count ; p_env->index++ )
	{
		waitpid( -1 , & status , 0 );
		CLOSE( p_env->alive_pipes[p_env->index].fd[1] );
	}
	
	InfoLog( __FILE__ , __LINE__ , "destroy tcpdaemon_IOMP_worker poll ok" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_IOMP( p_env );
	
	return 0;
}

#endif

#if ( defined _WIN32 )

/* Leader-Follower线程池模型 初始化守护环境 */
static int InitDaemonEnv_WIN_TLF( struct TcpdaemonServerEnvironment *p_env )
{
	int		nret = 0 ;
	
	/* 公共初始化环境 */
	nret = InitDaemonEnv( p_env ) ;
	if( nret )
		return nret;
	
	/* 初始化临界区 */
	InitializeCriticalSection( & accept_critical_section ); 
	
	/* 创建tid跟踪信息数组 */
	p_env->thandles = (THANDLE_T*)malloc( sizeof(THANDLE_T) * (p_env->p_para->process_count+1) ) ;
	if( p_env->thandles == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->thandles , 0x00 , sizeof(THANDLE_T) * (p_env->p_para->process_count+1) );
	
	p_env->tids = (TID_T*)malloc( sizeof(TID_T) * (p_env->p_para->process_count+1) ) ;
	if( p_env->tids == NULL )
	{
		ErrorLog( __FILE__ , __LINE__ , "alloc failed , ERRNO[%d]" , ERRNO );
		return -1;
	}
	memset( p_env->tids , 0x00 , sizeof(TID_T) * (p_env->p_para->process_count+1) );
	
	return 0;
}

/* Leader-Follower线程池模型 清理守护环境 */
static int CleanDaemonEnv_WIN_TLF( struct TcpdaemonServerEnvironment *p_env )
{
	/* 初始化临界区 */
	DeleteCriticalSection( & accept_critical_section );
	
	/* 释放内存 */
	if( p_env->thandles )
	{
		free( p_env->thandles );
	}

	if( p_env->tids )
	{
		free( p_env->tids );
	}
	
	return CleanDaemonEnv( p_env );
}

/* Leader-Follow线程池模型for win32 入口函数 */
int tcpdaemon_WIN_TLF( struct TcpdaemonServerEnvironment *p_env )
{
	struct TcpdaemonServerEnvironment	*pse_new = NULL ;
	unsigned long			index ;
	
	int				nret = 0 ;
	long				lret = 0 ;
	
	/* 初始化守护环境 */
	nret = InitDaemonEnv_WIN_TLF( p_env ) ;
	if( nret )
	{
		ErrorLog( __FILE__ , __LINE__ , "init WIN-TLF failed[%d]" , nret );
		CleanDaemonEnv_WIN_TLF( p_env );
		return nret;
	}
	
	/* 创建工作进程池 */
	InfoLog( __FILE__ , __LINE__ , "create tcpdaemon_LF_worker pool starting" );
	
	for( p_env->index = 1 ; p_env->index <= p_env->p_para->process_count ; p_env->index++ )
	{
		pse_new = DuplicateServerEnv( p_env ) ;
		if( pse_new == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "DuplicateServerEnv failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_WIN_TLF( p_env );
			return -11;
		}
		
		p_env->thandles[p_env->index] = (THANDLE_T)_beginthreadex( NULL , 0 , tcpdaemon_LF_worker , pse_new , 0 , & (p_env->tids[p_env->index]) ) ;
		if( p_env->thandles[p_env->index] == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "_beginthreadex failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_WIN_TLF( p_env );
			return -12;
		}
		
		SLEEP(1);
	}
	
	InfoLog( __FILE__ , __LINE__ , "create tcpdaemon_LF_worker pool ended" );
	
	/* 监控工作线程池 */
	InfoLog( __FILE__ , __LINE__ , "monitoring all children starting" );
	
	while(1)
	{
		/* 监控工作线程结束事件 */
		index = WaitForMultipleObjects( p_env->p_para->process_count , p_env->thandles , FALSE , INFINITE ) ;
		if( index == WAIT_FAILED )
		{
			ErrorLog( __FILE__ , __LINE__ , "WaitForMultipleObjects failed , errno[%d]" , ERRNO );
			break;
		}
		index = index - WAIT_OBJECT_0 + 1 ;
		CloseHandle( p_env->thandles[index-1] );
		
		/* 重启线程进程 */
		InfoLog( __FILE__ , __LINE__ , "detecting child[%ld]tid[%ld] exit , rebooting" , index , p_env->tids[index-1] );
		
		Sleep(1000);
		
		p_env->requests_per_process = 0 ;
		
		pse_new = DuplicateServerEnv( p_env ) ;
		if( pse_new == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "DuplicateServerEnv failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_WIN_TLF( p_env );
			return -11;
		}
		
		p_env->thandles[index-1] = (THANDLE_T)_beginthreadex( NULL , 0 , tcpdaemon_LF_worker , pse_new , 0 , & (p_env->tids[index-1]) ) ;
		if( p_env->thandles[index-1] == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "_beginthreadex failed , ERRNO[%d]" , ERRNO );
			CleanDaemonEnv_WIN_TLF( p_env );
			return -12;
		}
	}
	
	InfoLog( __FILE__ , __LINE__ , "monitoring all children ended" );
	
	/* 销毁线程池 */
	InfoLog( __FILE__ , __LINE__ , "destroy tcpdaemon_LF_worker poll starting" );
	
	for( p_env->index = 1 ; p_env->index <= p_env->p_para->process_count ; p_env->index++ )
	{
		lret = WaitForMultipleObjects( p_env->p_para->process_count , p_env->thandles , TRUE , INFINITE ) ;
		if( lret == WAIT_FAILED )
		{
			break;
		}	
	}
	
	InfoLog( __FILE__ , __LINE__ , "destroy tcpdaemon_LF_worker poll ended" );
	
	/* 清理守护环境 */
	CleanDaemonEnv_WIN_TLF( p_env );
	
	return 0;
}

#endif

#if ( defined __linux__ ) || ( defined __unix )

/* 转换为守护进程 */
static int BindDaemonServer( func_tcpdaemon *tcpdaemon , struct TcpdaemonEntryParameter	*p_para )
{
	pid_t		pid ;
	
	pid = fork() ;
	if( pid == -1 )
	{
		return -1;
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
		return -2;
	}
	else if( pid > 0 )
	{
		exit(0);
	}
	
	setuid( getpid() ) ;
	
	umask( 0 ) ;
	
	return tcpdaemon( p_para );
}

#elif ( defined _WIN32 )

STARTUPINFO		g_si ;
PROCESS_INFORMATION	g_li ;
	
static int (* _iRTF_g_pfControlMain)(long lControlStatus) ;
SERVICE_STATUS		_gssServiceStatus ;
SERVICE_STATUS_HANDLE	_gsshServiceStatusHandle ;
HANDLE			_ghServerHandle ;
void			*_gfServerParameter ;
char			_gacServerName[257] ;
int			(* _gfServerMain)( void *pv ) ;

static void WINAPI ServiceCtrlHandler( DWORD dwControl )
{
	switch ( dwControl )
	{
		case SERVICE_CONTROL_STOP :
		case SERVICE_CONTROL_SHUTDOWN :
			
			_gssServiceStatus.dwCurrentState = SERVICE_STOP_PENDING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus ) ;
			_gssServiceStatus.dwCurrentState = SERVICE_STOPPED ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
			
		case SERVICE_CONTROL_PAUSE :
			
			_gssServiceStatus.dwCurrentState = SERVICE_PAUSE_PENDING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus ) ;
			_gssServiceStatus.dwCurrentState = SERVICE_PAUSED ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
		
		case SERVICE_CONTROL_CONTINUE :
			
			_gssServiceStatus.dwCurrentState = SERVICE_CONTINUE_PENDING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus ) ;
			_gssServiceStatus.dwCurrentState = SERVICE_RUNNING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
			
		case SERVICE_CONTROL_INTERROGATE :
			
			_gssServiceStatus.dwCurrentState = SERVICE_RUNNING ;
			SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus) ;
			break;
			
		default:
			
			break;
			
	}
	
	if( _iRTF_g_pfControlMain )
		(* _iRTF_g_pfControlMain)( dwControl );
	
	return;
}

static void WINAPI ServiceMainProc( DWORD argc , LPTSTR *argv )
{
	_gssServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS ;
	_gssServiceStatus.dwCurrentState = SERVICE_START_PENDING ;
	_gssServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP ;
	_gssServiceStatus.dwWin32ExitCode = 0 ;
	_gssServiceStatus.dwServiceSpecificExitCode = 0 ;
	_gssServiceStatus.dwCheckPoint = 0 ;
	_gssServiceStatus.dwWaitHint = 0 ;
	
	_gsshServiceStatusHandle = RegisterServiceCtrlHandler( _gacServerName , ServiceCtrlHandler ) ;
	if( _gsshServiceStatusHandle == (SERVICE_STATUS_HANDLE)0 )
		return;
	
	_gssServiceStatus.dwCheckPoint = 0 ;
	_gssServiceStatus.dwWaitHint = 0 ;
	_gssServiceStatus.dwCurrentState = SERVICE_RUNNING ;
	
	SetServiceStatus( _gsshServiceStatusHandle , &_gssServiceStatus );
	
	_gfServerMain( _gfServerParameter );
	
	return;
}

static int BindDaemonServer( char *pcServerName , int (* ServerMain)( void *pv ) , void *pv , int (* pfuncControlMain)(long lControlStatus) )
{
	SERVICE_TABLE_ENTRY ste[] =
	{
		{ _gacServerName , ServiceMainProc },
		{ NULL , NULL }
	} ;
	
	BOOL		bret ;
	
	memset( _gacServerName , 0x00 , sizeof( _gacServerName ) );
	if( pcServerName )
	{
		strncpy( _gacServerName , pcServerName , sizeof( _gacServerName ) - 1 );
	}
	
	_ghServerHandle = GetCurrentProcess() ;
	_gfServerMain = ServerMain ;
	_gfServerParameter = pv ;
	_iRTF_g_pfControlMain = pfuncControlMain ;
	
	bret = StartServiceCtrlDispatcher( ste ) ;
	if( bret != TRUE )
		return -1;
	
	return 0;
}

static int monitor( void *command_line )
{
	BOOL			bret = TRUE ;
	
	/* 创建子进程，并监控其结束事件，重启之 */
	while(1)
	{
		memset( & g_si , 0x00 , sizeof(STARTUPINFO) );
		g_si.dwFlags = STARTF_USESHOWWINDOW ;
		g_si.wShowWindow = SW_HIDE ;
		memset( & g_li , 0x00 , sizeof(LPPROCESS_INFORMATION) );
		bret = CreateProcess( NULL , (char*)command_line , NULL , NULL , TRUE , 0 , NULL , NULL , & g_si , & g_li ) ;
		if( bret != TRUE )
		{
			fprintf( stderr , "CreateProcess failed , errno[%d]\n" , GetLastError() );
			return 1;
		}
		
		CloseHandle( g_li.hThread );
		
		WaitForSingleObject( g_li.hProcess , INFINITE );
		CloseHandle( g_li.hProcess );
		
		Sleep(10*1000);
	}
	
	return 0;
}

static int control( long control_status )
{
	/* 当收到WINDOWS服务结束事件，先结束子进程 */
	if( control_status == SERVICE_CONTROL_STOP || control_status == SERVICE_CONTROL_SHUTDOWN )
	{
		TerminateProcess( g_li.hProcess , 0 );
	}
	
	return 0;
}

#endif

/* 主入口函数 */
static int _tcpdaemon( struct TcpdaemonEntryParameter *p_para )
{
	struct TcpdaemonServerEnvironment	env ;
	
	int				nret = 0 ;
	
	memset( & env , 0x00 , sizeof(struct TcpdaemonServerEnvironment) );
	env.p_para = p_para ;
	g_p_env = & env ;
	
	/* 设置日志环境 */
	SetLogFile( p_para->log_pathfilename );
	SetLogLevel( p_para->log_level );
	InfoLog( __FILE__ , __LINE__ , "tcpdaemon startup - v%s" , __TCPDAEMON_VERSION );
	
	/* 检查入口参数 */
	nret = CheckCommandParameter( p_para ) ;
	if( nret )
		return nret;
	
#if ( defined __linux__ ) || ( defined __unix )
	if( STRCMP( p_para->server_model , == , "IF" ) )
	{
		/* 进入Instance-Fork进程模型 入口函数 */
		nret = tcpdaemon_IF( & env ) ;
	}
	if( STRCMP( p_para->server_model , == , "LF" ) )
	{
		/* 进入Leader-Follow进程池模型 入口函数 */
		nret = tcpdaemon_LF( & env ) ;
	}
	if( STRCMP( p_para->server_model , == , "IOMP" ) )
	{
		/* 进入IOMultiplex进程池模型 入口函数 */
		nret = tcpdaemon_IOMP( & env ) ;
	}
#elif ( defined _WIN32 )
	if( STRCMP( p_para->server_model , == , "WIN-TLF" ) )
	{
		/* 进入Leader-Follow线程池模型 入口函数 */
		nret = tcpdaemon_WIN_TLF( & env ) ;
	}
#endif
	
	InfoLog( __FILE__ , __LINE__ , "tcpdaemon ended - v%s" , __TCPDAEMON_VERSION );
	
	return nret;
}

func_tcpdaemon tcpdaemon ;
int tcpdaemon( struct TcpdaemonEntryParameter *p_para )
{
	int		nret = 0 ;
	
	if( p_para->daemon_level == 1 )
	{
#if ( defined __unix ) || ( defined __linux__ )
		/* 转换为守护进程 */
		nret = BindDaemonServer( & _tcpdaemon , p_para ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "convert to daemon failed[%d]" , nret );
			return nret;
		}
		else
		{
			return 0;
		}
#elif ( defined _WIN32 )
		long		c , len ;
		char		command_line[ 256 + 1 ] ;
		
		/* 父进程，转化为服务 */
		memset( command_line , 0x00 , sizeof(command_line) );
		len = SNPRINTF( command_line , sizeof(command_line) , "%s" , "tcpdaemon.exe" ) ;
		for( c = 1 ; c < argc ; c++ )
		{
			if( STRCMP( argv[c] , != , "--install-winservice" ) && STRCMP( argv[c] , != , "--uninstall-winservice" ) && STRCMP( argv[c] , != , "--daemon-level" ) )
			{
				if( strchr( argv[c] , ' ' ) || strchr( argv[c] , '\t' ) )
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " \"%s\"" , argv[c] ) ;
				else
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " %s" , argv[c] ) ;
			}
		}
		
		nret = BindDaemonServer( "TcpDaemon" , monitor , command_line , & control ) ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "convert to daemon failed[%d]" , nret );
			return nret;
		}
		else
		{
			return 0;
		}
#endif
	}
	else
	{
#if ( defined __unix ) || ( defined __linux__ )
		/* 调用tcpdaemon函数 */
		return _tcpdaemon( p_para );
#elif ( defined _WIN32 )
		/* 子进程，干活 */
		{
		WSADATA		wsd;
		if( WSAStartup( MAKEWORD(2,2) , & wsd ) != 0 )
			return 1;
		}
		
		/* 调用tcpdaemon函数 */
		nret = _tcpdaemon( & para ) ;
		WSACleanup();
		retutn nret;
#endif
	}
}

void *TDGetTcpmainParameter( struct TcpdaemonServerEnvironment *p_env )
{
	return p_env->param_tcpmain;
}

int TDGetListenSocket( struct TcpdaemonServerEnvironment *p_env )
{
	return p_env->listen_sock;
}

int *TDGetListenSocketPtr( struct TcpdaemonServerEnvironment *p_env )
{
	return &(p_env->listen_sock);
}

struct sockaddr_in TDGetListenAddress( struct TcpdaemonServerEnvironment *p_env )
{
	return p_env->listen_addr;
}

struct sockaddr_in *TDGetListenAddressPtr( struct TcpdaemonServerEnvironment *p_env )
{
	return &(p_env->listen_addr);
}

int TDGetProcessCount( struct TcpdaemonServerEnvironment *p_env )
{
	return p_env->p_para->process_count;
}

int *TDGetEpollArrayBase( struct TcpdaemonServerEnvironment *p_env )
{
	return p_env->epoll_array;
}

int TDGetThisEpoll( struct TcpdaemonServerEnvironment *p_env )
{
	return p_env->epoll_array[p_env->index];
}

int TDGetIoMultiplexEvent( struct TcpdaemonServerEnvironment *p_env )
{
	return p_env->io_multiplex_event;
}

void TDSetIoMultiplexDataPtr( struct TcpdaemonServerEnvironment *p_env , void *io_multiplex_data_ptr )
{
	p_env->io_multiplex_data_ptr = io_multiplex_data_ptr ;
	return;
}

