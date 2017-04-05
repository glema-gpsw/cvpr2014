#ifdef _MSC_VER
#include "inttypes.h"
#endif

#include <stdint.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/motion_vector.h>
}

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <ctime>
#include <string>
#include "common.h"
#include "diag.h"
#include <opencv/cv.h>

using namespace std;
using namespace cv;

#ifndef __FRAME_READER_H__
#define __FRAME_READER_H__


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

struct FrameReader
{
	static const int gridStep = 16;
	Size DownsampledFrameSize;
	Size OriginalFrameSize;
	int FrameCount;
	int frameIndex;
	int64_t prev_pts;
	bool ReadRawImages;
	float time;
	double frameScale;
	int64_t timeBase;
	float fps;
	AVFrame         *pFrame;
	AVFormatContext *pFormatCtx;
	SwsContext		*img_convert_ctx;
	AVStream		*video_st;
	AVIOContext		*pAvioContext;
	uint8_t			*pAvio_buffer;
	AVCodecContext *enc;
	AVCodec *pCodec;
	FILE* in;
	AVFrame rgb_picture;
	int videoStream;
	float height, width;
	int videoFrameCount;
	char *src_filename;
	void print_ffmpeg_error(int err) // copied from cmdutils.c
	{
		char errbuf[128];
		const char *errbuf_ptr = errbuf;

		if (av_strerror(err, errbuf, sizeof(errbuf)) < 0)
			errbuf_ptr = strerror(AVUNERROR(err));
		av_log(NULL, AV_LOG_ERROR, "print_ffmpeg_error: %s\n", errbuf_ptr);
	}
	int open_codec_context(int *stream_idx,
				      AVFormatContext *fmt_ctx, enum AVMediaType type)
	{
	    int ret;
	    AVStream *st;
	    AVCodecContext *dec_ctx = NULL;
	    AVCodec *dec = NULL;
	    AVDictionary *opts = NULL;

	    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
	    if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file '%s'\n",
			av_get_media_type_string(type), src_filename);
		return ret;
	    } else {
		*stream_idx = ret;
		st = fmt_ctx->streams[*stream_idx];

		/* find decoder for the stream */
		dec_ctx = st->codec;
		dec = avcodec_find_decoder(dec_ctx->codec_id);
		if (!dec) {
		    fprintf(stderr, "Failed to find %s codec\n",
			    av_get_media_type_string(type));
		    return AVERROR(EINVAL);
		}

		/* Init the video decoder */
		av_dict_set(&opts, "flags2", "+export_mvs", 0);
		if ((ret = avcodec_open2(dec_ctx, dec, &opts)) < 0) {
		    fprintf(stderr, "Failed to open %s codec\n",
			    av_get_media_type_string(type));
		    return ret;
		}
	    }

	    return 0;
	}
		
	int decode_packet(int *got_frame, int cached, AVPacket pkt)
	{
	    int decoded = pkt.size;

	    *got_frame = 0;

	    if (pkt.stream_index == videoStream) {
		int ret = avcodec_decode_video2(enc, pFrame, got_frame, &pkt);
		if (ret < 0) {
		    fprintf(stderr, "Error decoding video frame");
		    return ret;
		}

		if (*got_frame) {
		    int i;
		    AVFrameSideData *sd;

		    videoFrameCount++;
		    sd = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS);
		    if (sd) {
			const AVMotionVector *mvs = (const AVMotionVector *)sd->data;
			for (i = 0; i < sd->size / sizeof(*mvs); i++) {
			    const AVMotionVector *mv = &mvs[i];
			    printf("%d,%2d,%2d,%2d,%4d,%4d,%4d,%4d,0x%"PRIx64"\n",
				   videoFrameCount, mv->source,
				   mv->w, mv->h, mv->src_x, mv->src_y,
				   mv->dst_x, mv->dst_y, mv->flags);
			}
		    }
		}
	    }

	    return decoded;
}
	FrameReader(string videoPath, bool readRawImages)
	{
		ReadRawImages = readRawImages;
		pAvioContext = NULL;
		pAvio_buffer = NULL;
		in = NULL;
		frameIndex = 1;
		videoStream = -1;
		pFormatCtx = avformat_alloc_context();
		pCodec = NULL;	
		enc = NULL;
		av_register_all();

		int err = 0;

		if ((err = avformat_open_input(&pFormatCtx, videoPath.c_str(), NULL, NULL)) != 0)
		{
			print_ffmpeg_error(err);
			throw std::runtime_error("Couldn't open file");
		}

		if ((err = avformat_find_stream_info(pFormatCtx, NULL)) < 0)
		{
			print_ffmpeg_error(err);
			throw std::runtime_error("Stream information not found");
		}
		if (open_codec_context(&videoStream, pFormatCtx, AVMEDIA_TYPE_VIDEO) >= 0) {
        		video_st = pFormatCtx->streams[videoStream];
        		enc = video_st->codec;
    		}
		if(videoStream == -1)
			throw std::runtime_error("Video stream not found");
		
		av_dump_format(pFormatCtx, 0, src_filename, 0);
		pFrame = av_frame_alloc();
		av_init_packet(&pkt);
    		pkt.data = NULL;
    		pkt.size = 0;
		int cols = enc->width;
		int rows = enc->height;
		width = enc->width;
		height = enc->height;
		FrameCount = video_st->nb_frames;
		frameScale = av_q2d (video_st->time_base);
		fps = av_q2d(video_st->r_frame_rate);
		timeBase = (int64_t(enc->time_base.num) * AV_TIME_BASE) / int64_t(enc->time_base.den);

		if(FrameCount == 0)
		{
			FrameCount = (double)video_st->duration * frameScale;
		}

		DownsampledFrameSize = Size(cols / gridStep, rows / gridStep);
		OriginalFrameSize = Size(cols, rows);


/*
				AVPixelFormat target = AV_PIX_FMT_BGR24;
				img_convert_ctx = sws_getContext(video_st->codec->width,
					video_st->codec->height,
					video_st->codec->pix_fmt,
					video_st->codec->width,
					video_st->codec->height,
					target,
					SWS_BICUBIC,#
					NULL, NULL, NULL);

				avpicture_fill( (AVPicture*)&rgb_picture, NULL,
*/
				//av_log_set_level(AV_LOG_QUIET);
				//av_log_set_callback(av_null_log_callback);
				break;
		
	}

	bool process_frame(AVPacket *pkt)
	{
		av_frame_unref(pFrame);

		int got_frame = 0;
		int ret = avcodec_decode_video2(video_st->codec, pFrame, &got_frame, pkt);
		if (ret < 0)
			return false;

		ret = FFMIN(ret, pkt->size); /* guard against bogus return values */
		pkt->data += ret;
		pkt->size -= ret;
		return got_frame > 0;
	}
	bool GetNextFrame()
	{
		static bool initialized = false;
		AVPacket orig_pkt = pkt;
		int reti, got_frame;
		do
		{
		ret = decode_packet(&got_frame, 0);
		if (ret < 0)
              		break;
		pkt.data += ret;
            	pkt.size -= ret;
		}
		while(pkt.size>0);
		
			if(initialized)
			{
				if(process_frame(&pktCopy)){
					//time = (float)pktCopy.pts*frameScale;
					time = (double)pktCopy.dts*frameScale;
					return true;
				}
				else
				{
					av_free_packet(&pkt);
					initialized = false;
				}
			}

			int ret = av_read_frame(pFormatCtx, &pkt)/;
			
				break;

			initialized = true;
			pktCopy = pkt;
			if(pkt.stream_index != videoStream )
			{
				av_free_packet(&pkt);
				initialized = false;
				continue;
			}
		}

		return process_frame(&pkt);
	}
	
	/*void seek(float time){//, int frameIndex){
		//int64_t seekTarget = (int64_t)(time/frameScale);
		//std::cout<<"seekTarget: "<<seekTarget<<std::endl;
		float time_ = (float) time;
		float frameIndex = time_*fps;
		std::cout<<"Time :"<<time<<std::endl;
		std::cout<<"Time :"<<time_<<std::endl;
		std::cout<<"FrameIndex :"<<frameIndex<<std::endl;
		std::cout<<"FPS :"<<fps<<std::endl;
		//int64_t seekTarget = int64_t(time*fps) * timeBase;
		int64_t seekTarget = (int64_t)(time * (float)AV_TIME_BASE/1000.);
		std::cout<<"seekTarget: "<<seekTarget<<std::endl;
		std::cout<<"AV_TIME_BASE: "<<AV_TIME_BASE<<std::endl;
		av_seek_frame(pFormatCtx, -1, seekTarget, AVSEEK_FLAG_ANY);
	}*/

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

	void ReadMotionVectors(Frame& f)
	{
		// reading motion vectors, see ff_print_debug_info2 in ffmpeg's libavcodec/mpegvideo.c for reference and a fresh doc/examples/extract_mvs.c
		AVFrameSideData* sd = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS);
		
		AVMotionVector* mvs = (AVMotionVector*)sd->data;
		int mbcount = sd->size / sizeof(AVMotionVector);
		MotionVector mv;
		for(int i = 0; i < mbcount; i++)
		{
			AVMotionVector& mb = mvs[i];
			InitMotionVector(mv, mb.src_x, mb.src_y, mb.dst_x - mb.src_x, mb.dst_y - mb.src_y);
			PutMotionVectorInMatrix(mv, f);
		}
	}

	void ReadRawImage(Frame& res)
	{
		rgb_picture.data[0] = res.RawImage.ptr();
		sws_scale(img_convert_ctx, pFrame->data,
			pFrame->linesize, 0,
			video_st->codec->height,
			rgb_picture.data, rgb_picture.linesize);
	}



	Frame Read()
	{
	Frame res(frameIndex, Mat_<float>::zeros(DownsampledFrameSize), Mat_<float>::zeros(DownsampledFrameSize), Mat_<bool>::zeros(DownsampledFrameSize));
	bool read = GetNextFrame();
	
	
	}










