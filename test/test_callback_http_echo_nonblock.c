#include "tcpdaemon.h"

struct AcceptedSession
{
	int		sock ;
	struct sockaddr	addr ;
	char		http_buffer[ 4096 + 1 ] ;
	int		read_len ;
	int		write_len ;
	int		wrote_len ;
} ;

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvirment *p_env , int sock , void *p_addr )
{
	struct AcceptedSession	*p_accepted_session = NULL ;
	struct epoll_event	event ;
	int			len ;
	
	int			nret = 0 ;
	
	if( TDIsOnAcceptingSocket( p_env ) )
	{
		p_accepted_session = (struct AcceptedSession *)malloc( sizeof(struct AcceptedSession) ) ;
		if( p_accepted_session == NULL )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpmain | alloc failed , errno[%d]" , errno );
			return -1;
		}
		memset( p_accepted_session , 0x00 , sizeof(struct AcceptedSession) );
		
		p_accepted_session->sock = sock ;
		memcpy( & (p_accepted_session->addr) , p_addr , sizeof(struct AcceptedSession) );
		
		memset( & event , 0x00 , sizeof(struct epoll_event) );
		event.events = EPOLLIN | EPOLLERR ;
		event.data.ptr = p_accepted_session ;
		nret = epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_ADD , p_accepted_session->sock , & event ) ;
		if( nret == -1 )
		{
			ErrorLog( __FILE__ , __LINE__ , "tcpmain | epoll_ctl[%d] add accepted sock[%d] failed , errno[%d]" , TDGetThisEpoll(p_env) , sock , errno );
			return -1;
		}
		else
		{
			DebugLog( __FILE__ , __LINE__ , "tcpmain | epoll_ctl[%d] add accepted sock[%d] ok" , TDGetThisEpoll(p_env) , sock );
		}
	}
	else if( sock == 0 )
	{
		p_accepted_session = (struct AcceptedSession *) p_addr ;
		
		epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_DEL , p_accepted_session->sock , NULL );
		close( p_accepted_session->sock );
		free( p_accepted_session );
	}
	else
	{
		p_accepted_session = (struct AcceptedSession *) p_addr ;
		
		if( sock & EPOLLIN )
		{
			len = RECV( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->read_len , sizeof(p_accepted_session->http_buffer)-1-p_accepted_session->read_len , 0 ) ;
			if( len == 0 )
			{
				ErrorLog( __FILE__ , __LINE__ , "tcpmain | read accepted sock[%d] failed , errno[%d]" , p_accepted_session->sock , errno );
				return 1;
			}
			else if( len == -1 )
			{
				InfoLog( __FILE__ , __LINE__ , "tcpmain | accepted sock[%d] closed on remote , errno[%d]" , p_accepted_session->sock , errno );
				return 1;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "tcpmain | read accepted sock[%d] ok , [%d]bytes" , p_accepted_session->sock , len );
			}
			
			p_accepted_session->read_len += len ;
			
			if( strstr( p_accepted_session->http_buffer , "\r\n\r\n" ) )
			{
				p_accepted_session->write_len = sprintf( p_accepted_session->http_buffer , "HTTP/1.0 200 OK\r\n"
											"Content-length: 17\r\n"
											"\r\n"
											"Hello Tcpdaemon\r\n" ) ;
				
				memset( & event , 0x00 , sizeof(struct epoll_event) );
				event.events = EPOLLOUT | EPOLLERR ;
				event.data.ptr = p_accepted_session ;
				nret = epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_MOD , p_accepted_session->sock , & event ) ;
				if( nret == -1 )
				{
					ErrorLog( __FILE__ , __LINE__ , "tcpmain | epoll_ctl[%d] mod accepted sock[%d] failed , errno[%d]" , TDGetThisEpoll(p_env) , sock , errno );
					return -1;
				}
				else
				{
					DebugLog( __FILE__ , __LINE__ , "tcpmain | epoll_ctl[%d] mod accepted sock[%d] ok" , TDGetThisEpoll(p_env) , sock );
				}
				
				return 0;
			}
			
			if( p_accepted_session->read_len == sizeof(p_accepted_session->http_buffer)-1 )
			{
				WarnLog( __FILE__ , __LINE__ , "tcpmain | too many data accepted sock[%d]" , p_accepted_session->sock );
				return 1;
			}
		}
		else if( sock & EPOLLOUT )
		{
			len = SEND( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->wrote_len , p_accepted_session->write_len-p_accepted_session->wrote_len , 0 ) ;
			if( len == -1 )
			{
				ErrorLog( __FILE__ , __LINE__ , "tcpmain | write accepted sock[%d] failed , errno[%d]" , p_accepted_session->sock , errno );
				return 1;
			}
			else
			{
				DebugLog( __FILE__ , __LINE__ , "tcpmain | write accepted sock[%d] ok , [%d]bytes" , p_accepted_session->sock , len );
			}
			
			p_accepted_session->wrote_len += len ;
			
			if( p_accepted_session->wrote_len == p_accepted_session->write_len )
				return 1;
		}
	}
	
	return 0;
}

