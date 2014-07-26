#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

void usage()
{
	printf( "USAGE : tcpdaemon -m LF -c process_count [ -n max_requests_per_process ] -l ip:port -s so_pathfilename\n" );
	printf( "                  -m IF [ -c max_process_count ] -l ip:port -s so_pathfilename\n" );
	printf( "        other options :\n" );
	printf( "                  -v\n" );
	printf( "                  [ --daemon-level ]\n" );
#if ( defined __linux__ ) || ( defined __unix )
	printf( "                  [ --work-user work_user ]\n" );
#endif
	printf( "                  [ --work-path work_path ]\n" );
	printf( "                  [ --logfile log_pathfilename --loglevel-(debug|info|warn|error|fatal) ]\n" );
	printf( "                  [ --tcp-nodelay ] [ --tcp-linger linger ]\n" );
#if ( defined _WIN32 )
	printf( "                  [ --install-winservice ] [ --uninstall-winservice ]\n" );
#endif
}

void version()
{
	printf( "tcpdaemon v%s\n" , __version_tcpdaemon );
	printf( "Copyright by calvin<calvinwilliams.c@gmail.com> 2014\n" );
}

/* 解析命令行参数 */
int ParseCommandParameter( int argc , char *argv[] , struct TcpdaemonEntryParam *pep )
{
	int		c ;
	
	/* 解析命令行参数 */
	for( c = 1 ; c < argc ; c++ )
	{
		if( STRCMP( argv[c] , == , "-v" ) )
		{
			version();
			exit(0);
		}
		else if( STRCMP( argv[c] , == , "-m" ) && c + 1 < argc )
		{
			strncpy( pep->server_model , argv[c+1] , sizeof(pep->server_model)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "-c" ) && c + 1 < argc )
		{
			pep->max_process_count = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-n" ) && c + 1 < argc )
		{
			pep->max_requests_per_process = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-l" ) && c + 1 < argc )
		{
			char	*p = NULL ;
			p = strchr( argv[c+1] , ':' ) ;
			if( p == NULL )
			{
				InfoLog( __FILE__ , __LINE__ , "parameter --listen[%s]无效" , argv[c+1] );
				return 1;
			}
			if( p - argv[c+1] > sizeof(pep->ip) - 1 )
			{
				InfoLog( __FILE__ , __LINE__ , "parameter -l[%s]中的ip过长" , argv[c+1] );
				return 1;
			}
			strncpy( pep->ip , argv[c+1] , p - argv[c+1] );
			pep->port = atol(p+1) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-s" ) && c + 1 < argc )
		{
			strncpy( pep->so_pathfilename , argv[c+1] , sizeof(pep->so_pathfilename)-1 );
			c++;
		}
#if ( defined __linux__ ) || ( defined __unix )
		else if( STRCMP( argv[c] , == , "--work-user" ) && c + 1 < argc )
		{
			strncpy( pep->work_user , argv[c+1] , sizeof(pep->work_user)-1 );
			c++;
		}
#endif
		else if( STRCMP( argv[c] , == , "--work-path" ) && c + 1 < argc )
		{
			strncpy( pep->work_path , argv[c+1] , sizeof(pep->work_path)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--logfile" ) && c + 1 < argc )
		{
			strncpy( pep->log_pathfilename , argv[c+1] , sizeof(pep->log_pathfilename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-fatal" ) )
		{
			pep->log_level = LOGLEVEL_FATAL ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-error" ) )
		{
			pep->log_level = LOGLEVEL_ERROR ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-warn" ) )
		{
			pep->log_level = LOGLEVEL_WARN ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-info" ) )
		{
			pep->log_level = LOGLEVEL_INFO ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-debug" ) )
		{
			pep->log_level = LOGLEVEL_DEBUG ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-nodelay" ) )
		{
			pep->tcp_nodelay = 1 ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-linger" ) && c + 1 < argc )
		{
			pep->tcp_linger = atoi(argv[c+1]) ;
			c++;
		}
#if ( defined _WIN32 )
		else if( STRCMP( argv[c] , == , "--install-winservice" ) )
		{
			pep->install_winservice = 1 ;
		}
		else if( STRCMP( argv[c] , == , "--uninstall-winservice" ) )
		{
			pep->uninstall_winservice = 1 ;
		}
#endif
		else if( STRCMP( argv[c] , == , "--daemon-level" ) )
		{
			pep->daemon_level = 1 ;
		}
		else
		{
			InfoLog( __FILE__ , __LINE__ , "parameter[%s] invalid" , argv[c] );
			usage();
			exit(1);
		}
	}
	
	return 0;
}

#if ( defined __linux__ ) || ( defined __unix )

int main( int argc , char *argv[] )
{
	struct TcpdaemonEntryParam	ep ;
	
	int				nret = 0 ;
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
	/* 填充入口参数结构 */
	memset( & ep , 0x00 , sizeof(struct TcpdaemonEntryParam) );
	ep.call_mode = TCPDAEMON_CALLMODE_DAEMON ;
	ep.max_process_count = 0 ;
	ep.max_requests_per_process = 0 ;
	ep.log_level = LOGLEVEL_INFO ;
	
	/* 解析命令行参数 */
	nret = ParseCommandParameter( argc , argv , & ep ) ;
	if( nret )
	{
		return nret;
	}
	
	/* 检查命令行参数 */
	nret = CheckCommandParameter( & ep ) ;
	if( nret )
	{
		return nret;
	}
	
	if( ep.daemon_level == 1 )
	{
		/* 转换为守护进程 */
		nret = ConvertToDaemon() ;
		if( nret )
		{
			ErrorLog( __FILE__ , __LINE__ , "convert to daemon failed[%d]" , nret );
			return nret;
		}
	}
	
	/* 调用tcpdaemon函数 */
	return -tcpdaemon( & ep );
}

#elif ( defined _WIN32 )

STARTUPINFO		g_si ;
PROCESS_INFORMATION	g_li ;
	
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

int main( int argc , char *argv[] )
{
	struct TcpdaemonEntryParam	ep ;
	
	char				command_line[ 256 + 1 ] ;
	long				c , len ;
	
	int				nret = 0 ;
	
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
	if( argc == 1 + 1 && STRCMP( argv[1] , == , "--uninstall-winservice" ) )
	{
		/* 卸载WINDOWS服务 */
		nret = UninstallDaemonServer( "Tcp Daemon" ) ;
		if( nret )
		{
			printf( "Uninstall Windows Service Failure , error code[%d]\n" , nret );
		}
		else
		{
			printf( "Uninstall Windows Service Successful\n" );
		}
		
		return 0;
	}
	
	/* 填充入口参数结构 */
	memset( & ep , 0x00 , sizeof(struct TcpdaemonEntryParam) );
	ep.call_mode = TCPDAEMON_CALLMODE_DAEMON ;
	ep.max_process_count = 0 ;
	ep.max_requests_per_process = 0 ;
	ep.log_level = LOGLEVEL_INFO ;
	
	/* 解析命令行参数 */
	nret = ParseCommandParameter( argc , argv , & ep ) ;
	if( nret )
	{
		return nret;
	}
	
	/* 检查命令行参数 */
	nret = CheckCommandParameter( & ep ) ;
	if( nret )
	{
		return nret;
	}
	
	if( ep.install_winservice )
	{
		/* 安装WINDOWS服务 */
		memset( command_line , 0x00 , sizeof(command_line) );
		len = 0 ;
		for( c = 1 ; c < argc ; c++ )
		{
			if( STRCMP( argv[c] , != , "--install-winservice" ) && STRCMP( argv[c] , != , "--uninstall-winservice" ) )
			{
				if( strchr( argv[c] , ' ' ) || strchr( argv[c] , '\t' ) )
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " \"%s\"" , argv[c] ) ;
				else
					len += SNPRINTF( command_line + len , sizeof(command_line)-1 - len , " %s" , argv[c] ) ;
			}
		}
		
		nret = InstallDaemonServer( "Tcp Daemon" , command_line ) ;
		if( nret )
		{
			printf( "Install Windows Service Failure , error code[%d]\n" , nret );
		}
		else
		{
			printf( "Install Windows Service Successful\n" );
		}
		
		return 0;
	}
	
	if( ep.daemon_level == 1 )
	{
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
		
		nret = BindDaemonServer( "Tcp Daemon" , monitor , command_line , & control ) ;
	}
	else
	{
		/* 子进程，干活 */
		{
		WSADATA		wsd;
		if( WSAStartup( MAKEWORD(2,2) , & wsd ) != 0 )
			return 1;
		}
		
		/* 调用tcpdaemon函数 */
		nret = tcpdaemon( & ep ) ;
		
		WSACleanup();
	}
	
	return -nret;
}

#endif
