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

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <curl/curl.h>
#include <sqlite3.h>

using namespace std;

// Класс EmailSender, который отправляет email-сообщения из БД.
class EmailSender
{
public:
    EmailSender();
    ~EmailSender();

    void SetSettings(const string& username, const string& password,
        const string& smtp_server, const string& mail_from);
    bool sendToAll();
    void SetSubject(const string& subject);
    void SetBody(const string& body);
    void SetRecipients(const vector<string>& recipients);
    void AddRecipient(const string& email);
    void SetAttachment(const string& file_path); // Установка пути к вложению

private:
    struct ReadData
    {
        explicit ReadData(const char* str);
        const char* source;
        size_t size;
    };

    static size_t read_function(char* buffer, size_t size, size_t nitems, ReadData* data);
    static size_t read_file_callback(void* ptr, size_t size, size_t nmemb, FILE* stream); 
    static string base64_encode_file(const string& file_path); 


    string username_;               // Имя отправителя.
    string password_;               // Пароль отправителя.
    string smtp_server_;            // SMTP-сервер.
    string mail_from_;              // Email отправителя.
    string subject_;                // Заголовок письма.
    string body_;                   // Тело письма
    vector<string> recipients_;     // Вектор, содержащий всех получателей сообщения.
    string attachment_path_; // Путь к файлу вложения
};


EmailSender::EmailSender()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}
EmailSender::~EmailSender()
{
    curl_global_cleanup();
}
void EmailSender::SetSettings(const string& username, const string& password,
    const string& smtp_server, const string& mail_from)
{
    username_ = username;
    password_ = password;
    smtp_server_ = smtp_server;
    mail_from_ = mail_from;
}
void EmailSender::SetSubject(const string& subject)
{
    subject_ = subject;
}
void EmailSender::SetBody(const string& body)
{
    body_ = body;
}
void EmailSender::SetRecipients(const vector<string>& recipients)
{
    recipients_ = recipients;
}
void EmailSender::AddRecipient(const string& email)
{
    recipients_.push_back(email);
}

void EmailSender::SetAttachment(const string& file_path)
{
    attachment_path_ = file_path;
}

// Кодирование файла в base64 (не используется в текущей реализации)
string EmailSender::base64_encode_file(const string& file_path) 
{
    ifstream file(file_path, ios::binary);
    if (!file)
    {
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
    if (len > 0) 
    {
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
        cerr << "Пароль и имя пользователя для отправки сообщений не заполнены." << endl;
        return false;
    }
    if (smtp_server_.empty()) 
    {
        cerr << "Сервер SMTP не определен." << endl;
        return false;
    }
    if (recipients_.empty()) 
    {
        cerr << "Получатели письма не установлены." << endl;
        return false;
    }
    if (subject_.empty())
    {
        cerr << "Тема письма не объявлена." << endl;
        return false;
    }
    if (body_.empty()) 
    {
        cerr << "Тело письма не объявлено." << endl;
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl) 
    {
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
    if (!attachment_path_.empty())
    {
        FILE* file = nullptr;
        errno_t err = fopen_s(&file, attachment_path_.c_str(), "rb");
        if (err != 0 || !file) 
        {
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
        if (content_type.find("image/") != string::npos)
        {
            string content_id = "<" + filename + ">";
            curl_mime_headers(attach_part, nullptr, 1);
            curl_mime_name(attach_part, content_id.c_str());
        }
        if (file) fclose(file);
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
    curl_slist_free_all(headers);

    if (res != CURLE_OK) 
    {
        cerr << "Не удалось отправить сообщение: " << curl_easy_strerror(res) << endl;
        return false;
    }

    cout << "Email успешно отправлены " << recipients_.size() << " получателям" << endl;
    return true;
}

// Структура для хранения данных сотрудника
struct Employee 
{
    int id;
    string name;
    string email;
    string birthday;
};

// Функция загрузки шаблона письма
string loadTemplate(const string& filename, const string& fullName) 
{
    ifstream file(filename);
    if (!file.is_open())
    {
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
        static_cast<unsigned char>(content[2]) == 0xBF) 
    {
        content = content.substr(3);
    }

    // Не работает, нужно починить.
    content = regex_replace(content, regex("\\{фио сотрудника\\}"), fullName);
    return content;
}

// Функция поиска именинников
bool GetBirthdayEmployeesToday(vector<Employee>& birthdayEmployees, sqlite3* db) 
{
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
    while (sqlite3_step(stmt) == SQLITE_ROW)
    {
        Employee emp;
        emp.id = sqlite3_column_int(stmt, 0);
        emp.name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        emp.email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        emp.birthday = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3));

        if (!emp.email.empty()) 
        {
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
        cerr << "Ошибка инициализации CURL" << endl;
        return false;
    }

    string url = "https://isdayoff.ru/api/getdata?year=" + to_string(year) +
        "&month=" + to_string(month) +
        "&day=" + to_string(day);

    string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) 
    {
        cerr << "Ошибка CURL: " << curl_easy_strerror(res) << endl;
        curl_easy_cleanup(curl);
        return false;
    }


    curl_easy_cleanup(curl);
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
    }
    else
    {
        cerr << "Ошибка запроса: " << sqlite3_errmsg(db) << endl;
    }

    return success;
}

// Функция для обновления поля last_congratulated в БД
void UpdateLastCongratulated(sqlite3* db, int employee_id)
{
    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    char buffer[11]; 
    strftime(buffer, sizeof(buffer), "%d.%m.%Y", &localTime); 
    string currentDate(buffer);

    string sql = "UPDATE employees SET last_congratulated = ? WHERE id = ?";
    char* errorMessage = nullptr;

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) 
    {
        cerr << "Ошибка при подготовке SQL запроса: " << sqlite3_errmsg(db) << endl;
        return;
    }

    sqlite3_bind_text(stmt, 1, currentDate.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, employee_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) 
    {
        cerr << "Ошибка при обновлении last_congratulated: " << sqlite3_errmsg(db) << endl;
    }
    else 
    {
        cout << "Поле last_congratulated успешно обновлено для сотрудника " << employee_id << endl;
    }

    sqlite3_finalize(stmt);  
}

