#pragma execution_character_set("utf-8")

#include <iostream>
#include <Windows.h>

#include <cstring>
#include <fstream>
#include <ctime>
#include <sstream>
#include <regex>
#include <string>
#include <vector>
#include <algorithm>
#include <map>


#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <curl/curl.h>
#include "sqlite3.h"

#define LOG(msg) Logger::Log(msg)

using namespace std;


class Logger {
public:
    static void Log(const std::string& message) {
        // Получаем текущее время
        time_t now = time(nullptr);
        tm local_time;
        localtime_s(&local_time, &now);
        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &local_time);

        // Финальное сообщение
        std::string full_message = "[" + std::string(time_buf) + "] " + message;

        // Выводим в консоль
        std::cout << full_message << std::endl;

        // Пишем в файл
        std::ofstream log_file("log.txt", std::ios::app);
        if (log_file.is_open()) {
            log_file << full_message << std::endl;
        }
    }
};


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

    void SetAttachment(const string& file_path); // Установка пути к вложению

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
    static size_t read_file_callback(void* ptr, size_t size, size_t nmemb, FILE* stream); // Функция чтения файла
    static string base64_encode_file(const string& file_path); // Кодирование файла в Base64


    string username_;               // Имя отправителя.
    string password_;               // Пароль отправителя.
    string smtp_server_;            // SMTP-сервер.
    string mail_from_;              // Email отправителя.
    string subject_;                // Заголовок письма.
    string body_;                   // Тело письма
    vector<string> recipients_;     // Вектор, содержащий всех получателей сообщения.
    vector<string> attachment_paths_;
    // Путь к файлу вложения
};


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
void EmailSender::SetAttachment(const string& file_path)
{
    attachment_paths_.push_back(file_path);
}

// Класс конфигурации
class ConfigReader {
private:
    map<string, string> config_;

    void Trim(string& str) {
        str.erase(str.begin(), find_if(str.begin(), str.end(), [](int ch) {
            return !std::isspace(ch);
            }));
        str.erase(find_if(str.rbegin(), str.rend(), [](int ch) {
            return !isspace(ch);
            }).base(), str.end());
        if (!str.empty() && str.front() == '"') str.erase(0, 1);
        if (!str.empty() && str.back() == '"') str.erase(str.size() - 1, 1);
    }

public:
    bool Load(const string& filename) {
        ifstream file(filename);


        if (!file) {
            LOG("Ошибка открытия конфига: " + string(filename));
            return false;
        }
        else {
            LOG("конфиг: " + string(filename) + " успешно открыт");
        }


        string line;
        while (getline(file, line)) {
            Trim(line);
            if (line.empty() || line[0] == '#') continue;

            size_t pos = line.find('=');
            if (pos != string::npos) {
                string key = line.substr(0, pos);
                string value = line.substr(pos + 1);
                Trim(key);
                Trim(value);
                config_[key] = value;
            }
        }
        return true;
    }

    string GetString(const string& key, const string& def = "") {
        auto it = config_.find(key);
        return it != config_.end() ? it->second : def;
    }

    int GetInt(const string& key, int def = 0) {
        auto it = config_.find(key);
        if (it != config_.end()) {
            try { return stoi(it->second); }
            catch (...) { return def; }
        }
        return def;
    }

    bool GetBool(const string& key, bool def = false) {
        auto it = config_.find(key);
        if (it != config_.end()) {
            string val = it->second;
            transform(val.begin(), val.end(), val.begin(), ::tolower);
            return val == "true" || val == "1" || val == "yes";
        }
        return def;
    }
};




