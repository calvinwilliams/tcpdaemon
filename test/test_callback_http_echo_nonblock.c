#include "tcpdaemon.h"

struct AcceptedSession
{
	int		sock ; /* socket描述字 */
	struct sockaddr	addr ; /* socket地址 */
	
	char		http_buffer[ 4096 + 1 ] ; /* HTTP收发缓冲区 */
	int		read_len ; /* 读了多少字节 */
	int		write_len ; /* 将要写多少字节 */
	int		wrote_len ; /* 写了多少字节 */
} ;

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvironment *p_env , int sock , void *p_addr )
{
	struct AcceptedSession	*p_accepted_session = NULL ;
	int			len ;
	
	switch( TDGetIoMultiplexEvent(p_env) )
	{
		/* 接受新连接事件 */
		case IOMP_ON_ACCEPTING_SOCKET :
			/* 申请内存以存放已连接会话 */
			p_accepted_session = (struct AcceptedSession *)malloc( sizeof(struct AcceptedSession) ) ;
			if( p_accepted_session == NULL )
				return TCPMAIN_RETURN_ERROR;
			memset( p_accepted_session , 0x00 , sizeof(struct AcceptedSession) );
			
			p_accepted_session->sock = sock ;
			memcpy( & (p_accepted_session->addr) , p_addr , sizeof(struct sockaddr) );
			
			/* 设置已连接会话数据结构 */
			TDSetIoMultiplexDataPtr( p_env , p_accepted_session );
			
			/* 等待读事件 */
			return TCPMAIN_RETURN_WAITINGFOR_RECEIVING;
			
		/* 关闭连接事件 */
		case IOMP_ON_CLOSING_SOCKET :
			/* 释放已连接会话 */
			p_accepted_session = (struct AcceptedSession *) p_addr ;
			free( p_accepted_session );
			
			/* 等待下一任意事件 */
			return TCPMAIN_RETURN_WAITINGFOR_NEXT;
			
		/* 通讯接收事件 */
		case IOMP_ON_RECEIVING_SOCKET :
			p_accepted_session = (struct AcceptedSession *) p_addr ;
			
			/* 非堵塞接收通讯数据 */
			len = RECV( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->read_len , sizeof(p_accepted_session->http_buffer)-1-p_accepted_session->read_len , 0 ) ;
			if( len == 0 )
				return TCPMAIN_RETURN_CLOSE;
			else if( len == -1 )
				return TCPMAIN_RETURN_ERROR;
			
			/* 已接收数据长度累加 */
			p_accepted_session->read_len += len ;
			
			/* 如果已收完 */
			if( strstr( p_accepted_session->http_buffer , "\r\n\r\n" ) )
			{
				/* 组织响应报文 */
				p_accepted_session->write_len = sprintf( p_accepted_session->http_buffer , "HTTP/1.0 200 OK\r\n"
											"Content-length: 17\r\n"
											"\r\n"
											"Hello Tcpdaemon\r\n" ) ;
				return TCPMAIN_RETURN_WAITINGFOR_SENDING;
			}
			
			/* 如果缓冲区收满了还没收完 */
			if( p_accepted_session->read_len == sizeof(p_accepted_session->http_buffer)-1 )
				return TCPMAIN_RETURN_ERROR;
			
			/* 等待下一任意事件 */
			return TCPMAIN_RETURN_WAITINGFOR_NEXT;
			
		/* 通讯发送事件 */
		case IOMP_ON_SENDING_SOCKET :
			p_accepted_session = (struct AcceptedSession *) p_addr ;
			
			/* 非堵塞发送通讯数据 */
			len = SEND( p_accepted_session->sock , p_accepted_session->http_buffer+p_accepted_session->wrote_len , p_accepted_session->write_len-p_accepted_session->wrote_len , 0 ) ;
			if( len == -1 )
				return TCPMAIN_RETURN_ERROR;
			
			/* 已发送数据长度累加 */
			p_accepted_session->wrote_len += len ;
			
			/* 如果已发完 */
			if( p_accepted_session->wrote_len == p_accepted_session->write_len )
				return TCPMAIN_RETURN_CLOSE;
			
			/* 等待下一任意事件 */
			return TCPMAIN_RETURN_WAITINGFOR_NEXT;
			
		default :
			return TCPMAIN_RETURN_ERROR;
	}
}

