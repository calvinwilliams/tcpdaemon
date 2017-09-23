#include "tcpdaemon.h"

#include "IDL_hello_world.dsc.h"

#define PREHEAD_LEN		4

struct AcceptedSession
{
	int			sock ; /* socket描述字 */
	struct sockaddr		addr ; /* socket地址 */
	
	char			tdhb_buffer[ 9999 + 1 ] ; /* 收发缓冲区 */
	int			tdhb_len ;
	struct TDHBContext	context ; /* 固定通讯头+通讯体 收发上下文 */
} ;

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvironment *p_env , int sock , void *p_addr )
{
	struct AcceptedSession	*p_accepted_session = NULL ;
	hello_world		st ;
	int			unpack_len ;
	int			nret = 0 ;
	
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
			p_accepted_session->tdhb_len = sizeof(p_accepted_session->tdhb_buffer) ;
			nret = TDHBReceiveDataWithNonblock( p_accepted_session->sock , PREHEAD_LEN , p_accepted_session->tdhb_buffer , & (p_accepted_session->tdhb_len) , & (p_accepted_session->context) ) ;
			if( nret == TDHB_ERROR_SOCKET_CLOSED )
			{
				if( p_accepted_session->tdhb_len == 0 )
					return TCPMAIN_RETURN_CLOSE;
				else
					return TCPMAIN_RETURN_ERROR;
			}
			else if( nret == TCPMAIN_RETURN_WAITINGFOR_NEXT )
				return nret;
			else if( nret == TCPMAIN_RETURN_WAITINGFOR_SENDING )
				;
			else if( nret )
				return nret;
			
			/* 反序列化报文 */
			unpack_len = 0 ;
			memset( & st , 0x00 , sizeof(hello_world) );
			nret = DSCDESERIALIZE_COMPRESS_hello_world( p_accepted_session->tdhb_buffer , & unpack_len , & st ) ;
			if( nret )
				return TCPMAIN_RETURN_ERROR;
			
			/* 序列化报文 */
			memset( p_accepted_session->tdhb_buffer , 0x00 , sizeof(p_accepted_session->tdhb_buffer) );
			p_accepted_session->tdhb_len = sizeof(p_accepted_session->tdhb_buffer) ;
			nret = DSCSERIALIZE_COMPRESS_hello_world( & st , p_accepted_session->tdhb_buffer , & (p_accepted_session->tdhb_len) ) ;
			if( nret )
				return TCPMAIN_RETURN_ERROR;
			
			return TCPMAIN_RETURN_WAITINGFOR_SENDING;
			
		/* 通讯发送事件 */
		case IOMP_ON_SENDING_SOCKET :
			p_accepted_session = (struct AcceptedSession *) p_addr ;
			
			/* 非堵塞发送通讯数据 */
			nret = TDHBSendDataWithNonblock( p_accepted_session->sock , PREHEAD_LEN , p_accepted_session->tdhb_buffer , & (p_accepted_session->tdhb_len) , & (p_accepted_session->context) ) ;
			if( nret == TCPMAIN_RETURN_WAITINGFOR_NEXT )
				return nret;
			else if( nret == TCPMAIN_RETURN_WAITINGFOR_RECEIVING )
				/*
				 * 处理空闲操作
				 */
				/* ... */
				return nret;
			else
				return nret;
			
		default :
			return TCPMAIN_RETURN_ERROR;
	}
}