// Кодирование файла в base64 (не используется в текущей реализации)
string EmailSender::base64_encode_file(const string& file_path)
{
    ifstream file(file_path, ios::binary);
    if (!file) {
        LOG("Не удалось открыть файл для кодирования: " + string(file_path));
        return "";
    }
    else {
        LOG("файл для кодирования: " + string(file_path) + " успешно открыт");
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
size_t EmailSender::read_file_callback(void* ptr, size_t size, size_t nmemb, FILE* stream)
{
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
    if (len > data->size)
    {
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
    if (username_.empty() || password_.empty())
    {
        LOG("Пароль и имя пользователя для отправки сообщений не заполнены.");
        return false;
    }
    else {
        LOG("Пароль и имя пользователя для отправки сообщений успешно заполнены");
    }


    if (smtp_server_.empty())
    {
        LOG("Сервер SMTP не определен.");
        return false;
    }
    else {
        LOG("Сервер SMTP определен.");
    }



    if (recipients_.empty())
    {
        LOG("Получатели письма не установлены.");
    }
    else {
        LOG("Получатели письма установлены.");
    }


    if (subject_.empty())
    {
        LOG("Тема письма не объявлена.");
        return false;
    }
    else {
        LOG("Тема письма объявлена.");
    }



    if (body_.empty())
    {
        LOG("Тело письма не объявлено.");
        return false;
    }
    else {
        LOG("Тело письма объявлено.");
    }


    CURL* curl = curl_easy_init();
    if (!curl)
    {
        LOG("Ошибка при работе cur"); 

        return false;
    }
    else {
        LOG("Нет ошибки при работе cur");
    }
    // Устанавливаем параметры подключения
    curl_easy_setopt(curl, CURLOPT_USERNAME, username_.c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, password_.c_str());
    curl_easy_setopt(curl, CURLOPT_URL, smtp_server_.c_str());
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, mail_from_.c_str());

    // Формируем список получателей
    struct curl_slist* recipients = nullptr;
    for (const auto& email : recipients_)
    {
        recipients = curl_slist_append(recipients, email.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

    // Создаем MIME сообщение
    curl_mime* mime = curl_mime_init(curl);

    // Часть для заголовка Subject
    struct curl_slist* headers = nullptr;
    string subject_header = "Subject: " + subject_;
    headers = curl_slist_append(headers, subject_header.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Часть с текстом сообщения (HTML)
    curl_mimepart* text_part = curl_mime_addpart(mime);
    curl_mime_data(text_part, body_.c_str(), CURL_ZERO_TERMINATED);
    curl_mime_type(text_part, "text/html; charset=UTF-8");

    // Добавляем вложение, если оно есть
    for (const auto& path : attachment_paths_) {
        FILE* file = nullptr;
        errno_t err = fopen_s(&file, path.c_str(), "rb");
        if (err != 0 || !file) {
            LOG("Не удалось открыть файл вложения:" + string(path));

            continue;
        }
        else {
            LOG("файл вложения: " + string(path) + " успешно открыт");
        }

        size_t last_slash = path.find_last_of("\\/");
        string filename = (last_slash != string::npos) ?
            path.substr(last_slash + 1) : path;

        size_t last_dot = path.find_last_of('.');
        string extension = (last_dot != string::npos && last_dot > last_slash) ?
            path.substr(last_dot + 1) : "";

        string content_type = "application/octet-stream";
        transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

        if (extension == "jpg" || extension == "jpeg") content_type = "image/jpeg";
        else if (extension == "png") content_type = "image/png";
        else if (extension == "gif") content_type = "image/gif";
        else if (extension == "pdf") content_type = "application/pdf";

        curl_mimepart* attach_part = curl_mime_addpart(mime);
        curl_mime_filedata(attach_part, path.c_str());
        curl_mime_type(attach_part, content_type.c_str());
        curl_mime_filename(attach_part, filename.c_str());
        curl_mime_encoder(attach_part, "base64");

        if (content_type.find("image/") != string::npos) {
            string content_id = "<" + filename + ">";
            curl_mime_headers(attach_part, nullptr, 1);
            curl_mime_name(attach_part, content_id.c_str());
        }

        if (file) fclose(file);
    }


    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // Настройки SMTP
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); // Включить отладку - меняем 0 на 1.
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    // Отправка письма
    CURLcode res = curl_easy_perform(curl);

    // Очистка ресурсов

    curl_slist_free_all(recipients);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);


    LOG("Email успешно отправлены " + to_string(recipients_.size()) + " получателям" );
    return true;
}

using namespace std;

// Структура для хранения данных сотрудника
struct Employee
{
    int id;
    string name;
    string email;
    string birthday;
};

// Функция генерации сообщений
string GenerateTemplate(ConfigReader& configReader,
    const string& key,
    const vector<Employee>& employees,
    const string& date_override = "")
{
    string template_str = configReader.GetString(key);
    if (template_str.empty()) {
        return "";
    }

    vector<string> names;
    for (const auto& emp : employees) {
        names.push_back(emp.name);
    }

    string joined_names;
    if (names.size() == 1) {
        joined_names = names[0];
    }
    else {
        for (size_t i = 0; i < names.size(); ++i) {
            if (i > 0) joined_names += (i == names.size() - 1) ? " и " : ", ";
            joined_names += names[i];
        }
    }

    string date = date_override.empty() ? employees[0].birthday.substr(0, 5) : date_override;

    size_t pos;
    while ((pos = template_str.find("{names}")) != string::npos) {
        template_str.replace(pos, 7, joined_names);
    }

    while ((pos = template_str.find("{date}")) != string::npos) {
        template_str.replace(pos, 6, date);
    }

    return template_str;
}

// Функция нахождения именниников
bool GetBirthdayEmployees(vector<Employee>& birthdayEmployees, sqlite3* db)
{
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    char today_dd_mm[6];
    snprintf(today_dd_mm, sizeof(today_dd_mm), "%02d.%02d", localTime.tm_mday, localTime.tm_mon + 1);
    LOG("Ищем сотрудников с днём рождения:  " + string(today_dd_mm) ); 

    const char* sql = "SELECT id, name, email, birthday FROM employees WHERE birthday LIKE ? || '%';";
    sqlite3_stmt* stmt;


    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        LOG("Ошибка подготовки запроса: " + string(sqlite3_errmsg(db))); 
        return false;
    }
    else {
        LOG("Нет ошибки подготовки запроса: " + string(sqlite3_errmsg(db)));
    }


    if (sqlite3_bind_text(stmt, 1, today_dd_mm, 5, SQLITE_STATIC) != SQLITE_OK)
    {
        LOG("Ошибка привязки параметра: " + string(sqlite3_errmsg(db)));
        sqlite3_finalize(stmt);
        return false;
    }
    else {
        LOG("Нет шибки привязки параметра: " + string(sqlite3_errmsg(db)));
    }


    bool found = false;
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        const unsigned char* name = sqlite3_column_text(stmt, 1);
        const unsigned char* email = sqlite3_column_text(stmt, 2);
        const unsigned char* birthday = sqlite3_column_text(stmt, 3);

        if (email)
        {
            Employee emp;
            emp.id = sqlite3_column_int(stmt, 0);
            emp.name = name ? reinterpret_cast<const char*>(name) : "";
            emp.email = reinterpret_cast<const char*>(email);
            emp.birthday = birthday ? reinterpret_cast<const char*>(birthday) : "";

            birthdayEmployees.push_back(emp);
            found = true;
        }
    }

    sqlite3_finalize(stmt);
    return found;
}


// Callback-функция для записи ответа
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output)
{
    size_t total_size = size * nmemb;
    output->append((char*)contents, total_size);
    return total_size;
}

