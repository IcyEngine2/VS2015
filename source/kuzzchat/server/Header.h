#pragma once

#include <iostream>
#include <string>
#include <map>
#include <fstream>
#include "JSON.h"
#include <iostream>
#include <sstream>
#include <WinSock2.h>

using namespace std;
using namespace nlohmann;

enum connection_type
{
	null,
	create,
	login,
	edit
};

enum error_type
{
	success,
	does_not_exist,
	already_exists,
	invalid_pasword
};

struct rcv_data
{
	string cont_type;
	string error_msg;

	json body;
};

rcv_data http_rcv(SOCKET sock)
{
	string rcv_buffer;
	auto header = 0;

	while (true)
	{
		char buffer[4 * 1024] = {};
		auto bytes = recv(sock, buffer, sizeof(buffer), 0);
		if (bytes == -1)
		{
			cout << "Connection lost" << endl;
			rcv_buffer.clear();
			break;
		}
		else if (bytes == 0)
		{
			cout << "User closed the connection" << endl;
			rcv_buffer.clear();
			break;
		}

		rcv_buffer.append(buffer, bytes);

		header = rcv_buffer.find("\r\n\r\n");

		if (header < rcv_buffer.size())
		{
			auto pos = rcv_buffer.find("Content-Length");
			if (pos < rcv_buffer.size())
			{
				auto length = 0;
				sscanf_s(rcv_buffer.data() + pos, "Content-Length:%i", &length);
				if (rcv_buffer.size() == length + header + 4)
					break;
			}
			else
				break;
		}
	}

	auto offset = 0;
	const string ctype_label = "Content-Type: ";
	rcv_data data;
    if (!rcv_buffer.empty())
    {
        offset = rcv_buffer.find(" ") + 1;
        data.error_msg = rcv_buffer.substr(offset, rcv_buffer.find("\r\n", offset));

        offset = rcv_buffer.find(ctype_label) + ctype_label.length();
        data.cont_type = rcv_buffer.substr(offset, rcv_buffer.find("\r\n", offset));

        istringstream(rcv_buffer.substr(header + 4)) >> data.body;
    }
	return data;
}

bool http_reply(SOCKET sock, string input, string content_type, string error)
{
	string output;
	output += "HTTP/1.1 " + error + "\r\n"
		"Content-Type: " + content_type + "\r\n"
		"Content-Length: " + to_string(input.size()) + "\r\n" + "\r\n";

	output.append(input.data(), input.size());

	auto offset = 0;
	while (offset < output.size())
	{
		auto bytes = send(sock, output.data() + offset, output.size() - offset, 0);
		if (bytes <= 0)
		{
			cout << "Lost connection." << endl;
			return false;
		}
		offset += bytes;
	}
	return true;
}

bool http_snd(SOCKET sock, string input, string content_type)
{
	string output;
	output += "POST / HTTP/1.1\r\nContent-Type: " + content_type + "\r\n"
		"Content-Length: " + to_string(input.size()) + "\r\n" + "\r\n";

	output.append(input.data(), input.size());

	auto offset = 0;
	while (offset < output.size())
	{
		auto bytes = send(sock, output.data() + offset, output.size() - offset, 0);
		if (bytes <= 0)
		{
			cout << "Lost connection." << endl;
			return false;
		}
		offset += bytes;
	}
	return true;
}

rcv_data client_data_snd(string login, unsigned password)
{
	rcv_data data;
	data.body["user"] = login;
	data.body["pass"] = password;
	data.cont_type = "application/json";

	return data;
}

