CFLAGS = -O2 -D__STDC_CONSTANT_MACROS -g
#LDFLAGS = -lopencv_imgproc -lopencv_core -lpthread -lz -lc -lboost_python -lpython2.7
LDFLAGS = -lopencv_imgproc -lopencv_core -lavdevice -lavformat -lavfilter -lavcodec -lswresample  -lswscale -lavutil -lpthread -lx264 -lz -lc -lboost_python -lpython2.7 -lm -ldl -llzma -lstdc++
INCLUDE_DIRS = -I../bin/dependencies/include -I/usr/include/python2.7 
LIB_DIRS = -L../bin/dependencies/lib -L/usr/lib/x86_64-linux-gnu/
BIN = mpegflow
# cc -I/home/gabriel/cvpr2014/bin/dependencies/ffmpeg/../include -Wall -g   -c -o main.o extract_mvs.c
#all:
#	g++ $(INCLUDE_DIRS) $(CFLAGS) -o mpegflow.o main.cpp
#	g++ mpegflow.o  $(LIB_DIRS) $(LDFLAGS)  -o mpegflow
#-lopencv_imgproc -lopencv_core -lswscale -lavdevice -lavcodec -lswresample -lavformat -lavutil -lpthread -lx264 -lz -lc -lboost_python -lpython2.7 -lm -ldl -llzma -I../bin/dependencies/include -I/usr/include/python2.7 -L../bin/dependencies/lib -L/usr/lib/x86_64-linux-gnu/

all:
	gcc main.cpp -o $(BIN) $(CFLAGS) $(LDFLAGS) $(INCLUDE_DIRS) $(LIB_DIRS)
clean:
	rm $(BIN)