// Функция проверки, является ли день выходным
bool IsDayOff(int day, int month, int year)
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        LOG("Ошибка инициализации CURL ");
        return false;
    }
    else
    {
        LOG("Нет ошибки инициализации CURL ");
    }
    std::string url = "https://isdayoff.ru/api/getdata?year=" + to_string(year) +
        "&month=" + to_string(month) +
        "&day=" + to_string(day);

    string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);

    //// Выводим полученный ответ для отладки
    //cout << "Ответ от API: " << response << endl;


    // Ответ "1" — выходной, "0" — рабочий день
    return (response == "1");
}

// Функция чтения всех сотрудников из базы данных
bool ReadAllEmployees(vector<Employee>& employees, sqlite3* db)
{
    const char* sql = "SELECT id, name, email, birthday FROM employees;";
    sqlite3_stmt* stmt;
    bool success = false;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            Employee emp;
            emp.id = sqlite3_column_int(stmt, 0);
            emp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            emp.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            emp.birthday = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

            if (!emp.email.empty())
            {
                employees.push_back(emp);
                success = true;
            }
        }
        sqlite3_finalize(stmt);
        LOG("Нет ошибки запроса: " + string(sqlite3_errmsg(db)));
    }
    else {
        LOG("Ошибка запроса: " + string(sqlite3_errmsg(db)));
    }
  
    return success;
}

