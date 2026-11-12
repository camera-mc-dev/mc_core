#include "ImgServer.h"
//#define CPPHTTPLIB_OPENSSL_SUPPORT // optional, remove if using plain http
#include "httplib.h"
#include <chrono>

ImgServer::ImgServer(std::string title, int port, int jpegQuality) 
	: title(title), m_jpegQuality(jpegQuality), m_running(true), m_server(nullptr) 
{
	// Start the HTTP server in a background thread so it never steals your main loop
	m_workerThread = std::thread(&ImgServer::ServerThreadFunc, this, port);
}

ImgServer::~ImgServer()
{
	m_running = false;
	if (m_server) 
	{
		m_server->stop();
	}
	if (m_workerThread.joinable()) 
	{
		m_workerThread.join();
	}
	delete m_server;
}

void ImgServer::SetImage(const cv::Mat& image)
{
	std::lock_guard<std::mutex> lock(m_mutex);
	if (!image.empty()) {
		// Clone ensures thread safety against in-place modifications in your main loop
		image.clone().copyTo(m_currentFrame);
	}
}

void ImgServer::ServerThreadFunc(int port)
{
	m_server = new httplib::Server();

	// MJPEG Streaming Endpoint
	m_server->Get("/stream", [this](const httplib::Request&, httplib::Response& res)
	{
		res.set_content_provider(
			"multipart/x-mixed-replace; boundary=frame",
			[this](size_t /*offset*/, httplib::DataSink& sink) 
			{
				cv::Mat frameToSend;
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					if (m_currentFrame.empty()) 
					{
						std::this_thread::sleep_for(std::chrono::milliseconds(10));
						return true; // Keep connection open waiting for first frame
					}
					m_currentFrame.copyTo(frameToSend);
				}

				std::vector<uchar> buf;
				cv::imencode(".jpg", frameToSend, buf, {cv::IMWRITE_JPEG_QUALITY, m_jpegQuality});

				std::string header = "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: " + 
									std::to_string(buf.size()) + "\r\n\r\n";
				
				if (!sink.write(header.data(), header.size()) ||
					!sink.write((const char*)buf.data(), buf.size()) ||
					!sink.write("\r\n", 2)) 
				{
					return false; // Client disconnected
				}

				// Cap the streaming frame rate to save CPU (e.g., ~30 FPS max)
				std::this_thread::sleep_for(std::chrono::milliseconds(33));
				return true;
			}
		);
	});

	// Simple index page with an embedded stream tag
	m_server->Get("/", [this](const httplib::Request&, httplib::Response& res)
	{
		std::stringstream ss;
		ss << "<html><body>"
		   << "<h2>" << title <<  "</h2>"
		   << "<img src=\"/stream\" style=\"max-width:100%; height:auto;\" />"
		   << "</body></html>";
		res.set_content( ss.str(), "text/html");
	});

	m_server->listen("0.0.0.0", port);
}
