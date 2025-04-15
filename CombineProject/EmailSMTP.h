#pragma once

#ifndef EmailSMTP_H
#define EmailSMTP_H

#include <string>
#include <vector>
#include <curl/curl.h>
#include <sqlite3.h>

using namespace std;

             // Класс EmailSender, который отправляет email-сообщения из БД.
class EmailSender 
{
public:
             // Конструктор по умолчанию
             // (гарантируем, что поля класса будут иметь определенные значения).
             // (инициализируем объект класса во время его создания).
    EmailSender();
             // Деструктор по умолчанию
             // (освобождение использованных ресурсов и удаление нестатических переменных).
    ~EmailSender();

             // Сеттер, содержащий имя пользователя, пароль, SMTP сервер и email отправителя.
    void SetSettings(const string& username, const string& password,
        const string& smtp_server, const string& mail_from);

             // Функция, которая отправляет сообщения пользователям.
    bool sendToAll();

             // Сеттер, который устанавливает тему сообщения.
    void SetSubject(const string& subject);

             // Сеттер, который устанавливает тело сообщения.
    void SetBody(const string& body);

             // Сеттер, который устанавливает получателей сообщений.
    void SetRecipients(const vector<string>& recipients);

             // Функция, которая добавляет получателей сообщения.
    void AddRecipient(const string& email);

private:

             // Структура, хранящая данные, которые могут быть использованы для
             // чтения/передачи данных в программе.
    struct ReadData
    {
             // Объявляем комплилятору, что оператор преобразования
             // используется только в явном виде.
        explicit ReadData(const char* str);
        const char* source;
        size_t size;
    };

             // Функция для чтения данных и передачи их в буфер(Используется для отладки)?
    static size_t read_function(char* buffer, size_t size, size_t nitems, ReadData* data);



    string username_;               // Имя отправителя.
    string password_;               // Пароль отправителя.
    string smtp_server_;            // SMTP-сервер.
    string mail_from_;              // Email отправителя.
    string subject_;                // Заголовок письма.
    string body_;                   // Тело письма
    vector<string> recipients_;     // Вектор, содержащий всех получателей сообщения.
};

#endif // EMAILSMTP_H