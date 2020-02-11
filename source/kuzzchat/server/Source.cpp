#define CURL_STATICLIB



#include <queue>
#include <string>
#include "Header.h"
#include <thread>

#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmainwindow.h>
#include <QtCore/qplugin.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qtextedit.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/QMessageBox>
#include <QtGui/qstandarditemmodel.h>
#include <QtCore/qsortfilterproxymodel.h>
#include <QtWidgets/qlistview.h>
#include <QtWidgets/qtableview.h>
#include <QtWidgets/qtreeview.h>
#include <QtWidgets/qfilesystemmodel.h>
#include <QtWidgets/QSplitter>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QMenu>
#include <QtWidgets/QAction>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qboxlayout.h>
#include <QtGui/qdesktopservices.h>




using namespace std;



#pragma comment (lib, "ws2_32.lib") 

#pragma comment(lib, "Qt5Cored")
#pragma comment(lib, "Qt5Guid")
#pragma comment(lib, "Qt5Widgetsd")
#pragma comment(lib, "Qt5EventDispatcherSupportd")
#pragma comment(lib, "Qt5FontDatabaseSupportd")
#pragma comment(lib, "Qt5WindowsUIAutomationSupportd")
#pragma comment(lib, "Qt5ThemeSupportd")
#pragma comment(lib, "qwindowsd")
#pragma comment(lib, "qtlibpngd")
#pragma comment(lib, "qtfreetyped")

#pragma comment(lib, "user32")
#pragma comment(lib, "Advapi32")
#pragma comment(lib, "ws2_32")
#pragma comment(lib, "Shell32")
#pragma comment(lib, "Ole32")
#pragma comment(lib, "Version")
#pragma comment(lib, "Netapi32")
#pragma comment(lib, "Userenv")
#pragma comment(lib, "Winmm")
#pragma comment(lib, "Imm32")
#pragma comment(lib, "gdi32")
#pragma comment(lib, "Wtsapi32")
#pragma comment(lib, "Dwmapi")

Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);

struct launch_info
{
	unsigned host_port = 0;
	int max_client = 0;
};

struct user_info
{
	string login;
	unsigned pasword = 0;
	SOCKET sock = INVALID_SOCKET;
	queue<json> msgs; //UWU
	thread thread;

};





map<string, user_info> users;




void user_rcv_thread(user_info& user)
{
	while (true)
	{
		json json;

		auto msg = http_rcv(user.sock);
		
		if (!msg.body.is_object())
			break;
		json["user"] = user.login;
		json["msg"] = msg.body["msg"];
		for (auto& other_user:users)
			if (user.login != other_user.first)
			{
				other_user.second.msgs.push(json);
			}
	}
}

void user_snd_thread()
{
	while (true)
	{
		for (auto& user : users)
		{
			while (!user.second.msgs.empty())
			{
				string snd_msg;
				ostringstream(snd_msg) << user.second.msgs.front();
				user.second.msgs.pop();
				http_snd(user.second.sock,snd_msg,"application/json");
			}
		}
	}
}

void server_thread(launch_info info)
{

	WSADATA wsa = {};
	WSAStartup(MAKEWORD(2, 2), &wsa);
	SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in srvAddr = {};
	srvAddr.sin_port = htons(info.host_port);
	srvAddr.sin_family = AF_INET;

	if (::bind(server, (sockaddr*)&srvAddr, sizeof(srvAddr)) < 0)
		cout << "bind fail compilation" << endl;

	if (listen(server, 5) < 0)
		cout << "listen fail compilation" << endl;

	std::cout << "Ready to HTTP" << std::endl;

	while (true)
	{
		sockaddr_in usrAddr = {};
		int size = sizeof(usrAddr);
		SOCKET client = accept(server, (sockaddr*)&usrAddr, &size);
		std::cout << "got new connection b***es, from Ip:"
			<< (int)usrAddr.sin_addr.S_un.S_un_b.s_b1 << "."
			<< (int)usrAddr.sin_addr.S_un.S_un_b.s_b2 << "."
			<< (int)usrAddr.sin_addr.S_un.S_un_b.s_b3 << "."
			<< (int)usrAddr.sin_addr.S_un.S_un_b.s_b4 << endl;

		auto data = http_rcv(client);

		auto type = connection_type(data.body["type"]);
		string login_str = data.body["user"];
		int pass = data.body["pass"];

		error_type error;

		switch (type)
		{
		case null:
			break;
		case create:
		{
			if (users.find(login_str) == users.end())
			{

				auto& new_user = users[login_str];
				new_user.login = login_str;
				new_user.pasword = pass;
				new_user.sock = client;

				//!!!!!!!
				new_user.thread = thread
				{
					user_rcv_thread,ref(new_user)
				};
				//!!!!!!!!
			}
			else
			{
				error = error_type::already_exists;
			}
		}
			break;
		case login:
		{
			auto search = users.find(login_str);
			if (search != users.end())
			{
				auto& user = search->second;
				if (user.pasword == pass)
				{
					user.sock = client;
					user.thread = thread
					{
						user_rcv_thread,ref(user)
					};
				}
				else
				{
					error = error_type::invalid_pasword;
				}
		
			}
			else
			{
				error = error_type::does_not_exist;
			}
			break;
		}
			
		case edit:
			break;
		}

		http_snd(client, to_string(error), "text/plain");

		if (error)
			closesocket(client);
	}
}

