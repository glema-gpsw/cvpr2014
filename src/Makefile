# Will link statically with OpenCV, FFmpeg

CFLAGS = -O3 -D__STDC_CONSTANT_MACROS
LDFLAGS = -lopencv_imgproc -lopencv_core -lswscale -lavdevice -lavformat -lavcodec -lswresample -lavutil -lpthread -lz -lc -lib:libva.so.1 -lrt -llzma -lboost_python -lpython2.7
BIN = fastvideofeat

all:
	g++ main.cpp -o $(BIN) $(CFLAGS) $(LDFLAGS)  `python-config --includes`
clean:
	rm $(BIN)
