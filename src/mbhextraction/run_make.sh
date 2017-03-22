c++ main.cpp  `python-config --includes` -o mpegflow.o -fPIC -c -D__STDC_CONSTANT_MACROS -lopencv_imgproc -lopencv_core -lswscale -lavdevice -lavformat -lavcodec -lswresample -lavutil -lpthread -lbz2 -lz -lc -lrt -llzma -lva -lboost_python -lpython2.7 -I../../bin/dependencies/include -L../../bin/dependencies/lib
c++ -o mpegflow.so -shared mpegflow.o -lboost_python -lpython2.7 -lopencv_imgproc -lopencv_core -lswscale -lavdevice -lavformat -lavcodec -lswresample -lavutil -lpthread -lbz2 -lz -lc -lrt -llzma -lva -I../../bin/dependencies/include -L../../bin/dependencies/lib
rm -f /mnt/hd00/action_fixed_fps_skiing/code/mpegflow/module_mpegflow.so
cp mpegflow.so /mnt/hd00/action_fixed_fps_skiing/code/mpegflow/module_mpegflow.so
