#include "tcpdaemon.h"

#include "IDL_hello_world.dsc.h"

#define DEBUG			0

#define COMM_HEAD_LEN		4

_WINDLL_FUNC int tcpmain( struct TcpdaemonServerEnvironment *p_env , int sock , void *p_addr )
{
	char		comm_buffer[ 9999 + 1 ] ;
	char		tmp [ 4 + 1 ];
	int		received_len ;
	int		receiving_body_len ;
	hello_world	st ;
	int		pack_len ;
	int		unpack_len ;
	int		sent_len ;
	int		sending_len ;
	int		len ;
	
	int		nret = 0 ;
	
	/* 接收通讯请求 */
	memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
	received_len = 0 ;
	receiving_body_len = 0 ;
	while(1)
	{
		len = recv( sock , comm_buffer + received_len , sizeof(comm_buffer)-1 - received_len , 0 ) ;
		if( len == 0 )
			return TCPMAIN_RETURN_CLOSE;
		else if( len == -1 )
			return TCPMAIN_RETURN_ERROR;
		
		received_len += len ;
		
		if( receiving_body_len == 0 && received_len >= COMM_HEAD_LEN )
		{
			receiving_body_len = (comm_buffer[0]-'0')*1000 + (comm_buffer[1]-'0')*100 + (comm_buffer[2]-'0')*10 + (comm_buffer[3]-'0') ;
		}
		
		if( receiving_body_len && COMM_HEAD_LEN + receiving_body_len == received_len )
		{
			break;
		}
	}
	
	/* 反序列化报文 */
	unpack_len = 0 ;
	memset( & st , 0x00 , sizeof(hello_world) );
	nret = DSCDESERIALIZE_COMPACT_hello_world( comm_buffer+4 , & unpack_len , & st ) ;
	if( nret )
		return TCPMAIN_RETURN_ERROR;
	
#if DEBUG
	printf( "message[%s]\n" , st.message );
#endif

	/* 序列化报文 */
	memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
	pack_len = sizeof(comm_buffer)-4 ;
	nret = DSCSERIALIZE_COMPACT_hello_world( & st , comm_buffer+4 , & pack_len ) ;
	if( nret )
		return TCPMAIN_RETURN_ERROR;
	
	sprintf( tmp , "%04d" , pack_len );
	memcpy( comm_buffer , tmp , 4 );
	pack_len += 4 ;
	
	/* 发送通讯响应 */
	sending_len = pack_len ;
	sent_len = 0 ;
	while( sent_len < sending_len )
	{
		len = send( sock , comm_buffer + sent_len , sending_len - sent_len , 0 ) ;
		if( len == -1 )
			return TCPMAIN_RETURN_ERROR;
		
		sent_len += len ;
	}
	
	return TCPMAIN_RETURN_CLOSE;
}