bool launch_win(launch_info& info)
{
	auto result = false;

	QDialog win;
	QGridLayout grid(&win);
	win.setWindowModality(Qt::WindowModality::ApplicationModal);

	//---------------------------------------------------
	QLabel host_i;
	host_i.setText("Host Port");
	QLabel max_client_i;
	max_client_i.setText("max Clients");

	//---------------------------------------------------
	QLineEdit host;
	QLineEdit max_client;
	//---------------------------------------------------
	QPushButton ok;
	QPushButton cancel;
	ok.setText("OK");
	cancel.setText("Cancel");
	//---------------------------------------------------
	grid.addWidget(&host_i, 0, 0);
	grid.addWidget(&host, 0, 1);
	grid.addWidget(&max_client_i, 1, 0);
	grid.addWidget(&max_client, 1, 1);
	grid.addWidget(&ok, 2, 0);
	grid.addWidget(&cancel, 2, 1);
	//----------------------------------------------------
	host.setText(QString::number(info.host_port));
	max_client.setText(QString::number(info.max_client));
	//----------------------------------------------------
	QObject::connect(&ok, &QPushButton::clicked, [&]
	{
		info.host_port = host.text().toInt();
		info.max_client = max_client.text().toInt();
		result = true;
		win.close();

	});
	QObject::connect(&cancel, &QPushButton::clicked, [&]
	{
		win.close();
	});

	win.show();
	win.exec();
	return result;
}


int main(int count, char** args)
{
	QApplication app(count, args);
	QApplication::setStyle("Fusion");
	QMainWindow win;
	QWidget tmp;
	win.setCentralWidget(&tmp);
	win.setGeometry(500, 350, 550, 260);
	win.setFixedSize(550, 260);

	thread net_thread;

	launch_info info;

	QSplitter v_split(&tmp);
	v_split.setOrientation(Qt::Orientation::Horizontal);



	QWidget tmp_u;
	QWidget tmp_d;
	QWidget tmp_l;
	QWidget tmp_r;



	v_split.addWidget(&tmp_l);
	v_split.addWidget(&tmp_r);

	tmp_r.setFixedWidth(150);


	QVBoxLayout vbox_chat(&tmp_l);
	QVBoxLayout vbox_users(&tmp_r);


	QTextEdit chatbox;
	vbox_chat.addWidget(&chatbox);
	chatbox.setReadOnly(true);



	QTextEdit userbox;
	vbox_users.addWidget(&userbox);
	userbox.setReadOnly(true);

	QLabel label_users;
	label_users.setText("Users Online");
	vbox_users.addWidget(&label_users, 1);

	QHBoxLayout hbox_input;
	vbox_chat.addLayout(&hbox_input, 2);



	QLineEdit line;
	hbox_input.addWidget(&line);

	QPushButton btn_input;
	btn_input.setText("Send");
	hbox_input.addWidget(&btn_input);


	QMenuBar menuBar;
	QMenu*   m_main = new QMenu("&Main");

	QAction launch;
	launch.setText("&Launch");


	m_main->addAction(&launch);
	m_main->addAction("&Exit", &win, SLOT(close()), Qt::CTRL + Qt::Key_E);

	win.setMenuBar(&menuBar);
	menuBar.addMenu(m_main);

	QObject::connect(&launch, &QAction::triggered, [&chatbox,&info,&net_thread]
	{
		if (launch_win(info))
		{
			net_thread = thread
			{
				 server_thread,
				 info
			};
		}
	
	});


	QObject::connect(&btn_input, &QPushButton::clicked, [&chatbox]
		{
		cout << "Blabalbaba";
		});

	menuBar.show();
	win.show();
	app.exec();
	return 0;
}