/*
	Frame Read()
	{
		TIMERS.ReadingAndDecoding.Start();
		Frame res(frameIndex, Mat_<float>::zeros(DownsampledFrameSize), Mat_<float>::zeros(DownsampledFrameSize), Mat_<bool>::zeros(DownsampledFrameSize));
		res.RawImage = Mat(OriginalFrameSize, CV_8UC3);

		bool read = GetNextFrame();
		if(read)
		{
			res.NoMotionVectors = av_frame_get_side_data(pFrame, AV_FRAME_DATA_MOTION_VECTORS) == NULL;
			res.PictType = av_get_picture_type_char(pFrame->pict_type);
			//fragile, consult fresh f_select.c and ffprobe.c when updating ffmpeg
			res.PTS = pFrame->pkt_pts != AV_NOPTS_VALUE ? pFrame->pkt_pts : (pFrame->pkt_dts != AV_NOPTS_VALUE ? pFrame->pkt_dts : prev_pts + 1);
			prev_pts = res.PTS;
			if(!res.NoMotionVectors)
				ReadMotionVectors(res);
			if(ReadRawImages)
				ReadRawImage(res);
		}
		else
		{
			res = Frame(res.FrameIndex);
			res.PTS = -1;
		}

		TIMERS.ReadingAndDecoding.Stop();

		
			frameIndex++;
		return res;
	}
*/
	~FrameReader()
	{
		//causes double free error. av_free(pFrame);
		//sws_freeContext(img_convert_ctx);
		/*avcodec_close(video_st->codec);
		av_close_input_file(pFormatCtx);*/
		/*if(pAvio_buffer)
			av_free(pAvio_buffer);*/
		/*if(pAvioContext)
			av_free(pAvioContext);*/
		if(in)
			fclose(in);
		avio_close(pFormatCtx->pb);
		pFormatCtx->pb = NULL;
		avformat_free_context(pFormatCtx);
		//if (pFrame)
		//avcodec_free_frame(&pFrame);
			//av_frame_free(&pFrame);
		if (pFrame->extended_data != pFrame->data)
			av_freep(&pFrame->extended_data);
		av_frame_free(&pFrame);
	}
};

#endif
