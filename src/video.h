/*
 * Copyright (c) 2012 Stefano Sabatini
 * Copyright (c) 2014 Clément Bœsch
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
extern "C"{
#include <libavcodec/avcodec.h>
#include <libavutil/motion_vector.h>
#include <libavformat/avformat.h>
}
#include <string>
#include "common.h"
#include <opencv/cv.h>
#include <opencv/cxcore.h>
using namespace cv;

struct MotionVector
{
	int X,Y;
	float Dx,Dy;

	int Mx, My;
	char TypeCode, SegmCode;
	
	static const int NO_MV = -10000;

	bool NoMotionVector()
	{
		return (Dx == NO_MV && Dy == NO_MV) || (Dx == -NO_MV && Dy == -NO_MV);
	}

	bool IsIntra()
	{
		return TypeCode == 'P' || TypeCode == 'A' || TypeCode == 'i' || TypeCode == 'I';
	}
};

#ifndef __FRAME_READER_H__
#define __FRAME_READER_H__

struct FrameReader
{


	static const int gridStep = 16;
	AVFormatContext *fmt_ctx;
	AVCodecContext *video_dec_ctx;
	AVStream *video_stream;

	int video_stream_idx;
	AVFrame *frame;
	AVPacket pkt;
	int video_frame_count;
	int ret, got_frame;
	float time;
	int width, height;
	Size DownsampledFrameSize;
	Size OriginalFrameSize;
	float fps, frameScale;
	int timeBase;
	int frameCount;	
	const char *src_filename = NULL;
	FrameReader(const char *videoPath)
	{
	
	fmt_ctx = NULL;
	video_dec_ctx = NULL;
	video_stream = NULL;
	frame = NULL;
	time = -1.;
	video_stream_idx = -1;
	video_frame_count = 0;
	src_filename = videoPath;
	
	av_register_all();
	if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
		fprintf(stderr, "Could not open source file %s\n", src_filename);
		exit(1);
	}

        if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		fprintf(stderr, "Could not find stream information\n");
		exit(1);
	}

	open_codec_context(fmt_ctx, AVMEDIA_TYPE_VIDEO);
	av_dump_format(fmt_ctx, 0, src_filename, 0);

	if (!video_stream) {
	fprintf(stderr, "Could not find video stream in the input, aborting\n");
	ret = 1;
	release();
	exit(1);
	}

	frame = av_frame_alloc();
	if (!frame) {
	fprintf(stderr, "Could not allocate frame\n");
	ret = AVERROR(ENOMEM);
	release();
	exit(1);
	}


	int cols = video_dec_ctx->width;
	int rows = video_dec_ctx->height;
	width = video_dec_ctx->width;
	height = video_dec_ctx->height;
	frameCount = video_stream->nb_frames;
	frameScale = av_q2d (video_stream->time_base);
	fps = av_q2d(video_stream->r_frame_rate);
	timeBase = (int64_t(video_dec_ctx->time_base.num) * AV_TIME_BASE) / int64_t(video_dec_ctx->time_base.den);

	if(frameCount == 0)
	{
		frameCount = (double)video_stream->duration * frameScale;
	}

	DownsampledFrameSize = Size(cols / gridStep, rows / gridStep);
	OriginalFrameSize = Size(cols, rows);
	}
	
	int open_codec_context(AVFormatContext *fmt_ctx, enum AVMediaType type)
	{
	    int ret;
	    AVStream *st;
	    AVCodecContext *dec_ctx = NULL;
	    AVCodec *dec = NULL;
	    AVDictionary *opts = NULL;

	    ret = av_find_best_stream(fmt_ctx, type, -1, -1, &dec, 0);
	    if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
			av_get_media_type_string(type), src_filename);
		return ret;
	    } else {
		int stream_idx = ret;
		st = fmt_ctx->streams[stream_idx];

		dec_ctx = avcodec_alloc_context3(dec);
		if (!dec_ctx) {
		    fprintf(stderr, "Failed to allocate codec\n");
		    return AVERROR(EINVAL);
		}

		ret = avcodec_parameters_to_context(dec_ctx, st->codecpar);
		if (ret < 0) {
		    fprintf(stderr, "Failed to copy codec parameters to codec context\n");
		    return ret;
		}

		/* Init the video decoder */
		av_dict_set(&opts, "flags2", "+export_mvs", 0);
		if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
		    fprintf(stderr, "Failed to open %s codec\n",
			    av_get_media_type_string(type));
		    return ret;
		}

		video_stream_idx = stream_idx;
		video_stream = fmt_ctx->streams[video_stream_idx];
		video_dec_ctx = dec_ctx;
	    }

	    return 0;
	}

	void PutMotionVectorInMatrix(MotionVector& mv, Frame& f)
	{
		f.width = width;
		f.height = height;
		int i_16 = mv.Y / gridStep;
		int j_16 = mv.X / gridStep;

		i_16 = max(0, min(i_16, DownsampledFrameSize.height-1)); 
		j_16 = max(0, min(j_16, DownsampledFrameSize.width-1));

		if(mv.NoMotionVector())
		{
			f.Missing(i_16,j_16) = true;
		}
		else
		{
			f.Dx(i_16, j_16) = mv.Dx;
			f.Dy(i_16, j_16) = mv.Dy;
		}
	}

	void InitMotionVector(MotionVector& mv, int sx, int sy, int dx, int dy)
	{
		//inverting vectors to match optical flow directions
		dx = -dx;
		dy = -dy;

		mv.X = sx;
		mv.Y = sy;
		mv.Dx = dx;
		mv.Dy = dy;
		mv.Mx = -1;
		mv.My = -1;
		mv.TypeCode = '?';
		mv.SegmCode = '?';
	}

	
	int decode_packet(const AVPacket *pkt, Frame &f, bool &found)
	{
	    int ret = avcodec_send_packet(video_dec_ctx, pkt);
	    if (ret < 0) {
		fprintf(stderr, "Error while sending a packet to the decoder: \n");
		return ret;
	    }

	    while (ret >= 0)  {
		ret = avcodec_receive_frame(video_dec_ctx, frame);
		if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
			break;
		}
		else if (ret< 0) { 
			fprintf(stderr, "Error while receiving a frame from the decoder: \n");
			return ret;
		}   
		if (ret >= 0) {
		    int i;
		    AVFrameSideData *sd;

		    video_frame_count++;
		    found = true;
		    sd = av_frame_get_side_data(frame, AV_FRAME_DATA_MOTION_VECTORS);
		    if (sd) {
			MotionVector mv_;
			const AVMotionVector *mvs = (const AVMotionVector *)sd->data;
			for (i = 0; i < sd->size / sizeof(*mvs); i++) {
			    const AVMotionVector *mv = &mvs[i];
			    InitMotionVector(mv_, mv->src_x, mv->src_y, mv->dst_x - mv->src_x, mv->dst_y - mv->src_y);
			    PutMotionVectorInMatrix(mv_, f);
			}
		    }
		    av_frame_unref(frame);
		}
	    }

	    return 0;
	}



	void release(){
	    avcodec_free_context(&video_dec_ctx);
	    avformat_close_input(&fmt_ctx);
	    av_frame_free(&frame);
	}

	
	Frame Read(){
		
		Frame fr(video_frame_count, Mat_<float>::zeros(DownsampledFrameSize), Mat_<float>::zeros(DownsampledFrameSize), Mat_<bool>::zeros(DownsampledFrameSize));
		bool found = false;
		int ret = 0;
		found = false;
		
		while (!found && av_read_frame(fmt_ctx, &pkt) >= 0) {
			
        		if (pkt.stream_index == video_stream_idx){
            	 		ret = decode_packet(&pkt, fr, found);
				time = (float)pkt.dts*frameScale;
			
			}
			fr.PTS=pkt.pts;	
        		av_packet_unref(&pkt);
        	}
		if (ret < 0){
			fr.PTS = -1;
		}
		return fr;
	}	
	



	~FrameReader(){
		release();
	}
};


#endif

int open_file(const char *src_filename){

	AVFormatContext *fmt_ctx = NULL;
	av_register_all();
	if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
	return 1;
	}
	else{
	return 0;
	}
}
