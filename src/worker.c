#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 * LastVersion : 2014-06-29	v1.0.0		创建
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#define MAX_INT(_a_,_b_)	((_a_)>(_b_)?(_a_):(_b_))

int worker( struct TcpdaemonEntryParam *pep , struct TcpdaemonServerEnv *pse )
{
	struct sembuf		sb ;
	fd_set			readfds ;
	
	struct sockaddr		accept_addr ;
	socklen_t		accept_addrlen ;
	int			accept_sock ;
	
	int			nret = 0 ;
	
	while(1)
	{
		DebugLog( __FILE__ , __LINE__ , "WORKER%ld | waiting for entering accept mutex\n" , pse->index );
		
		/* 进入临界区 */
		memset( & sb , 0x00 , sizeof(struct sembuf) );
		sb.sem_num = 0 ;
		sb.sem_op = -1 ;
		sb.sem_flg = SEM_UNDO ;
		nret = semop( pse->accept_mutex , & sb , 1 ) ;
		if( nret == -1 )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | enter accept mutex failed , errno[%d]\n" , pse->index , errno );
			return -1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "WORKER%ld | enter accept mutex ok\n" , pse->index );
		}
		
		/* 监控侦听socket或存活管道事件 */
		FD_ZERO( & readfds );
		FD_SET( pse->listen_sock , & readfds );
		FD_SET( pse->alive_pipe->fd[0] , & readfds );
		nret = select( MAX_INT(pse->listen_sock,pse->alive_pipe->fd[0])+1 , & readfds , NULL , NULL , NULL ) ;
		if( nret == -1 )
		{	
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | select failed , errno[%d]\n" , pse->index , errno );
			break;
		}
		
		if( FD_ISSET( pse->alive_pipe->fd[0] , & readfds ) )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | alive_pipe received quit command\n" , pse->index );
			break;
		}
		
		/* 接受新客户端连接 */
		accept_addrlen = sizeof(struct sockaddr) ;
		memset( & accept_addr , 0x00 , accept_addrlen );
		accept_sock = accept( pse->listen_sock , & accept_addr , & accept_addrlen ) ;
		if( accept_sock == -1 )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | accept failed , errno[%d]\n" , pse->index , errno );
			break;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "WORKER%ld | accept ok , [%d]accept[%d]\n" , pse->index , pse->listen_sock , accept_sock );
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
		
		/* 离开临界区 */
		memset( & sb , 0x00 , sizeof(struct sembuf) );
		sb.sem_num = 0 ;
		sb.sem_op = 1 ;
		sb.sem_flg = SEM_UNDO ;
		nret = semop( pse->accept_mutex , & sb , 1 ) ;
		if( nret == -1 )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | leave accept mutex failed , errno[%d]\n" , pse->index , errno );
			return -1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "WORKER%ld | leave accept mutex ok\n" , pse->index );
		}
		
		/* 调用通讯数据协议及应用处理回调函数 */
		DebugLog( __FILE__ , __LINE__ , "WORKER%ld | 调用tcpmain\n" , pse->index );
		nret = pse->pfunc_tcpmain( pep->param_tcpmain , accept_sock , & accept_addr ) ;
		if( nret < 0 )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | tcpmain return[%d]\n" , pse->index , nret );
			break;
		}
		else if( nret > 0 )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | tcpmain return[%d]\n" , pse->index , nret );
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "WORKER%ld | tcpmain return[%d]\n" , pse->index , nret );
		}
		
		/* 关闭客户端连接 */
		close( accept_sock );
		DebugLog( __FILE__ , __LINE__ , "close[%d]\n" , accept_sock );
		
		/* 检查工作进程处理数量 */
		pse->requests_per_process++;
		if( pep->max_requests_per_process != 0 && pse->requests_per_process >= pep->max_requests_per_process )
		{
			InfoLog( __FILE__ , __LINE__ , "WORKER%ld | maximum number of processing[%ld][%ld] , ending\n" , pse->index , pse->requests_per_process , pep->max_requests_per_process );
			break;
		}
	}
	
	/* 最终离开临界区 */
	memset( & sb , 0x00 , sizeof(struct sembuf) );
	sb.sem_num = 0 ;
	sb.sem_op = 1 ;
	sb.sem_flg = SEM_UNDO ;
	nret = semop( pse->accept_mutex , & sb , 1 ) ;
	if( nret == -1 )
	{
		InfoLog( __FILE__ , __LINE__ , "WORKER%ld | leave accept mutex finally failed , errno[%d]\n" , pse->index , errno );
		return -1;
	}
	else
	{
		DebugLog( __FILE__ , __LINE__ , "WORKER%ld | leave accept mutex finally ok\n" , pse->index );
	}
	
	return 0;
}

