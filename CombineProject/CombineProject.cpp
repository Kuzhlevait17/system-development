#include <windows.h>
#include <string>
#include <iostream>
#include <sqlite3.h>
#include <vector>
#include <ctime>
#include "SQLC.h"
#include "EmailSMTP.h"

using namespace std;

            // Функция, которая определяет какой сегондя день - выходной(праздничный день) или рабочий.
void checkDay()
{
            // Структура для хранения праздников
    struct Holiday 
    {
        int day;
        int month;
    };

            // Функция для проверки на праздник

    auto isHoliday = [](int day, int month)
        {
            // Объявляем дни, в которые у нас государственные выходные.
            vector<Holiday> holidays = 
            {
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

            // Проверяем дату. Если у нас сегодня праздник, то выводим true.
            for (const auto& holiday : holidays)
            {
                if (holiday.day == day && holiday.month == month)
                {
                    return true;
                }
            }
            return false;
        };

            // Функция для проверки на выходной день
    auto isWeekend = [](int dayOfWeek)
    {
            return (dayOfWeek == 0, dayOfWeek == 6); // Воскресенье - 0, Суббота - 6
    };

            // Получаем текущее время.
    time_t t = time(nullptr);
            // Создаем структуру tm для хранения локального времени
    tm currentTime;

            // Конвертируем time_t в локальное время 
    localtime_s(&currentTime, &t);

            // Извлекаем день и месяц.
    int day = currentTime.tm_mday;
    int month = currentTime.tm_mon + 1;

            // Создаем новую структуру tm и заполням ее.
    tm timeInfo = {};
    timeInfo.tm_year = currentTime.tm_year;
    timeInfo.tm_mon = month - 1;
    timeInfo.tm_mday = day;

            // Нормализуем структуру времени (корректируем недопустимые значения)
            // И вычисляем день недели (tm_wday)
    mktime(&timeInfo);

            // Получаем день недели. 0 - вс, 1 - пн... 6 - сб
    int dayOfWeek = timeInfo.tm_wday;

            // Проверяем на выходной и выводим на экран.
    if (isWeekend(dayOfWeek) || isHoliday(day, month))
    {
        cout << "Сегодня выходной!" << endl;
    }
    else {
        cout << "Сегодня рабочий день." << endl;
    }
}


bool ReadEmails(vector<string>& emails, sqlite3* db)
{
      const char* sql = "SELECT email FROM employees;"; // ?

      // Указатель на подготовленное выражение 
      sqlite3_stmt* stmt;

      bool success = false;

      // Подготавливаем SQL-запрос к выполнению
   
      if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
     {
          // Пошагово выполняем запрос (sqlite3_step)
          // SQLITE_ROW означает, что есть доступная строка данных
         while (sqlite3_step(stmt) == SQLITE_ROW) 
         {
             //Получаем текст из первого стоблца.
            const unsigned char* emailText = sqlite3_column_text(stmt, 0);

            // Если не пустое значение, то добавляем в вектор.
            if (emailText)
            {
                        emails.push_back(reinterpret_cast<const char*>(emailText));
                        success = true;
            }
        }
           // Освобождаем ресурсы на подготовленное выражение
           sqlite3_finalize(stmt);
      }
     else
     {
           cerr << "Ошибка запроса: " << sqlite3_errmsg(db) << endl;
     }
            return success;
}


                // Функция, которая отправляет email-сообщение
bool sendEmail(const vector<string>& emails) 
{
                // Объект SMTP класса EmailSender
    EmailSender SMTP;
                // Устанавливаем соответствующие настройки - Имя пользователя(домена?), 
                // пароль, SMTP-сервер, email пользователя(домена?)
    SMTP.SetSettings("bo5sovb@yandex.com", "ipcftirvudiodaox",
        "smtps://smtp.yandex.ru:465", "bo5sovb@yandex.com");
                // Устанавливаем тему сообщения.
                // Почему-то не работает тема.... :(
                // Потом нужно поменять тему.
    SMTP.SetSubject("Мне грустно...");
               // Объявляем тело сообщения.
               // Потом нужно будет поменять на нормальное сообщение...
    SMTP.SetBody("Никто не пришел на мою фанвстречу в честь моего дня рождения....");
               // Получатели сообщения.
    SMTP.SetRecipients(emails);
                // Если все сообщения отправились, то выводим в буфер, что все хорошо.
    if (SMTP.sendToAll()) 
    {
        cout << "Сообщение успешно отправлено!" << endl;
        return true;
    }
                // Если возникла где-то ошибка, то выводим в буфер, что что-то пошло не так.
    else 
    {
        cerr << "Ошибка при отправке сообщения" << endl;
        return false;
    }
}

int main() {
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);

    cout << "Программа запущена." << endl;

                // Открываем базу данных
    sqlite3* db;
                // База данных, в которой хранится информация о каждом сотруднике компании в виде
                // ID / ИМЯ ФАМИЛИЯ / EMAIL / ДЕНЬ РОЖДЕНИЯ
                // Для тестов нужно менять ссылку соответственно??
    if (sqlite3_open("C:\\Users\\lukuz\\source\\repos\\smtptest\\system-development\\proekt\\proekt\\employees.db", &db) != SQLITE_OK) 
    {
        cerr << "Не удалось открыть базу данных: " << sqlite3_errmsg(db) << endl;
        return 1;
    }

                // Читаем email-адреса
    vector<string> emails;
    ReadEmails(emails, db);

    if (!ReadEmails(emails, db)) 
    {
        cerr << "Не удалось прочитать email-адреса из базы данных" << endl;
        sqlite3_close(db);
        return 1;
    }

    if (emails.empty()) 
    {
        cout << "Не найдено ни одного email-адреса." << endl;
    }
    else
    {
                // Если найдены какие-либо email адреса,то выводим их на экран
                // Возможно это не нужно.
        cout << "Найдено " << emails.size() << " email-адресов:" << endl;
        for (const auto& email : emails) 
        {
            cout << "  " << email << endl;
        }
    }

                // Проверяем какой сегодня день.
    checkDay();

                // Отправляем письма и проверяем, что есть email-адреса и что не произошло ошибок.
    if (!emails.empty()) 
    {
        if (!sendEmail(emails))
        {
            cerr << "Произошла ошибка при отправке писем" << endl;
        }
    }

                // Закрываем базу данных.
    sqlite3_close(db);

    return 0;
}