// Функция для обновления поля last_congratulated в БД
void UpdateLastCongratulated(sqlite3* db, int employee_id)
{
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    char buffer[11];  // "DD.MM.YYYY" = 10 символов + 1 для \0
    strftime(buffer, sizeof(buffer), "%d.%m.%Y", &localTime);  // Изменено на формат "DD.MM.YYYY"
    string currentDate(buffer);

    // Подготовка SQL-запроса для обновления last_congratulated
    string sql = "UPDATE employees SET last_congratulated = ? WHERE id = ?";
    char* errorMessage = nullptr;

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK)
    {
        LOG("Ошибка при подготовке SQL запроса:" + string(sqlite3_errmsg(db)));
        return;
    }
    else
    {
        LOG("Нет ошибки при подготовке SQL запроса:" + string(sqlite3_errmsg(db)));
    }

    sqlite3_bind_text(stmt, 1, currentDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, employee_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        LOG("Ошибка при обновлении last_congratulated: " + string(sqlite3_errmsg(db))); 
    }
    else
    {
        LOG("Поле last_congratulated успешно обновлено для сотрудника " + to_string(employee_id));
    }

    sqlite3_finalize(stmt);  // Освобождаем ресурсы запроса
}

// Функция которая определяет какой сегодня рабочий день(первый или нет)
bool IsFirstWorkingDayAfterBreak()
{
    time_t now = time(0);
    tm today_tm;
    localtime_s(&today_tm, &now);

    int today_d = today_tm.tm_mday;
    int today_m = today_tm.tm_mon + 1;
    int today_y = today_tm.tm_year + 1900;

    bool todayIsDayOff = IsDayOff(today_d, today_m, today_y);
    LOG("Сегодня: " + to_string(today_d) + "." + to_string(today_m) + "." + to_string(today_y) +
        " -> " + (todayIsDayOff ? "Нерабочий день" : "Рабочий день"));

    if (!todayIsDayOff)
    {
        // Вчера
        time_t yesterday = now - 86400;
        tm yest_tm;
        localtime_s(&yest_tm, &yesterday);

        int yd = yest_tm.tm_mday;
        int ym = yest_tm.tm_mon + 1;
        int yy = yest_tm.tm_year + 1900;

        bool yesterdayIsDayOff = IsDayOff(yd, ym, yy);
        LOG("Вчера: " + to_string(yd) + "." + to_string(ym) + "." + to_string(yy) + " -> " + (yesterdayIsDayOff ? "Нерабочий день" : "Рабочий день"));

        if (yesterdayIsDayOff)
        {
            return true;
        }
    }

    return false;
}

// Функция отправки в рабочий день
bool sendEmail(const vector<Employee>& all_employees,
    const vector<Employee>& birthday_employees,
    const string& images_folder, const string& smtp_username, const string& smtp_password, const string& smtp_server, const string& mail_from,
    sqlite3* db,
    ConfigReader& configReader)
{
    // Получаем текущую дату
    time_t now = time(nullptr);
    tm tm_now;
    localtime_s(&tm_now, &now);
    char current_date[6];
    strftime(current_date, sizeof(current_date), "%d.%m", &tm_now);

    // Настройка SMTP
    EmailSender sender;
    sender.SetSettings(smtp_username, smtp_password, smtp_server, mail_from);

    // Отправка уведомлений всем сотрудникам
    if (!birthday_employees.empty()) {
        vector<string> all_emails;
        for (const auto& emp : all_employees)
        {
            all_emails.push_back(emp.email);
        }
        vector<string> names;
        for (const auto& emp : birthday_employees) {
            names.push_back(emp.name);
        }
        string template_key = (birthday_employees.size() == 1) ? "today_one" : "today_many";
        string reminder_body = GenerateTemplate(configReader, template_key, birthday_employees, current_date); // Передаём список сотрудников

        if (!reminder_body.empty() && !all_emails.empty())
        {
            sender.SetSubject(configReader.GetString("message_subject"));
            sender.SetBody(reminder_body);
            sender.SetRecipients(all_emails);

            // Прикрепляем фото всех именинников
            for (const auto& emp : birthday_employees) 
            {
                string photo_path = images_folder + "\\" + to_string(emp.id) + ".jpg";
                if (ifstream(photo_path).good())
                {
                    sender.SetAttachment(photo_path);
                }
            }

            if (sender.sendToAll())
            {
                LOG("Уведомления отправлены всем сотрудникам: " + to_string(all_emails.size()) + " адресов."); 
            }
            else
            {
                LOG("Ошибка отправки уведомлений ");
            }
        }
    }

    return true;
}

