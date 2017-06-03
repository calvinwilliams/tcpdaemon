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
	
	switch( TDGetIoMultiplexEvent(p_env) )
	{
		case IOMP_ON_ACCEPTING_SOCKET :
			p_accepted_session = (struct AcceptedSession *)malloc( sizeof(struct AcceptedSession) ) ;
			if( p_accepted_session == NULL )
				return -1;
			memset( p_accepted_session , 0x00 , sizeof(struct AcceptedSession) );
			
			p_accepted_session->sock = sock ;
			memcpy( & (p_accepted_session->addr) , p_addr , sizeof(struct AcceptedSession) );
			
			memset( & event , 0x00 , sizeof(struct epoll_event) );
			event.events = EPOLLIN | EPOLLERR ;
			event.data.ptr = p_accepted_session ;
			nret = epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_ADD , p_accepted_session->sock , & event ) ;
			if( nret == -1 )
				return -1;
			
			break;
			
		case IOMP_ON_CLOSING_SOCKET :
			p_accepted_session = (struct AcceptedSession *) p_addr ;
			
			epoll_ctl( TDGetThisEpoll(p_env) , EPOLL_CTL_DEL , p_accepted_session->sock , NULL );
			close( p_accepted_session->sock );
			free( p_accepted_session );
			
			break;
			
		case IOMP_ON_RECEIVING_SOCKET :
			p_accepted_session = (struct AcceptedSession *) p_addr ;
			
			len = RECV( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->read_len , sizeof(p_accepted_session->http_buffer)-1-p_accepted_session->read_len , 0 ) ;
			if( len == 0 )
				return 1;
			else if( len == -1 )
				return 1;
			
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
					return -1;
				
				return 0;
			}
			
			if( p_accepted_session->read_len == sizeof(p_accepted_session->http_buffer)-1 )
				return 1;
			
			break;
			
		case IOMP_ON_SENDING_SOCKET :
			p_accepted_session = (struct AcceptedSession *) p_addr ;
			
			len = SEND( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->wrote_len , p_accepted_session->write_len-p_accepted_session->wrote_len , 0 ) ;
			if( len == -1 )
				return 1;
			
			p_accepted_session->wrote_len += len ;
			
			if( p_accepted_session->wrote_len == p_accepted_session->write_len )
				return 1;
			
			break;
			
		default :
			break;
	}
	
	return 0;
}

