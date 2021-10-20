#ifndef MC_DEV_IMG_SENDER
#define MC_DEV_IMG_SENDER


#include <boost/asio.hpp>
#include <thread>
#include <mutex>
#include <chrono>

#include <cv.hpp>

#include <iostream>
using std::cout;
using std::endl;

void ReadToNewLine( boost::asio::ip::tcp::socket *socket, std::stringstream &ss )
{
	char ch = ' ';
	while( ch != '\n' )
	{
		boost::asio::read(*socket, boost::asio::buffer( &ch, 1 ) );
		ss << ch;
	}
}

class ImageSender
{
public:
	ImageSender( boost::asio::io_service &in_ioService, unsigned port )
	{
		ioService = &in_ioService;
		acceptor = new boost::asio::ip::tcp::acceptor( *ioService, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port ) );
		
		// start sender thread.
		sthread = std::thread( &ImageSender::SenderLoop, this );
	}
	
	~ImageSender()
	{
		// quit the thread...
	}
	
	void SetImages( std::string in_infoString, std::vector<cv::Mat> in_imgs )
	{
		lock.lock();
		
		infoString = in_infoString;
		imgs.resize( in_imgs.size() );
		for( unsigned ic = 0; ic < in_imgs.size(); ++ic )
		{
			imgs[ic] = in_imgs[ic].clone();
		}
		
		lock.unlock();
	}
	
	void SenderLoop()
	{
		while(1)
		{
			socket = new boost::asio::ip::tcp::socket( *ioService );
			acceptor->accept( *socket );
			
			bool goodConnection = true;
			while( goodConnection )
			{
				// 1) wait for requests
				boost::asio::streambuf buf;
				try
				{
					boost::asio::read_until( *socket, buf, "\n" );
				}
				catch(...)
				{
					goodConnection = false;
					continue;
				}
				std::string rq = boost::asio::buffer_cast<const char*>(buf.data());
				
				 // cout << "img sender got: " << rq << endl;
				
				lock.lock();
				
				// 2) send header
				std::stringstream header;
				header << "imgStart " << infoString.size() << " " << imgs.size() << "\n";
				try
				{
					boost::asio::write( *socket, boost::asio::buffer( header.str() ) );
				}
				catch(...)
				{
					goodConnection = false;
					continue;
				}
				
				// 3) send infoString
				try
				{
					boost::asio::write( *socket, boost::asio::buffer( infoString ) );
				}
				catch(...)
				{
					goodConnection = false;
					continue;
				}
				
				// 4) send images
				for( unsigned ic = 0; ic < imgs.size(); ++ic )
				{
					int nb;
					switch( imgs[ic].type() )
					{
						case CV_32FC1:
							nb = imgs[ic].rows * imgs[ic].cols * 1 * sizeof(float);
							break;
						
						case CV_32FC3:
							nb = imgs[ic].rows * imgs[ic].cols * 3 * sizeof(float);
							break;
						
						case CV_8UC1:
							nb = imgs[ic].rows * imgs[ic].cols * 1 * sizeof(char);
							break;
						
						case CV_8UC3:
							nb = imgs[ic].rows * imgs[ic].cols * 3 * sizeof(char);
							break;
					}
					header.str("");
					header << imgs[ic].rows << " " << imgs[ic].cols << " " << imgs[ic].type() << " " << nb << "\n";
					try
					{
						boost::asio::write( *socket, boost::asio::buffer( header.str() ) );
					}
					catch(...)
					{
						goodConnection = false;
						continue;
					}
					
					try
					{
						boost::asio::write( *socket, boost::asio::buffer( imgs[ic].data, nb ) );
					}
					catch(...)
					{
						goodConnection = false;
						continue;
					}
					
// 					// send up to toWrite bytes at a time.
// 					int toWrite = 256;
// 					int bc = 0;
// 					while( goodConnection && bc < nb )
// 					{
// 						int wb = std::min( toWrite, nb-bc );
// 						int sent = boost::asio::write( *socket, boost::asio::buffer( imgs[ic].data + bc, wb ) );
// 						cout << "sent: " << bc / (float) nb << " " << wb << " " << bc << " " << bc + wb << " " << nb << endl;
// 						bc += toWrite;
// 					}
// 					cout << "sent: " << bc / (float) nb << endl;
// 					
				}
				
				//
				// It seems like Boost's sockets don't sent until there is
				// a minimum quantity of data in the buffer.
				//
				// That's fine if we have a constant stream of data, but 
				// I don't want to work that way. So I'll send a bunch of
				// extra data to "flush" the image through the socket.
				//
				// The cost of this is that we will have to handle the extra
				// data at the reciever when we start the next image request.
				//
				try
				{
					std::vector<unsigned char> extra(10, 0);
					boost::asio::write( *socket, boost::asio::buffer( "start_extra" ) );
					boost::asio::write( *socket, boost::asio::buffer(extra ) );
					boost::asio::write( *socket, boost::asio::buffer( "end_extra\n" ) );
				}
				catch(...)
				{
					goodConnection = false;
				}
				
				
				lock.unlock();
				
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			
			delete socket;
		}
	}
	
private:
	
	boost::asio::ip::tcp::acceptor *acceptor;
	boost::asio::ip::tcp::socket   *socket;
	boost::asio::io_service        *ioService;
	
	std::thread sthread;
	std::mutex  lock;
	
	std::vector<cv::Mat> imgs;
	std::string infoString;
	
	
	
	
};

