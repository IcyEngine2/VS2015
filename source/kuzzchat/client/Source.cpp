#define CURL_STATICLIB
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <iostream>
#include "../server/Header.h"
#include <map>
#include <string>
#include <fstream>
#include "../server/JSON.h"
#include <iostream>
#include <sstream>
#include <thread>


#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmainwindow.h>
#include <QtCore/qplugin.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qtextedit.h>
#include <QtWidgets/qgridlayout.h>
#include <string>
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
#include <QtWidgets/qslider.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qboxlayout.h>
#include <QtGui/qdesktopservices.h>
#include <QtWidgets/qcheckbox.h>

using namespace std;
using namespace nlohmann;

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

//struct my_info
//{
//	string login;
//	unsigned pass;
//	SOCKET soc;
//};

struct connect_info
{
	string ip;
	string login;
	unsigned pass;
	SOCKET soc;
};


void network_thread(connect_info info, connection_type type)
{
	WSADATA wsa = {};
	WSAStartup(MAKEWORD(2, 2), &wsa);
	SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	sockaddr_in srvAddr = {};
	auto host = gethostbyname(info.ip.substr(0,info.ip.find(":")).data());
	srvAddr.sin_port = htons(atoi(info.ip.substr(info.ip.find(":")+1).data()));
	srvAddr.sin_family = AF_INET;
	srvAddr.sin_addr.s_addr = *(uint32_t*)(host->h_addr);

	

	if (connect(client, (sockaddr*)&srvAddr, sizeof(srvAddr)) != 0)
	{
		std::cout << "Error connecting" << std::endl;
		return;
	}
	auto data = client_data_snd(info.login, info.pass);
	string snd_login;
	ostringstream(snd_login) << data.body;
	http_snd(client, snd_login, data.cont_type);
	data = http_rcv(client);

	error_type error = error_type(atoi(data.error_msg.data()));
	switch (error)
	{
	case success:
	{
		cout << "Fine...";
	}
		break;
	case does_not_exist:
		cout << "NETU";
		break;
	case already_exists:
		cout << "UZHE EST";
		break;
	case invalid_pasword:
		cout << "!PARVILNO";
		break;
	default:
		break;
	}
	
}

int hash(const std::string& str)
{
	auto value = 0x811C9DC5;
	for (auto chr : str)
	{
		value = (value ^ chr) * 0x01000193;
	}
	return value;
}

bool connect_win(connect_info& info)
{
	QDialog win;
	QGridLayout grid(&win);
	win.setWindowModality(Qt::WindowModality::ApplicationModal);

//---------------------------------------------------
	QLabel ip_i;
	ip_i.setText("Input IP");
	QLabel login_i;
	login_i.setText("Input Login");
	QLabel pass_i;
	pass_i.setText("Input Pass");
	
//---------------------------------------------------
	QLineEdit ip;
	QLineEdit login;
	QLineEdit pass;
	pass.setPlaceholderText("Your Password");
	pass.setEchoMode(QLineEdit::EchoMode::Password);
//---------------------------------------------------
	QPushButton ok;
	QPushButton cancel;
	ok.setText("OK");
	cancel.setText("Cancel");
//---------------------------------------------------
	grid.addWidget(&ip_i,0,0);
	grid.addWidget(&ip,0,1);
	grid.addWidget(&login_i,1,0);
	grid.addWidget(&login,1,1);
	grid.addWidget(&pass_i,2,0);
	grid.addWidget(&pass,2,1);
	grid.addWidget(&ok, 3, 0);
	grid.addWidget(&cancel, 3, 1);
//----------------------------------------------------
	ip.setText(QString::fromStdString(info.ip));
	login.setText(QString::fromStdString(info.login));
//----------------------------------------------------
	auto result = false;
	QObject::connect(&ok, &QPushButton::clicked, [&]
	{
		info.ip = ip.text().toStdString();
		info.login = login.text().toStdString();
		if (!pass.text().isEmpty())
			::hash(pass.text().toStdString());
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



int __stdcall wWinMain(HINSTANCE__*, HINSTANCE__*, wchar_t*, int)
{
#pragma region init_qt
	QApplication app(__argc, __argv);
	QApplication::setStyle("Fusion");
	QMainWindow win;
	QWidget tmp;
	win.setCentralWidget(&tmp);
	win.setGeometry(500, 350, 550, 260);
	win.setFixedSize(550, 260);

	
	connect_info info;
	
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

	QAction connect;
	connect.setText("&Connect");	


	m_main->addAction(&connect);
	m_main->addAction("&Exit", &win, SLOT(close()), Qt::CTRL + Qt::Key_E);

	win.setMenuBar(&menuBar);
	menuBar.addMenu(m_main);
#pragma endregion QT INITIALIZATION
	
	thread net_thread;
	QObject::connect(&connect, &QAction::triggered, [&chatbox, &info, &net_thread]
	{
		if (connect_win(info))
		{
			net_thread = thread
			{
				network_thread, info,connection_type::login
			};
		}
	});


	QObject::connect(&btn_input, &QPushButton::clicked, [&line, &net_thread, &info]
	{
			if (line.text() != 0)
			{		
				http_snd, info.soc, line.text(), "text/plain";
			}
	});
	
	menuBar.show();
	win.show();
	app.exec();
	if (net_thread.joinable())
		net_thread.join();
	return 0;
}