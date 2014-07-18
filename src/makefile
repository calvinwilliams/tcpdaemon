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
			tcpdaemon.c \
			worker.c \
			LOGC.c \
			service.c \

###### 目标文件、安装目录配置区
BIN		=	tcpdaemon
BININST		=	$(HOME)/exbin
LIB		=	libtcpdaemon.so
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
tcpdaemon:	main.o tcpdaemon.o worker.o LOGC.o service.o
	$(CC) $(ESOFLAGS) -o $@ main.o tcpdaemon.o worker.o LOGC.o service.o $(LFLAGS)

libtcpdaemon.so: tcpdaemon.o worker.o LOGC.o service.o
	$(CC) $(SOFLAGS) -o $@ tcpdaemon.o worker.o LOGC.o service.o $(LFLAGS)

