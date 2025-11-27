#include "misc/imgSender.h"
#include "imgio/loadsave.h"


int main(void)
{
#if BOOST_VERSION < 108000
	boost::asio::io_service ioService;
#else
	boost::asio::io_context ioService;
#endif
	
	ImageSender s(ioService, 123456);
// 	ImageReceiver r(ioService, "127.0.0.1", 123456 );
	
	cv::Mat i = MCDLoadImage("data/testImg2.jpg");
	cv::resize( i, i, cv::Size(128,128) );
	std::vector< cv::Mat > si, ri;
	
	
	si.push_back(i);
	s.SetImages("test", si);
	
	while(1);
	
// 	std::string info;
// 	r.GetImages( info, ri );
// 	
// 	cout << info << " " << ri.size() << endl;
}
