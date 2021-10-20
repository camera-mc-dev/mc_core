#ifndef IMAGES_H_MC
#define IMAGES_H_MC

#include <string>
#include <cv.hpp>

cv::Mat LoadImage(std::string filename);
void SaveImage(cv::Mat &img, std::string filename);

#endif