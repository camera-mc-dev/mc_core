#ifndef IMAGES_H_MC
#define IMAGES_H_MC

#include <string>
#include <cv.hpp>
#include "math/mathTypes.h"

cv::Mat LoadImage(std::string filename);
void SaveImage(cv::Mat &img, std::string filename);

void SaveCFImage( cfMatrix &img, std::string filename );
cfMatrix LoadCFImage(std::string filename);

// saves a float buffer with multiple channels
// don't provide a file extension, it will be set automatically to .fbuf
void SaveFBuffer( int rows, int cols, int channels, std::vector<float> &inBuff, std::string fn );
void LoadFBuffer( std::string filename, std::vector<float> &outBuff, int &rows, int &cols, int &channels );

#endif
