#include <windows.h>
#include <string>
#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <ctime>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <fstream>
#include <regex>
#include "SQLC.h"
#include "EmailSMTP.h"

using namespace std;

// Структура для хранения данных сотрудника
struct Employee {
    int id;
    string name;
    string email;
    string birthday;
};

// Функция загрузки шаблона письма
string loadTemplate(const string& filename, const string& fullName) {
    ifstream file(filename);
    if (!file.is_open()) {
        cerr << "Не удалось открыть шаблон: " << filename << endl;
        return "";
    }

    stringstream buffer;
    buffer << file.rdbuf();
    string content = buffer.str();

    // Удаляем BOM, если он есть (UTF-8 BOM: 0xEF,0xBB,0xBF)
    if (content.size() >= 3 &&
        static_cast<unsigned char>(content[0]) == 0xEF &&
        static_cast<unsigned char>(content[1]) == 0xBB &&
        static_cast<unsigned char>(content[2]) == 0xBF) {
        content = content.substr(3);
    }

    content = regex_replace(content, regex("\\{фио сотрудника\\}"), fullName);
    return content;
}

// Функция поиска именинников
bool GetBirthdayEmployeesToday(vector<Employee>& birthdayEmployees, sqlite3* db) {
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    char today_dd_mm[6]; // Форматируем день и месяц в "DD.MM"
    snprintf(today_dd_mm, sizeof(today_dd_mm), "%02d.%02d", localTime.tm_mday, localTime.tm_mon + 1);

    const char* sql = "SELECT id, name, email, birthday FROM employees WHERE birthday LIKE ? || '%';";
    sqlite3_stmt* stmt;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        cerr << "Ошибка подготовки запроса: " << sqlite3_errmsg(db) << endl;
        return false;
    }

    if (sqlite3_bind_text(stmt, 1, today_dd_mm, 5, SQLITE_STATIC) != SQLITE_OK) {
        cerr << "Ошибка привязки параметра: " << sqlite3_errmsg(db) << endl;
        sqlite3_finalize(stmt);
        return false;
    }

    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Employee emp;
        emp.id = sqlite3_column_int(stmt, 0);
        emp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        emp.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        emp.birthday = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
        
        if (!emp.email.empty()) {
            birthdayEmployees.push_back(emp);
            found = true;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}

// Функция, которая определяет какой сегодня день - выходной или рабочий
void checkDay() {
    // Структура для хранения праздников
    struct Holiday {
        int day;
        int month;
    };

    // Функция для проверки на праздник
    auto isHoliday = [](int day, int month) {
        vector<Holiday> holidays = {
            {1, 1},   // Новый год
            {2, 1},   // Новогодние каникулы
            {3, 1},   // Новогодние каникулы
            {4, 1},   // Новогодние каникулы
            {5, 1},   // Новогодние каникулы
            {6, 1},   // Новогодние каникулы
            {7, 1},   // Рождество Христово
            {8, 1},   // Новогодние каникулы
            {23, 2},  // День защитника Отечества
            {8, 3},   // Международный женский день
            {1, 5},   // Праздник Весны и Труда
            {9, 5},   // День Победы
            {12, 6},  // День России
            {4, 11}   // День народного единства
        };

        for (const auto& holiday : holidays) {
            if (holiday.day == day && holiday.month == month) {
                return true;
            }
        }
        return false;
    };

    // Функция для проверки на выходной день
    auto isWeekend = [](int dayOfWeek) {
        return (dayOfWeek == 0 || dayOfWeek == 6); // Воскресенье - 0, Суббота - 6
    };

    // Получаем текущее время
    time_t t = time(nullptr);
    tm currentTime;
    localtime_s(&currentTime, &t);

    // Извлекаем день и месяц
    int day = currentTime.tm_mday;
    int month = currentTime.tm_mon + 1;

    // Вычисляем день недели
    tm timeInfo = {};
    timeInfo.tm_year = currentTime.tm_year;
    timeInfo.tm_mon = month - 1;
    timeInfo.tm_mday = day;
    mktime(&timeInfo);
    int dayOfWeek = timeInfo.tm_wday;

    // Проверяем на выходной и выводим на экран
    if (isWeekend(dayOfWeek) || isHoliday(day, month)) {
        cout << "Сегодня выходной!" << endl;
    }
    else {
        cout << "Сегодня рабочий день." << endl;
    }
}

// Функция чтения всех сотрудников из базы данных
bool ReadAllEmployees(vector<Employee>& employees, sqlite3* db) {
    const char* sql = "SELECT id, name, email, birthday FROM employees;";
    sqlite3_stmt* stmt;
    bool success = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            Employee emp;
            emp.id = sqlite3_column_int(stmt, 0);
            emp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            emp.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            emp.birthday = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
            
            if (!emp.email.empty()) {
                employees.push_back(emp);
                success = true;
            }
        }
        sqlite3_finalize(stmt);
    }
    else {
        cerr << "Ошибка запроса: " << sqlite3_errmsg(db) << endl;
    }

    return success;
}

