#include "renderer2/showImage.h"

class ShowImageRenderer : public Rendering::BasicRenderer
{
	friend Rendering::RendererFactory;
protected:
	ShowImageRenderer(unsigned width, unsigned height, std::string title) : BasicRenderer(width,height,title) {}
public:
	bool Step()
	{
		Render();

		win.setActive();
		sf::Event ev;
		while( win.pollEvent(ev) )
		{
			if( ev.type == sf::Event::Closed )
				return false;
		}
		return true;
	}
	
	~ShowImageRenderer()
	{
		win.close();
	}
};

void Rendering::ShowImage( cv::Mat &img, unsigned winWidth, unsigned winHeight)
{
	float ar = img.rows / (float)img.cols;
	if( winWidth > 0 && winHeight > 0)
	{
		// do as the user commands!!
	}
	else if( winWidth == 0 && winHeight > 0)
	{
		// set win width given winHeight.
		winWidth = winHeight / ar;
	}
	else if( winWidth > 0 && winHeight == 0)
	{
		// set win height given win width
		winHeight = ar * winWidth;
	}
	else
	{
		// some rational default that ideally takes into account the screen size... but hey...
		winWidth = 1000;
		winHeight = winWidth * ar;
	}

	std::shared_ptr<ShowImageRenderer> ren;
	Rendering::RendererFactory::Create(ren, winWidth, winHeight, "show image") ;
	ren->SetActive();
	ren->SetBGImage(img);
	ren->Get2dBgCamera()->SetOrthoProjection(0,img.cols, 0, img.rows, -100, 100);

	while( ren->Step() );
	ren->SetInactive();
	
	
}
