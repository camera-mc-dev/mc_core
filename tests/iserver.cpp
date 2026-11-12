#include "misc/imgServer.h"
#include "imgio/loadsave.h"


int main(void)
{
	ImgServer s;
	
	cv::Mat i = LoadImage("data/testCard.png");
	cv::resize( i, i, cv::Size(128,128) );
	std::vector< cv::Mat > si, ri;
	
	
	s.SetImage( i );
	
	while(1);
	

}