// Функция отправки сообщений за выходные
bool SendLate(const vector<Employee>& all_employees,
    const vector<Employee>& missed_employees,
    const string& images_folder, const string& smtp_username, const string& smtp_password, const string& smtp_server, const string& mail_from,
    sqlite3* db,
    ConfigReader& configReader) {
    EmailSender sender;
    sender.SetSettings(smtp_username, smtp_password, smtp_server, mail_from);

    if (!missed_employees.empty())
    {
        vector<string> all_emails;
        for (const auto& emp : all_employees) 
        {
            all_emails.push_back(emp.email);
        }

        string template_key = (missed_employees.size() == 1) ? "weekend_one" : "weekend_many";
        string reminder_body = GenerateTemplate(configReader, template_key, missed_employees);

        if (!reminder_body.empty() && !all_emails.empty())
        {
            sender.SetSubject(configReader.GetString("message_subject"));
            sender.SetBody(reminder_body);
            sender.SetRecipients(all_emails);

            // Отладочный вывод
            LOG("Папка с изображениями: " + string(images_folder)); 

            for (const auto& emp : missed_employees) 
            {
                string photo_path = images_folder + "\\" + to_string(emp.id) + ".jpg";
                LOG("Проверка файла: " + string(photo_path)); 

                FILE* file = nullptr;
                errno_t err = fopen_s(&file, photo_path.c_str(), "rb");
                if (err == 0 && file != nullptr) 
                {
                    fclose(file);
                    sender.SetAttachment(photo_path);
                    LOG("Файл прикреплён: " + string(photo_path)); 
                }
                else {
                    LOG("Ошибка открытия файла:  " + string(photo_path) + " код ошибки: " + to_string(err)); 
                }
            }

            if (sender.sendToAll()) 
            {
                LOG("Уведомления отправлены."); 
            }
            else
            {
                LOG("Ошибка отправки. " );
            }
        }
    }
    return true;
}



