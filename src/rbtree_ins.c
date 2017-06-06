/*
 * rbtree_tpl
 * author	: calvin
 * email	: calvinwilliams@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "tcpdaemon_in.h"

#include "rbtree_tpl.h"

LINK_RBTREENODE_PTR( LinkTcpdaemonAcceptedSessionDataPtrTreeNode , struct TcpdaemonServerEnvirment , session_io_multiplex_data_ptr_rbtree , struct TcpdaemonAcceptedSession , io_multiplex_data_ptr_rbnode , io_multiplex_data_ptr )
QUERY_RBTREENODE_PTR( QueryTcpdaemonAcceptedSessionDataPtrTreeNode , struct TcpdaemonServerEnvirment , session_io_multiplex_data_ptr_rbtree , struct TcpdaemonAcceptedSession , io_multiplex_data_ptr_rbnode , io_multiplex_data_ptr )
UNLINK_RBTREENODE( UnlinkTcpdaemonAcceptedSessionDataPtrTreeNode , struct TcpdaemonServerEnvirment , session_io_multiplex_data_ptr_rbtree , struct TcpdaemonAcceptedSession , io_multiplex_data_ptr_rbnode )
LINK_RBTREENODE_INT_ALLOWDUPLICATE( LinkTcpdaemonAcceptedSessionBeginTimestampTreeNode , struct TcpdaemonServerEnvirment , session_begin_timestamp_rbtree , struct TcpdaemonAcceptedSession , begin_timestamp_rbnode , begin_timestamp )
struct TcpdaemonAcceptedSession *GetTimeoutAcceptedSession( struct TcpdaemonServerEnvirment *p_env , int now_timestamp )
{
	struct rb_node			*p_curr = NULL ;
	struct TcpdaemonAcceptedSession	*p_session = NULL ;
	
	p_curr = rb_first( & (p_env->session_begin_timestamp_rbtree) ); 
	if( p_curr == NULL )
		return NULL;
	
	p_session = rb_entry( p_curr , struct TcpdaemonAcceptedSession , begin_timestamp_rbnode ) ;
	if( now_timestamp >= p_session->begin_timestamp + p_env->p_para->timeout_seconds )
		return p_session;
	
	return NULL;
}
UNLINK_RBTREENODE( UnlinkTcpdaemonAcceptedSessionBeginTimestampTreeNode , struct TcpdaemonServerEnvirment , session_begin_timestamp_rbtree , struct TcpdaemonAcceptedSession , begin_timestamp_rbnode )
DESTROY_RBTREE( DestroyTcpdaemonAcceptedSessionTree , struct TcpdaemonServerEnvirment , session_io_multiplex_data_ptr_rbtree , struct TcpdaemonAcceptedSession , io_multiplex_data_ptr_rbnode , NULL )

