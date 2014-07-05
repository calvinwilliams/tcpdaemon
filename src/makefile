# 此文件由makeobj.sh自动生成
############################################################
# 项目名 : 
# 模块名 : 
# 备  注 : 
############################################################

###### 源文件配置区
#@ c_FILE
c_FILE		=	\
			main.c \
			nomain.c \
			_main.c \
			tcpdaemon.c \
			util.c \
			worker.c \

###### 目标文件、安装目录配置区
BIN		=	tcpdaemon
BININST		=	$(HOME)/exbin
LIB		=	libtcpdaemon.so libtcpdaemonmain.so
LIBINST		=	$(HOME)/exlib
HDER		=	tcpdaemon.h
HDERINST	=	$(HOME)/exinc/tcpdaemon

###### 编译选项
CFLAGS		=	-I. \

###### 链接选项
LFLAGS		=	-L. \
			-lpthread \
			-ldl \

###### 额外宏定义区
CLEAN_ADDITION	=

###### 加载mktpl模板库
#@ make_all
#@ make_clean
#@ make_install
#@ make_install_BININST
#@ make_install_HDERINST
#@ make_install_DFTHDERINST

include $(MKTPLDIR)/makeobj_$(MKTPLOS).inc

###### 目标文件依赖关系
tcpdaemon:	util.o main.o _main.o tcpdaemon.o worker.o
	$(CC) $(ESOFLAGS) -o $@ util.o main.o _main.o tcpdaemon.o worker.o $(LFLAGS)

libtcpdaemon.so:util.o tcpdaemon.o worker.o
	$(CC) $(SOFLAGS) -o $@ util.o tcpdaemon.o worker.o $(LFLAGS)

libtcpdaemonmain.so:util.o nomain.o _main.o tcpdaemon.o worker.o
	$(CC) $(SOFLAGS) -o $@ util.o nomain.o _main.o tcpdaemon.o worker.o $(LFLAGS)

