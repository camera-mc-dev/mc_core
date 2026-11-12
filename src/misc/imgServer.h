#ifndef MC_DEV_IMG_SERVER
#define MC_DEV_IMG_SERVER

#include <opencv2/opencv.hpp>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

// Forward declaration of httplib server to keep headers clean
namespace httplib { class Server; }

class ImgServer
{
public:
	ImgServer(std::string title="mcdev image server", int port = 8080, int jpegQuality = 80);
	~ImgServer();
	
	// Non-blocking: updates the latest frame for active streams
	void SetImage(const cv::Mat& image);
	
private:
	
	std::string title;
	
	void ServerThreadFunc(int port);
	
	int m_jpegQuality;
	std::atomic<bool> m_running;
	
	cv::Mat m_currentFrame;
	std::mutex m_mutex;
	
	httplib::Server* m_server;
	std::thread m_workerThread;
};

#endif
