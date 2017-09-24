#include "tcpdaemon_in.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

static void usage()
{
#if ( defined __linux__ ) || ( defined __unix )
	printf( "USAGE : tcpdaemon -m IF -l ip:port -s so_pathfilename [ -c max_process_count ]\n" );
	printf( "                  -m LF -l ip:port -s so_pathfilename -c process_count [ -n max_requests_per_process ]\n" );
#elif ( defined _WIN32 )
	printf( "USAGE : tcpdaemon -m WIN-TLF -l ip:port -s so_pathfilename -c process_count [ -n max_requests_per_process ]\n" );
#endif
	
	printf( "        other options :\n" );
	printf( "                  -v\n" );
	printf( "                  [ --logfile log_pathfilename --loglevel-(debug|info|warn|error|fatal) ]\n" );
	printf( "                  [ --daemon-level ]\n" );
	printf( "                  [ --work-path work_path ]\n" );
#if ( defined __linux__ ) || ( defined __unix )
	printf( "                  [ --work-user work_user ]\n" );
	printf( "                  [ --cpu-affinity begin_mask ]\n" );
#elif ( defined _WIN32 )
	printf( "                  [ --tcp-nodelay ] [ --tcp-linger linger ]\n" );
	printf( "                  [ --timeout (seconds) ]\n" );
	printf( "                  [ --install-winservice ] [ --uninstall-winservice ]\n" );
#endif
}

static void version()
{
	printf( "tcpdaemon v%s\n" , __TCPDAEMON_VERSION );
	printf( "Copyright by calvin<calvinwilliams@163.com> 2014~2017\n" );
}

