#ifndef _H_SERVICE_
#define _H_SERVICE_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#if ( defined __linux__ ) || ( defined __unix )

#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
int ConvertToDaemon();

#elif ( defined _WIN32 )

#include <windows.h>
int InstallDaemonServer( char *pcServerName , char *pcRunParameter );
int UninstallDaemonServer( char *pcServerName );
int BindDaemonServer( char *pcServerName , int (* ServerMain)( void *pv ) , void *pv , int (* pfuncControlMain)(long lControlStatus) );

#endif

#ifdef __cplusplus
}
#endif

#endif

