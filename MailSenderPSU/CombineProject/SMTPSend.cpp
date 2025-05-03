#include "EmailSMTP.h"
#include <curl/curl.h>
#include <sqlite3.h>
#include <iostream>
#include <cstring>
#include <fstream>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <algorithm>

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

void EmailSender::SetAttachment(const string& file_path) {
    attachment_path_ = file_path;
}

string EmailSender::base64_encode_file(const string& file_path) {
    ifstream file(file_path, ios::binary);
    if (!file) {
        cerr << "Не удалось открыть файл для кодирования: " << file_path << endl;
        return "";
    }

    // Читаем весь файл
    string content((istreambuf_iterator<char>(file)),
        istreambuf_iterator<char>());

    BIO* bio, * b64;
    BUF_MEM* bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(bio, content.c_str(), content.length());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    return result;
}

// Функция для чтения файла (используется при отправке вложения)
size_t EmailSender::read_file_callback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    size_t retcode = fread(ptr, size, nmemb, stream);
    return retcode;
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

    // Проверка существования файла вложения
    if (attachment_path_.empty()) {
        cerr << "Путь к вложению не указан." << endl;
        return false;
    }

    FILE* file = nullptr;
    errno_t err = fopen_s(&file, attachment_path_.c_str(), "rb");
    if (err != 0 || !file) {
        cerr << "Не удалось открыть файл вложения: " << attachment_path_ << endl;
        return false;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    CURL* curl = curl_easy_init();
    if (!curl) {
        cerr << "Ошибка при работе curl" << endl;
        return false;
    }

    // Устанавливаем параметры подключения
    curl_easy_setopt(curl, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, smtp_server_.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mail_from_.c_str());

    // Формируем список получателей
    struct curl_slist* recipients = nullptr;
    for (const auto& email : recipients_) {
        recipients = curl_slist_append(recipients, email.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    // Создаем MIME сообщение
    curl_mime* mime = curl_mime_init(curl);

    // Часть с текстом сообщения
    curl_mimepart* part = curl_mime_addpart(mime);
    curl_mime_data(part, body_.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_type(part, "text/plain");

    // Добавляем вложение, если оно есть
    if (!attachment_path_.empty()) {
        // Получаем имя файла и расширение
        size_t last_slash = attachment_path_.find_last_of("\\/");
        size_t last_dot = attachment_path_.find_last_of('.');

        string filename = (last_slash != string::npos) ?
            attachment_path_.substr(last_slash + 1) :
            attachment_path_;

        string extension = (last_dot != string::npos && last_dot > last_slash) ?
            attachment_path_.substr(last_dot + 1) : "";

        // Определяем Content-Type по расширению
        string content_type = "application/octet-stream";
        transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        if (extension == "jpg" || extension == "jpeg") content_type = "image/jpeg";
        else if (extension == "png") content_type = "image/png";
        else if (extension == "gif") content_type = "image/gif";
        else if (extension == "pdf") content_type = "application/pdf";

        // Добавляем часть с вложением
        curl_mimepart* attach_part = curl_mime_addpart(mime);
        curl_mime_filedata(attach_part, attachment_path_.c_str());
        curl_mime_type(attach_part, content_type.c_str());
        curl_mime_name(attach_part, filename.c_str());
        curl_mime_encoder(attach_part, "base64"); // Пусть Curl сам кодирует в Base64

        // Для изображений можно добавить Content-ID для встраивания в HTML
        if (content_type.find("image/") != string::npos) {
            string content_id = "<" + filename + ">";
            curl_mime_headers(attach_part, NULL, 1); // Очищаем старые заголовки
            curl_mime_name(attach_part, NULL); // Убираем имя, если нужно встроенное изображение
            curl_mime_data_cb(attach_part, -1,
                [](char* buffer, size_t size, size_t nitems, void* arg) -> size_t {
                    FILE* file = static_cast<FILE*>(arg);
                    if (!file) return CURL_READFUNC_ABORT;
                    return fread(buffer, size, nitems, file);
                },
                NULL,
                [](void* arg) {
                    if (arg) {
                        FILE* file = static_cast<FILE*>(arg);
                        fclose(file);
                    }
                },
                file);
        }
    }
    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // Настройки SMTP
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L); // Включаем отладку
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Отправка письма
    CURLcode res = curl_easy_perform(curl);

    // Очистка ресурсов
    curl_slist_free_all(recipients);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "Не удалось отправить сообщение: " << curl_easy_strerror(res) << endl;
        return false;
    }

    cout << "Email успешно были отправлены " << recipients_.size() << " получателям" << endl;
    return true;
}
