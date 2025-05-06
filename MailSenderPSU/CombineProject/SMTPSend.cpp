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

using namespace std;

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

// Сеттер настроек
void EmailSender::SetSettings(const string& username, const string& password, 
                            const string& smtp_server, const string& mail_from)
{
    username_ = username;
    password_ = password;
    smtp_server_ = smtp_server;
    mail_from_ = mail_from;
}

// Сеттер темы сообщения
void EmailSender::SetSubject(const string& subject)
{
    subject_ = subject;
}

// Сеттер тела сообщения
void EmailSender::SetBody(const string& body)
{
    body_ = body;
}

// Сеттер получателей
void EmailSender::SetRecipients(const vector<string>& recipients)
{
    recipients_ = recipients;
}

// Добавление получателя
void EmailSender::AddRecipient(const string& email)
{
    recipients_.push_back(email);
}

// Установка вложения
void EmailSender::SetAttachment(const string& file_path) {
    attachment_path_ = file_path;
}

// Кодирование файла в base64 (не используется в текущей реализации)
string EmailSender::base64_encode_file(const string& file_path) {
    ifstream file(file_path, ios::binary);
    if (!file) {
        cerr << "Не удалось открыть файл для кодирования: " << file_path << endl;
        return "";
    }

    string content((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());

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

// Структура для чтения данных
EmailSender::ReadData::ReadData(const char* str)
    : source(str),
    size(str ? strlen(str) : 0)
{
}

// Функция чтения данных для curl
size_t EmailSender::read_function(char* buffer, size_t size, size_t nitems, ReadData* data)
{
    size_t len = size * nitems;
    if (len > data->size) {
        len = data->size;
    }
    if (len > 0) {
        memcpy(buffer, data->source, len);
        data->source += len;
        data->size -= len;
    }
    return len;
}

// Основная функция отправки email
bool EmailSender::sendToAll()
{
    // Проверка обязательных полей
    if (username_.empty() || password_.empty()) {
        cerr << "Пароль и имя пользователя для отправки сообщений не заполнены." << endl;
        return false;
    }

    if (smtp_server_.empty()) {
        cerr << "Сервер SMTP не определен." << endl;
        return false;
    }

    if (recipients_.empty()) {
        cerr << "Получатели письма не установлены." << endl;
        return false;
    }

    if (subject_.empty()) {
        cerr << "Тема письма не объявлена." << endl;
        return false;
    }

    if (body_.empty()) {
        cerr << "Тело письма не объявлено." << endl;
        return false;
    }

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

    // Часть с текстом сообщения (HTML)
    curl_mimepart* text_part = curl_mime_addpart(mime);
    curl_mime_data(text_part, body_.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_type(text_part, "text/html; charset=UTF-8");

    // Добавляем вложение, если оно есть
    if (!attachment_path_.empty()) {
        FILE* file = nullptr;
        errno_t err = fopen_s(&file, attachment_path_.c_str(), "rb");
        if (err != 0 || !file) {
            cerr << "Не удалось открыть файл вложения: " << attachment_path_ << endl;
            curl_mime_free(mime);
            curl_slist_free_all(recipients);
            curl_easy_cleanup(curl);
            return false;
        }

        // Получаем имя файла
        size_t last_slash = attachment_path_.find_last_of("\\/");
        string filename = (last_slash != string::npos) ?
            attachment_path_.substr(last_slash + 1) :
            attachment_path_;

        // Получаем расширение файла
        size_t last_dot = attachment_path_.find_last_of('.');
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
        curl_mime_filename(attach_part, filename.c_str());
        curl_mime_encoder(attach_part, "base64");

        // Для изображений можно добавить Content-ID для встраивания в HTML
        if (content_type.find("image/") != string::npos) {
            string content_id = "<" + filename + ">";
            curl_mime_headers(attach_part, nullptr, 1);
            curl_mime_name(attach_part, content_id.c_str());
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
    if (file) fclose(file);
    curl_slist_free_all(recipients);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        cerr << "Не удалось отправить сообщение: " << curl_easy_strerror(res) << endl;
        return false;
    }

    cout << "Email успешно отправлены " << recipients_.size() << " получателям" << endl;
    return true;
}