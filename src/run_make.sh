INSTALL_DIR=/mnt/hd00/action_fixed_fps_skiing/code/mpegflow/
c++ main.cpp -Wno-deprecated-declarations -I/usr/include/python2.7/ -o mpegflow.o -fPIC -c -D__STDC_CONSTANT_MACROS -lopencv_imgproc -lopencv_core -lswscale -lavdevice -lavformat -lavcodec -lswresample -lavutil -lpthread -lz -lc -llzma  -lboost_python -lpython2.7 -I../bin/dependencies/include -L../bin/dependencies/lib
c++ -o mpegflow.so -shared mpegflow.o -lboost_python -lpython2.7 -lopencv_imgproc -lopencv_core -lswscale -lavdevice -lavformat -lavcodec -lswresample -lavutil -lpthread -lz -lc -llzma -I../bin/dependencies/include -L../bin/dependencies/lib
cp -f mpegflow.so $INSTALL_DIR
