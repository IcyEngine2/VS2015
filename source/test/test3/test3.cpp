#include "D:/Downloads/mysql-connector-c++-8.0.25-winx64/include/mysqlx/xdevapi.h"
#include <iostream>

#pragma comment(lib, "D:/Downloads/mysql-connector-c++-8.0.25-winx64/lib64/vs14/mysqlcppconn8.lib")

int main()
{
	try
	{
		mysqlx::Session s(mysqlx::SessionOption::HOST, "localhost", mysqlx::SessionOption::PORT, 33060, mysqlx::SessionOption::USER, "root", mysqlx::SessionOption::PWD, "123456677");
		s.sql(R"(CREATE DATABASE IF NOT EXISTS test111;)").execute();
		s.sql(R"(use test111)").execute();
		s.sql(R"(create table if not exists table_test (a int, b int);)").execute();
		s.sql(R"(insert into table_test values (?,?);)").bind(1).bind(2).execute();
		auto myResult = s.sql(R"(select * from table_test)").execute();
		auto list = myResult.fetchAll();

		for (auto l : list) {
			auto s = l[0];
			auto s1 = l[1];

			std::cout << s << " " << s1 << std::endl;
		}
	}
	catch (...)
	{

	}
}