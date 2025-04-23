#include "EmailSMTP.h"
#include <curl/curl.h>
#include <sqlite3.h>
#include <iostream>
#include <cstring>

                    // Конструктор по умолчанию
EmailSender::EmailSender()
{
                    // Первая инициализация, готовимся к работе с libcurl.
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

                    // Деструктор по умолчанию
EmailSender::~EmailSender()
{
    curl_global_cleanup();
}

                    // Сеттер, в который ставим все настройки для отправителя emailа
void EmailSender::SetSettings(const string& username, const string& password, const string& smtp_server,const string& mail_from)
{
    username_ = username;
    password_ = password;
    smtp_server_ = smtp_server;
    mail_from_ = mail_from;
}

                    // Сеттер, который устанавливает тему сообщения.
void EmailSender::SetSubject(const string& subject)
{
    subject_ = subject;
}

                    // Сеттер, который устанавливает тело сообщения.
void EmailSender::SetBody(const string& body)
{
    body_ = body;
}

                    // Сеттер, который устанавливает получателей.
void EmailSender::SetRecipients(const vector<string>& recipients)
{
    recipients_ = recipients;
}

                    // Добавляем в recipients все emailы
void EmailSender::AddRecipient(const string& email)
{
    recipients_.push_back(email);
}


                    // Вычисляем и сохраняем длину строки
EmailSender::ReadData::ReadData(const char* str)
    : source(str),
    size(str ? strlen(str) : 0)
{
}

// Функция, которая используется для чтения данных из источника и передачи их в буфер
size_t EmailSender::read_function(char* buffer, size_t size, size_t nitems, ReadData* data)
{

  //       вычисляем общий размер данных для чтения.
  //       size - размер одного элемента данных (в байтах)
  //       nitems - количество данных (в байтах) которое нужно посчитать  

    size_t len = size * nitems;
    if (len > data->size) 
    {
        // проверяем, что не вышли за пределы доступных данных
        len = data->size;
    }
    if (len > 0)
    {
        memcpy(buffer, data->source, len); // Копируем данные в буфер обмена.
        data->source += len;               // Обновляем указатель и размер данных
        data->size -= len;
    }
    return len;
}


// Функция, которая отправляет email-сообщения
bool EmailSender::sendToAll()
{
                    // Проверяем, что пароль и имя пользователя установлены.
    if (username_.empty() || password_.empty()) 
    {
        cerr << "Пароль и имя пользователя для отправки сообщений не заполнены." << endl;
        return false;
    }

                    // Проверяем, что smtp сервер установлен.
    if (smtp_server_.empty()) 
    {
        cerr << "Сервер SMTP не определен." << endl;
        return false;
    }
                
                    // Проверяем, что есть получатели сообщений.
    if (recipients_.empty())
    {
        cerr << "Получатели письма не установлены." << endl;
        return false;
    }

                    // Провреяем, что есть тема письма.
                    // Почему не работает то....
    if (subject_.empty())
    {
        cerr << "Тема письма не объявлена." << endl;
        return false;
    }
    
                    // Проверяем, что есть тело письма.
    if (body_.empty())
    {
        cerr << "Тело письма не объявлено." << endl;
        return false;
    }

                    // Инициализируем curl сессию.
    CURL* curl = curl_easy_init();
    if (!curl) 
    {
        cerr << "Ошибка при работе curl" << endl;
        return false;
    }

                    // Устанавливаем параметры подключения: Имя 
                    // пользователя, пароль, SMTP сервер, отправитель.
    curl_easy_setopt(curl, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, smtp_server_.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mail_from_.c_str());

                    // Формируем список получателей
    struct curl_slist* recipients = nullptr;
                    // Заполняем recipients emailами из вектора.
    for (const auto& email : recipients_) 
    {
        recipients = curl_slist_append(recipients, email.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    string email_text =
        "From: " + mail_from_ + "\r\n" +
        "To: " + recipients_[0] + "\r\n" +        // Первый получатель в поле "To" ?? Поменять надо как-то... :(
        "Subject: " + subject_ + "\r\n" +
        "\r\n" +                                  // Пустая строка разделяет заголовки и тело
        body_ + "\r\n";

                                                  // Собираем текст для отправки сообщения на почту
    ReadData data(email_text.c_str());
    curl_easy_setopt(curl, CURLOPT_READDATA, &data);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_function);

                                                 // Настройки SMTP
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Включаем отладку.
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
                                                 // Проверяем сертефикат.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                                                 // Проверяем имя хоста.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

                                                // Отправка письма
    CURLcode res = curl_easy_perform(curl);

    // Очистка ресурсов
    curl_slist_free_all(recipients);
    curl_easy_cleanup(curl);
        
                                                // Если получилось что-то не так, то выводим ошибку.
    if (res != CURLE_OK) 
    {
        cerr << "Не удалось отправить сообщение: " << curl_easy_strerror(res) << endl;
        return false;
    }
                                                // Иначе выводим скольким сотрудникам пришло сообщение.
    cout << "Email успешно были отправлены " << recipients_.size() << " получателям" << endl;
    return true;
}