// Функция, проверяющая, что сегодня первый рабочий день.
bool IsFirstWorkingDayAfterBreak()
{
    time_t now = time(0);
    tm today_tm;
    localtime_s(&today_tm, &now);

    int today_d = today_tm.tm_mday;
    int today_m = today_tm.tm_mon + 1;
    int today_y = today_tm.tm_year + 1900;

    bool todayIsDayOff = IsDayOff(today_d, today_m, today_y);
    std::cout << "Сегодня: " << today_d << "." << today_m << "." << today_y << " -> "
        << (todayIsDayOff ? "Нерабочий день" : "Рабочий день") << std::endl;

    if (!todayIsDayOff) {
        // Вчера
        time_t yesterday = now - 86400;
        tm yest_tm;
        localtime_s(&yest_tm, &yesterday);

        int yd = yest_tm.tm_mday;
        int ym = yest_tm.tm_mon + 1;
        int yy = yest_tm.tm_year + 1900;

        bool yesterdayIsDayOff = IsDayOff(yd, ym, yy);
        std::cout << "Вчера: " << yd << "." << ym << "." << yy << " -> "
            << (yesterdayIsDayOff ? "Нерабочий день" : "Рабочий день") << std::endl;

        if (yesterdayIsDayOff) {
            return true;
        }
    }
}

// Функция отправки email (Дни рождения в рабочие дни)
bool sendEmail(const vector<Employee>& recipients, const vector<Employee>& birthdayEmployees, const string& images_folder, sqlite3* db)
{
    EmailSender SMTP;
    SMTP.SetSettings(
        "b1971ss@mail.ru",          // Логин от Mail.ru
        "pUdQrE3evHbRGNctUwq1",     // Пароль приложения
        "smtp://smtp.mail.ru:587",
        "b1971ss@mail.ru");

    // Для именинников
    for (const auto& emp : birthdayEmployees) 
    {
        string htmlBody = loadTemplate("birthday_template.html", emp.name);
        SMTP.SetSubject("С Днем Рождения!");
        SMTP.SetBody(htmlBody);
        SMTP.SetRecipients({ emp.email });

        // Добавляем фото, если есть
        string image_path = images_folder + "\\" + to_string(emp.id) + ".jpg";
        ifstream file(image_path);
        if (file.good()) 
        {
            SMTP.SetAttachment(image_path);
        }

        cout << "Отправка поздравления на: " << emp.email << " (" << emp.name << ")..." << endl;
        if (!SMTP.sendToAll())
        {
            cerr << "Ошибка при отправке на: " << emp.email << endl;
        }

        UpdateLastCongratulated(db, emp.id);
    }

    // Для остальных сотрудников (напоминание поздравить)
    vector<string> other_emails;
    for (const auto& emp : recipients) 
    {
        bool isBirthdayEmployee = false;
        for (const auto& bdEmp : birthdayEmployees) 
        {
            if (emp.email == bdEmp.email) 
            {
                isBirthdayEmployee = true;
                break;
            }
        }
        if (!isBirthdayEmployee) 
        {
            other_emails.push_back(emp.email);
        }
    }

    if (!other_emails.empty()) 
    {
        SMTP.SetSubject("Не забудьте поздравить коллег!");
        SMTP.SetBody("Сегодня день рождения у ваших коллег! Не забудьте их поздравить!");
        SMTP.SetRecipients(other_emails);

        cout << "Отправка напоминаний " << other_emails.size() << " сотрудникам..." << endl;
        if (!SMTP.sendToAll()) 
        {
            cerr << "Ошибка при отправке напоминаний" << endl;
        }

       
    }
    
    cout << "Рассылка завершена." << endl;
    return true;
}

