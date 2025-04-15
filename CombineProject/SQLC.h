#pragma once

#include <string>

int DatabaseOperations();
void deleteEmployee();
void updateEmployee();
void selectAllEmployees();
int callback(void* data, int argc, char** argv, char** azColName);
void insertEmployee();
bool isValidDate(const std::string& date);
void closeDatabase();
bool openDatabase();