#include <fstream>
#include <string>
#include <cstdio>
#include <algorithm>
#include <opencv/cv.h>
#include <opencv/cxcore.h>

#include "util.h"
#include "video.h"
#include "descriptors.h"
#include "diag.h"
#include <iterator>
#include <vector>
#include <boost/python.hpp>
#include <Python.h>

using namespace boost::python;
using namespace std;
using namespace cv;

struct Options
{
	string VideoPath;
	bool HogEnabled, HofEnabled, MbhEnabled;
	bool Dense;
	bool Interpolation;

	vector<int> GoodPts;

	Options(string video)
	{
		HogEnabled = false; // we don't actually use them
		HofEnabled = false; //we don't actually use them
		MbhEnabled = true;
		Dense = false;
		Interpolation = true;
		VideoPath = video;
		if(!ifstream(VideoPath.c_str()).good())
			throw runtime_error("Video doesn't exist or can't be opened: " + VideoPath);
	}
};
list get_descriptors(string video, double start =0, double end =-1)
{
	Options opts(video);
	setNumThreads(1);
	const int nt_cell = 3;
	const int tStride = 5;
	vector<Size> patchSizes;
	patchSizes.push_back(Size(32, 32));
	patchSizes.push_back(Size(48, 48));

	DescInfo hofInfo(8+1, true, nt_cell, opts.HofEnabled);
	DescInfo mbhInfo(8, false, nt_cell, opts.MbhEnabled);
	DescInfo hogInfo(8, false, nt_cell, opts.HogEnabled);

	int counter = -1;
	FrameReader rdr(opts.VideoPath, hogInfo.enabled);
	std::cout<<"Frame count: "<<rdr.FrameCount<<std::endl;
	boost::python::list descriptors;
	Size frameSizeAfterInterpolation = 
		opts.Interpolation
			? Size(2*rdr.DownsampledFrameSize.width - 1, 2*rdr.DownsampledFrameSize.height - 1)
			: rdr.DownsampledFrameSize;
	int cellSize = rdr.OriginalFrameSize.width / frameSizeAfterInterpolation.width;
	double fscale = 1 / 8.0;

	HofMbhBuffer buffer(hogInfo, hofInfo, mbhInfo, nt_cell, tStride, frameSizeAfterInterpolation, fscale, rdr.FrameCount, true);

	Frame frame = rdr.Read();
	double time = -1;
	// we read and discard until we get to the start frame
	while(time < start){
		Frame frame = rdr.Read();
		if (frame.PTS == -1){
			descriptors.append(-3.);
			break;
		}
		time = rdr.time;
	}	
	while(true){
		Frame frame = rdr.Read();
		if (frame.PTS == -1) {
		    	descriptors.append(-1.);
			break;
		}
		else if (rdr.time > end){
			descriptors.append(-2.);
			break;
		}
		if(frame.NoMotionVectors || (hogInfo.enabled && frame.RawImage.empty())){
			++counter;
			continue;
		}

		frame.Interpolate(frameSizeAfterInterpolation, fscale);
		buffer.Update(frame, 1);
		if(buffer.AreDescriptorsReady)
		{
			for(int k = 0; k < patchSizes.size(); k++)
			{
				int blockWidth = patchSizes[k].width / cellSize;
				int blockHeight = patchSizes[k].height / cellSize;
				int xStride = opts.Dense ? 1 : blockWidth / 2;
				int yStride = opts.Dense ? 1 : blockHeight / 2;
				buffer.PrintFullDescriptor(blockWidth, blockHeight, xStride, yStride, descriptors);
			}
		}
		++counter;
	}
	return descriptors;
 }

BOOST_PYTHON_MODULE(module_mpegflow) {
    def("run", get_descriptors);
}