// Функция отправки email
bool sendEmail(const vector<Employee>& recipients, const vector<Employee>& birthdayEmployees, const string& images_folder) {
    EmailSender SMTP;
    SMTP.SetSettings(
        "b1971ss@mail.ru",          // Логин от Mail.ru
        "pUdQrE3evHbRGNctUwq1",     // Пароль приложения
        "smtp://smtp.mail.ru:587",
        "b1971ss@mail.ru");

    // Для именинников
    for (const auto& emp : birthdayEmployees) {
        string htmlBody = loadTemplate("birthday_template.html", emp.name);
        SMTP.SetSubject("С Днем Рождения!");
        SMTP.SetBody(htmlBody);
        SMTP.SetRecipients({ emp.email });

        // Добавляем фото, если есть
        string image_path = images_folder + "\\" + to_string(emp.id) + ".jpg";
        ifstream file(image_path);
        if (file.good()) {
            SMTP.SetAttachment(image_path);
        }

        cout << "Отправка поздравления на: " << emp.email << " (" << emp.name << ")..." << endl;
        if (!SMTP.sendToAll()) {
            cerr << "Ошибка при отправке на: " << emp.email << endl;
        }
    }

    // Для остальных сотрудников (напоминание поздравить)
    vector<string> other_emails;
    for (const auto& emp : recipients) {
        bool isBirthdayEmployee = false;
        for (const auto& bdEmp : birthdayEmployees) {
            if (emp.email == bdEmp.email) {
                isBirthdayEmployee = true;
                break;
            }
        }
        if (!isBirthdayEmployee) {
            other_emails.push_back(emp.email);
        }
    }

    if (!other_emails.empty()) {
        SMTP.SetSubject("Не забудьте поздравить коллег!");
        SMTP.SetBody("Сегодня день рождения у ваших коллег! Не забудьте их поздравить!");
        SMTP.SetRecipients(other_emails);
        
        cout << "Отправка напоминаний " << other_emails.size() << " сотрудникам..." << endl;
        if (!SMTP.sendToAll()) {
            cerr << "Ошибка при отправке напоминаний" << endl;
        }
    }

    cout << "Рассылка завершена." << endl;
    return true;
}

int main() {
    SetConsoleOutputCP(CP_UTF8); 
    SetConsoleCP(CP_UTF8);

    cout << "Программа запущена." << endl;

    // Открываем базу данных
    sqlite3* db;
    if (sqlite3_open("C:\\Program Files\\SQLiteStudio\\file\\employees.db", &db) != SQLITE_OK) {
        cerr << "Не удалось открыть базу данных: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

    // Проверяем формат дат в БД
    const char* check_sql = "SELECT birthday FROM employees LIMIT 5;";
    sqlite3_stmt* check_stmt;
    if (sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr) == SQLITE_OK) {
        cout << "Проверка формата дат в БД:" << endl;
        while (sqlite3_step(check_stmt) == SQLITE_ROW) {
            const unsigned char* date = sqlite3_column_text(check_stmt, 0);
            cout << " - " << (date ? reinterpret_cast<const char*>(date) : "NULL") << endl;
        }
        sqlite3_finalize(check_stmt);
    }

    // Ищем именинников
    vector<Employee> birthdayEmployees;
    if (!GetBirthdayEmployeesToday(birthdayEmployees, db)) {
        cout << "Ошибка при поиске именинников." << endl;
    }

    if (birthdayEmployees.empty()) {
        cout << "Сегодня нет именинников." << endl;
    }
    else {
        cout << "Найдены именинники: " << endl;
        for (const auto& emp : birthdayEmployees) {
            cout << "  " << emp.id << ": " << emp.name << " (" << emp.email << ")" << endl;
        }
    }

    // Читаем всех сотрудников
    vector<Employee> allEmployees;
    if (!ReadAllEmployees(allEmployees, db)) {
        cerr << "Не удалось прочитать данные сотрудников из базы данных" << endl;
        sqlite3_close(db);
        return 1;
    }

    if (allEmployees.empty()) {
        cout << "Не найдено ни одного сотрудника." << endl;
    }
    else {
        cout << "Всего сотрудников: " << allEmployees.size() << endl;
    }

    // Проверяем какой сегодня день
    checkDay();

    // Отправляем письма
    if (!allEmployees.empty()) {
        string images_folder = "C:\\Users\\Arseniy\\source\\repos\\proektiki\\CombineProject\\photos";
        if (!sendEmail(allEmployees, birthdayEmployees, images_folder)) {
            cerr << "Произошла ошибка при отправке писем" << endl;
        }
    }

    // Закрываем базу данных
    sqlite3_close(db);

    return 0;
}