// Функция отправки сообщений (Работает ТОЛЬКО в первый рабочий день, отправляет сообщения ЕСЛИ у кого-то день рождения был в нерабочий день).
bool SendLate(const vector<Employee>& recipients, const vector<Employee>& DayOffEmployees,  const string& images_folder, sqlite3* db)
{
    EmailSender SMTP2;
    SMTP2.SetSettings(
        "b1971ss@mail.ru",          // Логин от Mail.ru
        "pUdQrE3evHbRGNctUwq1",     // Пароль приложения
        "smtp://smtp.mail.ru:587",
        "b1971ss@mail.ru");

    // Для именинников, которые праздновали в выходной день
    for (const auto& emp : DayOffEmployees)
    {
       // string htmlBody = loadTemplate("birthday_template.html", emp.name);
        SMTP2.SetSubject("С Прошедшим Рождения!");
        SMTP2.SetBody("Поздравляем вас с прошедшим днем рождения!");
        SMTP2.SetRecipients({ emp.email });

        // Добавляем фото, если есть
        string image_path = images_folder + "\\" + to_string(emp.id) + ".jpg";
        ifstream file(image_path);
        if (file.good())
        {
            SMTP2.SetAttachment(image_path);
        }

        cout << "Отправка поздравления на: " << emp.email << " (" << emp.name << ")..." << endl;
        if (!SMTP2.sendToAll()) 
        {
            cerr << "Ошибка при отправке на: " << emp.email << endl;
        }

        UpdateLastCongratulated(db, emp.id);  // Обновляем дату последнего поздравления
    }

    // Для остальных сотрудников (напоминание поздравить)
    vector<string> other_emails;
    for (const auto& emp : recipients)
    {
        bool isBirthdayEmployee = false;
        for (const auto& bdEmp : DayOffEmployees)
        {
            if (emp.email == bdEmp.email)
            {
                isBirthdayEmployee = true;
                break;
            }
        }
        if (!isBirthdayEmployee) {
            other_emails.push_back(emp.email);
        }
    }

    if (!other_emails.empty())
    {
        SMTP2.SetSubject("Не забудьте поздравить коллег!");
        SMTP2.SetBody("В прошедшие нерабочие дни был день рождения у ваших коллег! Не забудьте их поздравить!");
        SMTP2.SetRecipients(other_emails);

        cout << "Отправка напоминаний " << other_emails.size() << " сотрудникам..." << endl;
        if (!SMTP2.sendToAll())
        {
            cerr << "Ошибка при отправке напоминаний" << endl;
        }
    }

    cout << "Рассылка завершена." << endl << endl << endl << endl;
    return true;

}

