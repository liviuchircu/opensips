# $Id$
#
# sip_router makefile
#
# WARNING: requires gmake (GNU Make)
#

sources= $(wildcard *.c)
objs= $(sources:.c=.o)
depends= $(sources:.c=.d)

NAME=sip_router


CC=gcc
CFLAGS=-O2 -Wcast-align  #-Wmissing-prototypes  -Wall
LEX=lex
YACC=bison
YACC_FLAGS=-d
# on linux and freebsd keep it empty (e.g. LIBS= )
# on solaris add -lxnet (e.g. LIBS= -lxnet)
LIBS=-lfl -L/usr/local/lib
ALLDEP=Makefile

MKDEP=gcc -M


#implicit rules


%.o:%.c $(ALLDEP)
	$(CC) $(CFLAGS) -c $< -o $@

%.d: %.c
	$(MKDEP) $< >$@

# normal rules
$(NAME): $(objs)
	$(CC) $(CFLAGS) $(objs) -o $(NAME) $(LIBS)

lex.yy.c: cfg.lex $(ALLDEP)
	$(LEX) $<

cfg.tab.c: cfg.y
	$(YACC) $(YACC_FLAGS) $<


.PHONY: all
all: $(NAME)

.PHONY: dep
dep: $(depends)

.PHONY: clean
clean:
	-rm $(objs) $(NAME)

.PHONY: proper
proper: clean
	-rm $(depends)

include $(depends)




