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

static int requester( int processing_count )
{
	int			i ;
	
	struct NetAddress	netaddr ;
	int			total_len ;
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
	
	memset( comm_buffer , 0x00 , sizeof(comm_buffer) );
	total_len = sprintf( comm_buffer , "00101234567890" ) ;
	
	for( i = 0 ; i < processing_count ; i++ )
	{
		/* 连接服务端 */
		netaddr.sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
		if( netaddr.sock == -1 )
		{
			printf( "socket failed , errno[%d]\n" , errno );
			return -1;
		}
		
		nret = connect( netaddr.sock , (struct sockaddr *) & (netaddr.addr) , sizeof(struct sockaddr) ) ;
		if( nret == -1 )
		{
			printf( "connect failed , errno[%d]\n" , errno );
			return -1;
		}
		
		/* 发送通讯响应 */
		sending_len = total_len ;
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
		
		/* 接收通讯请求 */
		received_len = 0 ;
		receiving_body_len = 0 ;
		while( receiving_body_len == 0 || ( receiving_body_len > 0 && received_len - COMM_HEAD_LEN < receiving_body_len ) )
		{
			if( receiving_body_len == 0 )
				len = recv( netaddr.sock , comm_buffer + received_len , COMM_HEAD_LEN - received_len , 0 ) ;
			else
				len = recv( netaddr.sock , comm_buffer + received_len , COMM_HEAD_LEN + receiving_body_len - received_len , 0 ) ;
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
				receiving_body_len = (comm_buffer[0]-'0')*1000 + (comm_buffer[1]-'0')*100 + (comm_buffer[2]-'0')*10 + (comm_buffer[3]-'0') ;
		}
		
		/* 断开服务端 */
		close( netaddr.sock );
		
		/* printf( "RECV[%.*s]\n" , received_len , comm_buffer ); */
	}
	
	return 0;
}

static int test_client( int processor_count , int processing_count )
{
	int		i ;
	pid_t		pid ;
	int		status ;
	
	signal( SIGCLD , SIG_DFL );
	signal( SIGCHLD , SIG_DFL );
	
	for( i = 0 ; i < processor_count ; i++ )
	{
		pid = fork() ;
		if( pid == -1 )
		{
			printf( "fork failed , errno[%d]\n" , errno );
			return -1;
		}
		else if( pid == 0 )
		{
			printf( "fork ok , pid[%d]\n" , getpid() );
			exit(-requester(processing_count));
		}
	}
	
	for( i = 0 ; i < processor_count ; i++ )
	{
		pid = waitpid( -1 , & status , 0 ) ;
		if( pid == -1 )
		{
			printf( "waitpid failed , errno[%d]\n" , errno );
			return -1;
		}
	}
	
	return 0;
}

static void usage()
{
	printf( "USAGE : test_client processor_count processing_count\n" );
	return;
}

int main( int argc , char *argv[] )
{
	if( argc != 1 + 2 )
	{
		usage();
		exit(7);
	}
	
	setbuf( stdout , NULL );
	
	return -test_client( atoi(argv[1]) , atoi(argv[2]) );
}

