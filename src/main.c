#include "inc.h"

/*
 * tcpdaemon - TCP连接管理守护
 * author      : calvin
 * email       : calvinwilliams.c@gmail.com
 * LastVersion : 2014-06-29	v1.0.0		创建
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

int main( int argc , char *argv[] )
{
	struct TcpdaemonEntryParam	ep ;
	
	memset( & ep , 0x00 , sizeof(struct TcpdaemonEntryParam) );
	ep.call_mode = TCPDAEMON_CALLMODE_DAEMON ;
	
	return _main( argc , argv , & ep , NULL , NULL );
}

