#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "IDL_hello_world.dsc.h"

struct NetAddress
{
	char			ip[ 30 + 1 ] ;
	int			port ;
	int			sock ;
	struct sockaddr_in	addr ;
} ;

#define SETNETADDRESS(_netaddr_) \
	memset( & ((_netaddr_).addr) , 0x00 , sizeof(struct sockaddr_in) ); \
	(_netaddr_).addr.sin_family = AF_INET ; \
	if( (_netaddr_).ip[0] == '\0' ) \
		(_netaddr_).addr.sin_addr.s_addr = INADDR_ANY ; \
	else \
		(_netaddr_).addr.sin_addr.s_addr = inet_addr((_netaddr_).ip) ; \
	(_netaddr_).addr.sin_port = htons( (unsigned short)((_netaddr_).port) );

#define COMM_HEAD_LEN		4

static int test_client_latency()
{
	struct timeval		begin_timestamp ;
	struct timeval		end_timestamp ;
	long			diff_us ;
	
	struct NetAddress	netaddr ;
	hello_world		st ;
	int			pack_len ;
	int			unpack_len ;
	char			tmp[ 4 + 1 ] ;
	char			comm_buffer[ 9999 + 1 ] ;
	int			received_len ;
	int			receiving_body_len ;
	int			sent_len ;
	int			sending_len ;
	int			len ;
	
	int			nret = 0 ;
	
	strcpy( netaddr.ip , "0" );
	netaddr.port = 9527 ;
	SETNETADDRESS( netaddr );
	
	while(1)
	{
		/* 得到开始时间戳 */
		gettimeofday( & begin_timestamp , NULL );
		
		/* 组织报文 */
		memset( & st , 0x00 , sizeof(hello_world) );
		strcpy( st.message , "hello world" );
		
		/* 序列化报文 */
		memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
		pack_len = sizeof(comm_buffer) - 4 ;
		nret = DSCSERIALIZE_COMPACT_hello_world( & st , comm_buffer+4 , & pack_len ) ;
		if( nret )
		{
			printf( "DSCSERIALIZE_COMPACT_hello_world failed[%d]\n" , nret );
			return -1;
		}
		
		sprintf( tmp , "%04d" , pack_len );
		memcpy( comm_buffer , tmp , 4 );
		pack_len += 4 ;
		
		/* 连接服务端 */
		netaddr.sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
		if( netaddr.sock == -1 )
		{
			printf( "socket failed , errno[%d]\n" , errno );
			return -1;
		}
		
		{
			int	onoff = 1 ;
			setsockopt( netaddr.sock , IPPROTO_TCP , TCP_NODELAY , (void*) & onoff , sizeof(int) );
		}
		
		nret = connect( netaddr.sock , (struct sockaddr *) & (netaddr.addr) , sizeof(struct sockaddr) ) ;
		if( nret == -1 )
		{
			printf( "connect failed , errno[%d]\n" , errno );
			return -1;
		}
		
		/* 发送通讯请求 */
		sending_len = pack_len ;
		sent_len = 0 ;
		while( sent_len < sending_len )
		{
			len = send( netaddr.sock , comm_buffer + sent_len , sending_len - sent_len , 0 ) ;
			if( len == -1 )
			{
				printf( "send failed , errno[%d]\n" , errno );
				return -1;
			}
			
			sent_len += len ;
		}
		
		/* 接收通讯响应 */
		memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
		received_len = 0 ;
		receiving_body_len = 0 ;
		while(1)
		{
			len = recv( netaddr.sock , comm_buffer + received_len , sizeof(comm_buffer)-1 - received_len , 0 ) ;
			if( len == 0 )
			{
				printf( "close on receiving\n" );
				return -1;
			}
			else if( len == -1 )
			{
				printf( "recv failed , errno[%d]\n" , errno );
				return -1;
			}
			
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
		
		/* 断开服务端 */
		close( netaddr.sock );
		
		/* 反序列化报文 */
		unpack_len = 0 ;
		memset( & st , 0x00 , sizeof(hello_world) );
		nret = DSCDESERIALIZE_COMPACT_hello_world( comm_buffer+4 , & unpack_len , & st ) ;
		if( nret )
		{
			printf( "DSCDESERIALIZE_COMPACT_hello_world failed[%d]\n" , nret );
			return -1;
		}
		
		/* 得到开始时间戳 */
		gettimeofday( & end_timestamp , NULL );
		
		/* 计算时间差 */
		diff_us = (end_timestamp.tv_sec-begin_timestamp.tv_sec)*1000000 + (end_timestamp.tv_usec-begin_timestamp.tv_usec) ;
		
		/* 输出统计信息 */
		printf( "message[%s] latency[%ld]us\n" , st.message , diff_us );
		
		sleep(1);
	}
	
	return 0;
}

int main( int argc , char *argv[] )
{
	setbuf( stdout , NULL );
	
	return -test_client_latency() ;
}