class ImageReceiver
{
public:
	ImageReceiver( boost::asio::io_service &ioService, std::string address, unsigned port )
	{
		socket   = new boost::asio::ip::tcp::socket( ioService );
		
		socket->connect( boost::asio::ip::tcp::endpoint( boost::asio::ip::address::from_string( address ), port ) );
		
		boost::asio::socket_base::receive_buffer_size option;
		socket->get_option(option);
		cout << "rd buf size: " << option.value() << endl;
	}
	
	void GetImages( std::string &infoString, std::vector< cv::Mat > &imgs )
	{
		// 1) send ready
		boost::asio::write( *socket, boost::asio::buffer( "ready\n" ) );
		
		// 2) read response from server.
		//
		// Now, this is where things get tricky. We might get the header,
		// or we might get the junk "flush" data we had to send to make
		// sure all the image data went through.
		//
		// a) header
// 		boost::asio::streambuf hbuf;
// 		boost::asio::read_until( *socket, hbuf, "\n" );
// 		hss << boost::asio::buffer_cast<const char*>( hbuf.data() );
		
		std::stringstream hss;
		ReadToNewLine( socket, hss );
// 		cout << "hss: " << hss.str() << " :hss" << endl;
		if( hss.str().find("imgStart") == std::string::npos )
		{
// 			boost::asio::streambuf hbuf2;
// 			boost::asio::read_until( *socket, hbuf2, "\n" );
// 			hss << boost::asio::buffer_cast<const char*>( hbuf2.data() );
			
			hss.str("");
			ReadToNewLine( socket, hss );
			
// 			cout << "hss 2: " << hss.str() << endl;
			if( hss.str().find("imgStart") == std::string::npos )
			{
				cout << "Couldn't find imgStart :( " << endl;
			}
		}
		
		std::string ist;
		int infoSize, numImgs;
		hss >> ist;
		hss >> infoSize;
		hss >> numImgs;
		cout << ist << " " << infoSize << " " << numImgs << endl;
		
		// b) info string
		boost::asio::streambuf istrbuf;
		boost::asio::read(*socket, istrbuf, boost::asio::transfer_exactly( infoSize ) );
		infoString = boost::asio::buffer_cast<const char*>( istrbuf.data() );
		
		// c) images
		imgs.resize( numImgs );
		for( unsigned ic = 0; ic < numImgs; ++ic )
		{
			// c1) image info.
// 			boost::asio::streambuf iibuf;
// 			boost::asio::read_until( *socket, iibuf, "\n" );
// 			
// 			std::stringstream iiss;
// 			iiss << boost::asio::buffer_cast<const char*>( iibuf.data() );
			
			//
			// I would love to know why read_until didn't work, but, this does.
			//
			char ch = ' ';
			std::stringstream iiss;
			while( ch != '\n' )
			{
				boost::asio::read(*socket, boost::asio::buffer( &ch, 1 ) );
				iiss << ch;
			}
// 			cout << "iiss: " << iiss.str() << " :iiss" << endl;
			
			int r, c, t, nb;
			iiss >> r;
			iiss >> c;
			iiss >> t;
			iiss >> nb;
			
			
			imgs[ic] = cv::Mat( r, c, t );
			boost::asio::read(*socket, boost::asio::buffer( imgs[ic].data, nb ) );
			
// 			// read up to toRead bytes at a time.
// 			int toRead = 256;
// 			int bc = 0;
// 			while( bc < nb )
// 			{
// 				
// 				int rb = std::min( nb - bc, toRead );
// 				cout << "read: " << ic << " " << bc / (float) nb << " " << rb << " " << bc << " " << bc + rb << " " << nb << endl;
// 				boost::asio::read(*socket, boost::asio::buffer( imgs[ic].data + bc, rb ) );
// 				bc += toRead;
// 			}
		}
	}
	
	
private:
	
	boost::asio::ip::tcp::socket   *socket;
};


#endif
