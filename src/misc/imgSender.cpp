#include "imgSender.h"

void ReadToNewLine( boost::asio::ip::tcp::socket *socket, std::stringstream &ss )
{
	char ch = ' ';
	while( ch != '\n' )
	{
		boost::asio::read(*socket, boost::asio::buffer( &ch, 1 ) );
		ss << ch;
	}
}
