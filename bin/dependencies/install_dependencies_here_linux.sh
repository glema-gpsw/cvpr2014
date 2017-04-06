# This script will try to download and install from sources opencv 2.4.9 (with minimal set of modules), ffmpeg 2.4, yasm 1.3.0 (required by ffmpeg), yael 4.38, ATLAS 3.10.2 and LAPACK 3.5.0 (required by yael).
# I cannot guarantee it will work on absolutely all systems, hopefully it still provides guidance.

git clone https://git.videolan.org/git/ffmpeg.git
cd ffmpeg
./configure --enable-shared --prefix=$(pwd)/.. --yasmexe=$(pwd)/../bin/yasm --enable-gpl --enable-libx264
make && make install

