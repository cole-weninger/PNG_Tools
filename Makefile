CC = gcc       # compiler
CFLAGS = -Wall -g -std=c99 # compilation flg 
LD = gcc       # linker
LDFLAGS = -g   # debugging symbols in build
LDLIBS = -lz   # link with libz

LIB_UTIL = zutil.o crc.o
SRCS   = findpng.c catpng.c crc.c zutil.c
OBJS   = catpng.o $(LIB_UTIL) 

TARGETS= findpng catpng

all: ${TARGETS}

findpng: findpng.o
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS)

catpng: $(OBJS) 
	$(LD) -o $@ $^ $(LDLIBS) $(LDFLAGS) 

%.o: %.c 
	$(CC) $(CFLAGS) -c $< 

%.d: %.c
	gcc -MM -MF $@ $<

-include $(SRCS:.c=.d)

.PHONY: clean
clean:
	rm -f *.d *.o $(TARGETS)