int main() 
{
    SetConsoleOutputCP(1251);
    SetConsoleCP(1251);

    cout << "Программа запущена." << endl;

    // Открываем базу данных
    sqlite3* db;
    if (sqlite3_open("C:\\Users\\lukuz\\Downloads\\Telegram Desktop\\employees (2).db", &db) != SQLITE_OK) 
    {
        cerr << "Не удалось открыть базу данных: " << sqlite3_errmsg(db) << endl;
        return 1;
    }
    // Проверяем формат дат в БД
    const char* check_sql = "SELECT birthday FROM employees LIMIT 5;";
    sqlite3_stmt* check_stmt;
    if (sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr) == SQLITE_OK) 
    {
        cout << "Проверка формата дат в БД:" << endl;
        while (sqlite3_step(check_stmt) == SQLITE_ROW) 
        {
            const unsigned char* date = sqlite3_column_text(check_stmt, 0);
            cout << " - " << (date ? reinterpret_cast<const char*>(date) : "NULL") << endl;
        }
        sqlite3_finalize(check_stmt);
    }

    time_t now = time(nullptr);
    tm localTime;
    localtime_s(&localTime, &now);

    cout << "Текущая дата: " << localTime.tm_mday << "."
        << localTime.tm_mon + 1 << "." << localTime.tm_year + 1900 << endl;

    int day = localTime.tm_mday;       
    int month = localTime.tm_mon + 1; 
    int year = localTime.tm_year + 1900;
    bool RestDay = IsDayOff(day, month, year);
    vector <Employee> DayOffEmployees;
 
    if (RestDay)
    {
        cout << "Сегодня выходной или праздник." << endl;
  
        // Просто выводим что у кого-то был день рождения в нерабочий день.
        if (!GetBirthdayEmployeesToday(DayOffEmployees, db))
        {
            cout << "Ошибка при поиске именниников в нерабочий день." << endl;
        }
        if (DayOffEmployees.empty())
        {
            cout << "Именниников в нерабочие дни не было.";
        }
        else
        {
            cout << "Найдено " << DayOffEmployees.size() << " именинник(ов) в нерабочий день. " << endl;
        }
    }
    else
    {
            // Читаем всех сотрудников
            vector<Employee> allEmployees;
            if (!ReadAllEmployees(allEmployees, db))
            {
                cerr << "Не удалось прочитать данные сотрудников из базы данных" << endl;
                sqlite3_close(db);
                return 1;
            }

            if (allEmployees.empty())
            {
                cout << "Не найдено ни одного сотрудника." << endl;
            }
            else
            {
                cout << "Всего сотрудников: " << allEmployees.size() << endl;
            }

            if (IsFirstWorkingDayAfterBreak())
            {
                cout << "Сегодня первый рабочий день после выходных. Ищем именинников за выходные..." << endl;

                time_t now = time(nullptr);
                time_t cursor = now - 86400; // начинаем с вчера

                while (true)
                {
                    tm tm_cursor;
                    localtime_s(&tm_cursor, &cursor);

                    int d = tm_cursor.tm_mday;
                    int m = tm_cursor.tm_mon + 1;
                    int y = tm_cursor.tm_year + 1900;

                    if (!IsDayOff(d, m, y))
                        break;

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

                    cursor -= 86400; // предыдущий день
                }

                if (!DayOffEmployees.empty())
                {
                    cout << "Найдено " << DayOffEmployees.size() << " именинников за нерабочие дни:" << endl;
                    for (const auto& emp : DayOffEmployees)
                    {
                        cout << " " << emp.id << ": " << emp.name << " (" << emp.email << " )" << endl;
                    }

                    string images_folder = "C:\\Users\\lukuz\\OneDrive\\Desktop\\Combine2\\CombineProject\\photos";
                    if (!SendLate(allEmployees, DayOffEmployees, images_folder, db))
                    {
                        cerr << "Произошла ошибка при отправке писем" << endl;
                    }
                }
                else
                {
                    cout << "За нерабочие дни именинников не было." << endl;
                }

                // Ищем именинников дальше
                vector<Employee> birthdayEmployees;
                if (!GetBirthdayEmployeesToday(birthdayEmployees, db))
                {
                    cout << "Ошибка при поиске именинников." << endl;
                }

                if (birthdayEmployees.empty())
                {
                    cout << "Сегодня нет именинников." << endl;
                }
                else
                {
                    cout << "Найдены именинники: " << endl;
                    for (const auto& emp : birthdayEmployees)
                    {
                        cout << "  " << emp.id << ": " << emp.name << " (" << emp.email << ")" << endl;
                    }
                }

                if (!allEmployees.empty())
                {
                    string images_folder = "C:\\Users\\lukuz\\OneDrive\\Desktop\\Combine2\\CombineProject\\photos";
                    if (!sendEmail(allEmployees, birthdayEmployees, images_folder, db))
                    {
                        cerr << "Произошла ошибка при отправке писем" << endl;
                    }
                }
            }
            else
            {
                // Ищем именинников если рабочий день не первый
                vector<Employee> birthdayEmployees;
                if (!GetBirthdayEmployeesToday(birthdayEmployees, db))
                {
                    cout << "Ошибка при поиске именинников." << endl;
                }

                if (birthdayEmployees.empty())
                {
                    cout << "Сегодня нет именинников." << endl;
                }
                else
                {
                    cout << "Найдены именинники: " << endl;
                    for (const auto& emp : birthdayEmployees)
                    {
                        cout << "  " << emp.id << ": " << emp.name << " (" << emp.email << ")" << endl;
                    }
                }

                if (!allEmployees.empty())
                {
                    string images_folder = "C:\\Users\\lukuz\\OneDrive\\Desktop\\Combine2\\CombineProject\\photos";
                    if (!sendEmail(allEmployees, birthdayEmployees, images_folder, db))
                    {
                        cerr << "Произошла ошибка при отправке писем" << endl;
                    }
                }
            }
            // Закрываем базу данных
            sqlite3_close(db);
        }

    return 0;
}