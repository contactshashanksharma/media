rm -rf ffmpeg-sample
gcc -o ffmpeg-sample ffmpeg-sample.c -g -lavformat -lavcodec -lswresample -lswscale -lavutil -lm -lz -lpthread