/* 解析命令行参数 */
static int ParseCommandParameter( int argc , char *argv[] , struct TcpdaemonEntryParameter *p_para )
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
			strncpy( p_para->server_model , argv[c+1] , sizeof(p_para->server_model)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "-c" ) && c + 1 < argc )
		{
			p_para->process_count = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-n" ) && c + 1 < argc )
		{
			p_para->max_requests_per_process = atol(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-l" ) && c + 1 < argc )
		{
			char	*p = NULL ;
			p = strchr( argv[c+1] , ':' ) ;
			if( p == NULL )
			{
				INFOLOG( "parameter --listen[%s]无效" , argv[c+1] );
				return 1;
			}
			if( p - argv[c+1] > sizeof(p_para->ip) - 1 )
			{
				INFOLOG( "parameter -l[%s]中的ip过长" , argv[c+1] );
				return 1;
			}
			strncpy( p_para->ip , argv[c+1] , p - argv[c+1] );
			p_para->port = atoi(p+1) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "-s" ) && c + 1 < argc )
		{
			strncpy( p_para->so_pathfilename , argv[c+1] , sizeof(p_para->so_pathfilename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--work-path" ) && c + 1 < argc )
		{
			strncpy( p_para->work_path , argv[c+1] , sizeof(p_para->work_path)-1 );
			c++;
		}
#if ( defined __linux__ ) || ( defined __unix )
		else if( STRCMP( argv[c] , == , "--work-user" ) && c + 1 < argc )
		{
			strncpy( p_para->work_user , argv[c+1] , sizeof(p_para->work_user)-1 );
			c++;
		}
#endif
		else if( STRCMP( argv[c] , == , "--logfile" ) && c + 1 < argc )
		{
			strncpy( p_para->log_pathfilename , argv[c+1] , sizeof(p_para->log_pathfilename)-1 );
			c++;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-fatal" ) )
		{
			p_para->log_level = LOGLEVEL_FATAL ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-error" ) )
		{
			p_para->log_level = LOGLEVEL_ERROR ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-warn" ) )
		{
			p_para->log_level = LOGLEVEL_WARN ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-info" ) )
		{
			p_para->log_level = LOGLEVEL_INFO ;
		}
		else if( STRCMP( argv[c] , == , "--loglevel-debug" ) )
		{
			p_para->log_level = LOGLEVEL_DEBUG ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-nodelay" ) )
		{
			p_para->tcp_nodelay = 1 ;
		}
		else if( STRCMP( argv[c] , == , "--tcp-linger" ) && c + 1 < argc )
		{
			p_para->tcp_linger = atoi(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "--timeout" ) && c + 1 < argc )
		{
			p_para->timeout_seconds = atoi(argv[c+1]) ;
			c++;
		}
		else if( STRCMP( argv[c] , == , "--cpu-affinity" ) && c + 1 < argc )
		{
			p_para->cpu_affinity = atoi(argv[c+1]) ;
			c++;
		}
#if ( defined _WIN32 )
		else if( STRCMP( argv[c] , == , "--install-winservice" ) )
		{
			p_para->install_winservice = 1 ;
		}
		else if( STRCMP( argv[c] , == , "--uninstall-winservice" ) )
		{
			p_para->uninstall_winservice = 1 ;
		}
#endif
		else if( STRCMP( argv[c] , == , "--daemon-level" ) )
		{
			p_para->daemon_level = 1 ;
		}
		else
		{
			INFOLOG( "parameter[%s] invalid" , argv[c] );
			usage();
			exit(1);
		}
	}
	
	return 0;
}

#if ( defined _WIN32 )

static int InstallDaemonServer( char *pcServerName , char *pcRunParameter )
{
	char		acPathFileName[256];
	char		acStartCommand[256];
	SC_HANDLE	schSCManager;
	SC_HANDLE	schService;
	
	memset( acPathFileName , 0x00 , sizeof( acPathFileName ) );
	GetModuleFileName( NULL, acPathFileName, 255 );
	_snprintf( acStartCommand , sizeof(acStartCommand)-1 , "\"%s\" %s" , acPathFileName , pcRunParameter );
	
	schSCManager = OpenSCManager( NULL , NULL , SC_MANAGER_CREATE_SERVICE ) ;
	if( schSCManager == NULL )
	{
		return -1;
	}
	
	schService = CreateService( schSCManager ,
				pcServerName ,
				pcServerName ,
				SERVICE_ALL_ACCESS ,
				SERVICE_WIN32_OWN_PROCESS|SERVICE_INTERACTIVE_PROCESS ,
				SERVICE_AUTO_START ,
				SERVICE_ERROR_NORMAL ,
				acStartCommand ,
				NULL ,
				NULL ,
				NULL ,
				NULL ,
				NULL );
	if( schService == NULL )
	{
		CloseServiceHandle( schSCManager );
		return -2;
	}
	
	CloseServiceHandle( schService );
	CloseServiceHandle( schSCManager );
	
	return 0;
}

static int UninstallDaemonServer( char *pcServerName )
{
	SC_HANDLE	schSCManager;
	SC_HANDLE	schService;
	
	BOOL		bret;
	
	schSCManager = OpenSCManager( NULL , NULL , SC_MANAGER_CREATE_SERVICE ) ;
	if( schSCManager == NULL )
	{
		return -1;
	}
	
	schService = OpenService( schSCManager, pcServerName, SERVICE_ALL_ACCESS) ;
	if( schService == NULL )
	{
		CloseServiceHandle( schSCManager );
		return -2;
	}
	
	bret = DeleteService( schService ) ;
	if( bret == FALSE )
	{
		CloseServiceHandle( schService );
		CloseServiceHandle( schSCManager );
		return -3;
	}
	
	CloseServiceHandle( schService );
	CloseServiceHandle( schSCManager );
	
	return 0;
}

#endif

int main( int argc , char *argv[] )
{
	struct TcpdaemonEntryParameter	para ;
	
	int				nret = 0 ;
	
	if( argc == 1 )
	{
		usage();
		exit(0);
	}
	
#if ( defined _WIN32 )
	if( argc == 1 + 1 && STRCMP( argv[1] , == , "--uninstall-winservice" ) )
	{
		/* 卸载WINDOWS服务 */
		nret = UninstallDaemonServer( "TcpDaemon" ) ;
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
#endif
	
	/* 填充入口参数结构 */
	memset( & para , 0x00 , sizeof(struct TcpdaemonEntryParameter) );
	para.log_level = LOGLEVEL_INFO ;
	
	/* 解析命令行参数 */
	nret = ParseCommandParameter( argc , argv , & para ) ;
	if( nret )
		return nret;
	
#if ( defined _WIN32 )
	if( para.install_winservice )
	{
		long		c , len ;
		char		command_line[ 256 + 1 ] ;
		
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
		
		nret = InstallDaemonServer( "TcpDaemon" , command_line ) ;
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
#endif
	
	return -tcpdaemon( & para );
}