int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8); // для корректного ввода, если нужно
    LOG(" ");
    LOG("Программа запущена");

    // Загружаем конфигурацию
    ConfigReader config;
    if (!config.Load("config.ini")) {
        LOG("Не удалось загрузить конфигурационный файл config.ini");
        return 1;
    }
    else {
        LOG("конфигурационный файл config.ini успешно загружен.");
    }
    
    if (config.GetString("db_path").empty()) {
        LOG("Ошибка: Параметр 'db_path' не найден в конфигурации"); 
        return 1;
    }
    else {
        LOG("Параметр 'db_path' успешно найден в конфигурации");
    }

    // 4. Открытие БД
    sqlite3* db;
    if (sqlite3_open( config.GetString("db_path").c_str(), &db) != SQLITE_OK) {
        LOG("Не удалось открыть базу данных: " + string ( sqlite3_errmsg(db)));
        return 1;
    }
    else {
        LOG("база данных: " + string(sqlite3_errmsg(db)) + " успешно открыта.");
    }

    // Проверка и добавление поля last_congratulated
    bool columnExists = false;
    const char* pragma_sql = "PRAGMA table_info(employees);";
    sqlite3_stmt* pragma_stmt;
    if (sqlite3_prepare_v2(db, pragma_sql, -1, &pragma_stmt, nullptr) == SQLITE_OK)
    {
        while (sqlite3_step(pragma_stmt) == SQLITE_ROW)
        {
            const unsigned char* colName = sqlite3_column_text(pragma_stmt, 1);
            if (colName && strcmp(reinterpret_cast<const char*>(colName), "last_congratulated") == 0) {
                columnExists = true;
                break;
            }
        }
    }
    sqlite3_finalize(pragma_stmt);

    if (!columnExists)
    {
        LOG("Поле 'last_congratulated' не найдено. Добавляем..."); 
        const char* alter_sql = "ALTER TABLE employees ADD COLUMN last_congratulated TEXT;";
        char* errMsg = nullptr;
        if (sqlite3_exec(db, alter_sql, nullptr, nullptr, &errMsg) != SQLITE_OK)
        {
            LOG("Ошибка при добавлении поля: " + string(errMsg));
            sqlite3_free(errMsg);
        }
        else
        {
            LOG("Поле добавлено успешно."); 
        }
    }

    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);
    LOG("Текущая дата: " + to_string(localTime.tm_mday) + "." + to_string ( localTime.tm_mon + 1) + "." + to_string ( localTime.tm_year + 1900));
    int day = localTime.tm_mday;
    int month = localTime.tm_mon + 1;
    int year = localTime.tm_year + 1900;
    bool RestDay = IsDayOff(day, month, year);
    vector<Employee> DayOffEmployees;
    vector<Employee> birthdayEmployees;

    if (RestDay)
    {
        LOG("Сегодня выходной или праздник.");
 
        if (DayOffEmployees.empty())
        {
            LOG("Именинников в нерабочие дни не было."); 
        }

        else
        {
            LOG("Найдено " + to_string(DayOffEmployees.size()) + " именинник(ов) в нерабочий день." );
        }
    }
    else
    {
        // Читаем всех сотрудников
        vector<Employee> allEmployees;
        if (!ReadAllEmployees(allEmployees, db))
        {
            LOG("Не удалось прочитать сотрудников из базы."); 
            sqlite3_close(db);
            return 1;
        }
        else {
            LOG("Успешно удалось прочитать сотрудников из базы.");
        }

        if (IsFirstWorkingDayAfterBreak())
        {
            LOG("Первый рабочий день после выходных. Ищем именинников за выходные..."); 

            time_t cursor = now - 86400;
            while (true)
            {
                tm tm_cursor;
                localtime_s(&tm_cursor, &cursor);

                int d = tm_cursor.tm_mday;
                int m = tm_cursor.tm_mon + 1;
                int y = tm_cursor.tm_year + 1900;

                if (!IsDayOff(d, m, y)) break;

                char dd_mm[6];
                snprintf(dd_mm, sizeof(dd_mm), "%02d.%02d", d, m);

                const char* sql = "SELECT id, name, email, birthday FROM employees WHERE birthday LIKE ? || '%';";
                sqlite3_stmt* stmt;

                if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK &&
                    sqlite3_bind_text(stmt, 1, dd_mm, -1, SQLITE_TRANSIENT) == SQLITE_OK)
                {
                    while (sqlite3_step(stmt) == SQLITE_ROW)
                    {
                        Employee emp;
                        emp.id = sqlite3_column_int(stmt, 0);
                        emp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
                        emp.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
                        emp.birthday = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));
                        if (!emp.email.empty()) 
                        {
                            DayOffEmployees.push_back(emp);
                        }
                    }
                    sqlite3_finalize(stmt);
                }

                cursor -= 86400;
            }

            if (!DayOffEmployees.empty()) 
            {
                LOG("Найдено" + to_string(DayOffEmployees.size()) + " именинников за выходные."); 
                string images_folder = config.GetString("photo_path").c_str();
                if (!SendLate(allEmployees, DayOffEmployees, images_folder, config.GetString("smtp_username"), config.GetString("smtp_password"), \
                    config.GetString("smtp_server"), config.GetString("mail_from"), db, config))
                {
                    LOG("Ошибка при отправке поздравлений за выходные."); 
                }
                else {
                    LOG("Нет ошибок при отправке поздравлений за выходные.");
                }
            }
            else
            {
                LOG("Именинников за выходные не найдено."); 
            }

            if (birthdayEmployees.empty()) 
            {
                LOG("Сегодня нет именинников."); 
            }
            else {
                LOG("Найдены именинники : ");
                for (const auto& emp : birthdayEmployees)
                {
                    cout << "  " << emp.id << ": " << emp.name << " (" << emp.email << ")" << endl;
                }
            }


            if (!allEmployees.empty())
            {
                string images_folder = config.GetString("photo_path").c_str();
                if (!sendEmail(allEmployees, birthdayEmployees, images_folder, config.GetString("smtp_username"), config.GetString("smtp_password"), \
                    config.GetString("smtp_server"), config.GetString("mail_from"), db, config))
                {
                    LOG("Произошла ошибка при отправке писем"); 
                }
                else {
                    LOG("Нет ошибок при отправке писем");
                }
            }

        }
        else
        {
            //// Сегодня обычный рабочий день, проверим именинников
           if (birthdayEmployees.empty())
            {
                LOG("Сегодня нет именинников."); 
            }
            else
            {
                LOG("Найдены именинники: "); 
                for (const auto& emp : birthdayEmployees)
                {
                    cout << "  " << emp.id << ": " << emp.name << " (" << emp.email << ")" << endl;
                }
            }


            if (!allEmployees.empty())
            {
                string images_folder = config.GetString("photo_path").c_str();
                if (!sendEmail(allEmployees, birthdayEmployees, images_folder, config.GetString("smtp_username"), config.GetString("smtp_password"), \
                    config.GetString("smtp_server"), config.GetString("mail_from"), db, config))
                {
                    LOG("Произошла ошибка при отправке писем."); 
                }
                else {
                    LOG("Нет ошибок при отправке писем");
                }
            }
        }
    }

    sqlite3_close(db);
    LOG("Программа завершена.");
    LOG(" ");
    return 0;
    

}
