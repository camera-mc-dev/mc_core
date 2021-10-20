#include "misc/imgSender.h"
#include "imgio/loadsave.h"


int main(void)
{
	boost::asio::io_service ioService;
	
	ImageReceiver r(ioService, "127.0.0.1", 123456 );
	std::vector< cv::Mat > ri;
	
	std::string info;
	r.GetImages( info, ri );
	cout << 1 << " " << info << " " << ri.size() << endl;
	
	ri.clear();
	r.GetImages( info, ri );
	cout << 2 << " " << info << " " << ri.size() << endl;
}
