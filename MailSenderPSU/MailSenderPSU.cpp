#include <iostream>
#include <sqlite3.h>
#include <string>
#include <windows.h>
#include <regex>

#define USERNAME "bo5sovb@yandex.com"
#define PASSWORD "ipcftirvudiodaox"
#define MAILTO "wylsomerice@gmail.com"
#define MAILFROM "bo5sovb@yandex.com"
#define SMTP "smtps://smtp.yandex.ru:465"

#include <stdio.h>
#include <curl/curl.h>
#include <vcruntime_string.h>

using namespace std;

sqlite3* db;  

const char* payload_text =
"To: " MAILTO "\r\n"
"From: " MAILFROM "\r\n"
"Subject: Привет,ТЕМА\r\n"
"\r\n"                                                
"Приглашаю всех на свой день рождения!\r\n"; //тут шаблонный текст и фото
                                             //фото берётся из папки, имя для поздравления из базы данных (SELECT name FROM Employees WHERE birthday == @сегодня@) 

static bool openConnection() {
    int exit = sqlite3_open("..\\employees.db", &db); //Не используйте абсолютный путь, .db-файл лежит в директории проекта
    if (exit != SQLITE_OK) {
        cerr << "Ошибка при открытии БД: " << sqlite3_errmsg(db) << endl;
        return false;
    }
    sqlite3_exec(db, "PRAGMA encoding = 'UTF-8';", nullptr, 0, nullptr); 
    cout << "База данных подключена успешно!\n";
    return true;
}

static void closeConnection() {
    sqlite3_close(db);
    cout << "База данных закрыта.\n";
}

static bool isValidDate(const string& date) {
    regex datePattern(R"(^\d{2}\.\d{2}\.\d{4}$)");
    return regex_match(date, datePattern);
}

//функционал редактирования бд не нужен, программа только рассылает письма и не требует никакого ввода во время работы
static void insertEmployee() {
    string name, email, birthday;
    cout << "Введите имя: ";
    getline(cin, name);
    cout << "Введите email: ";
    getline(cin, email);
    while (true) {
        cout << "Введите дату рождения (ДД-ММ-ГГГГ): ";
        getline(cin, birthday);
        if (isValidDate(birthday)) {
            break;
        }
        else cout << "Неверный формат даты. Используйте ДД.ММ.ГГГГ.\n";
    }

    string sql = "INSERT INTO Employees (name, email, birthday) VALUES ('" + name + "', '" + email + "', '" + birthday + "');"; //добавление

    char* errorMessage; 
    int exit = sqlite3_exec(db, sql.c_str(), nullptr, 0, &errorMessage); 

    if (exit != SQLITE_OK) { 
        cerr << "Ошибка при добавлении сотрудника: " << errorMessage << endl; 
        sqlite3_free(errorMessage); 
    }
    else {
        cout << "Сотрудник добавлен успешно!\n";
    }
}

static int callback(void* data, int argc, char** argv, char** azColName) {
    for (int i = 0; i < argc; i++) {
        cout << azColName[i] << ": " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    cout << endl;
    return 0;
}

static void selectAllEmployees() {
    const char* sql = "SELECT * FROM Employees;"; 

    char* errorMessage;
    int exit = sqlite3_exec(db, sql, callback, nullptr, &errorMessage);

    if (exit != SQLITE_OK) {
        cerr << "Ошибка при получении данных: " << errorMessage << endl;
        sqlite3_free(errorMessage);
    }
}

static void updateEmployee() {
    int id;
    string newEmail;
    cout << "Введите ID сотрудника, которого нужно изменить: ";
    cin >> id;
    cin.ignore();  
    cout << "Введите новый email: ";
    getline(cin, newEmail);

    string sql = "UPDATE Employees SET email = '" + newEmail + "' WHERE id = " + to_string(id) + ";"; 

    char* errorMessage;
    int exit = sqlite3_exec(db, sql.c_str(), nullptr, 0, &errorMessage);

    if (exit != SQLITE_OK) {
        cerr << "Ошибка при обновлении данных: " << errorMessage << endl;
        sqlite3_free(errorMessage);
    }
    else {
        cout << "Данные сотрудника обновлены!\n";
    }
}

static void deleteEmployee() {
    int id;
    cout << "Введите ID сотрудника, которого нужно удалить: ";
    cin >> id;
    cin.ignore();

    string sql = "DELETE FROM Employees WHERE id = " + to_string(id) + ";"; //удаление сотрудника по айди

    char* errorMessage;
    int exit = sqlite3_exec(db, sql.c_str(), nullptr, 0, &errorMessage);

    if (exit != SQLITE_OK) {
        cerr << "Ошибка при удалении сотрудника: " << errorMessage << endl;
        sqlite3_free(errorMessage);
    }
    else {
        cout << "Сотрудник удалён!\n";
    }
}

struct ReadData
{                                                     
    explicit ReadData(const char* str)
    {
        source = str;
        size = strlen(str);
    }

    const char* source;
    size_t size;
};

static size_t read_function(char* buffer, size_t size, size_t nitems, ReadData* data)
{                                          
    size_t len = size * nitems;
    if (len > data->size)
    {
        len = data->size;
    }
    memcpy(buffer, data->source, len);              
    data->source += len;                           
    data->size -= len;
    return len;
}



int main()
{
    // в самом начале работы программы проверяем, есть ли в бд 
    // сотрудники с сегодняшней датой в поле birthday, если нет, 
    // заканчиваем работу, если да, берём его имя и фото, 
    // формируем письмо и рассылаем всем сотрудникам в бд кроме именинника
    // (получаем массив строк с адресами почт сотрудников и отправляем всем по очереди одинаковое письмо)
    

    CURL* curl = curl_easy_init();
    if (!curl)
    {

        fprintf(stderr, "curl_easy_init failed\n");
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_USERNAME, USERNAME);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, PASSWORD);
    curl_easy_setopt(curl, CURLOPT_URL, SMTP);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, MAILFROM);

    struct curl_slist* rcpt = NULL;               
    rcpt = curl_slist_append(rcpt, MAILTO);

    curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, rcpt);

    // собираем текст для отправки сообщения на почту
    ReadData data(payload_text);
    curl_easy_setopt(curl, CURLOPT_READDATA, &data);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, read_function);

    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);   // включаем отладку
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);    // запрос загрузки файла на сервер

    // используем SSL (у нас 465 порт, который поддерживает
    // протокол TLS); далее небоходимо будет отслеживать,
    // чтобы сообщение не попало в спам
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);

    // пока что нет SSL сертификата?...
    // ТОЛЬКО для тестирования; далее нужен будет сертификат?
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);

    CURLcode res = curl_easy_perform(curl);     // выполяем smtp-запрос
    if (res != CURLE_OK)
    {
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
            curl_easy_strerror(res));           // если где-то будет ошибка, то её выводим на экран консоли
        curl_easy_cleanup(curl);                // освобождаем ресурсы
    }
    return 0;
}

// Главная функция программы
//int main() {
//    setlocale(LC_ALL, "RU");
//    SetConsoleOutputCP(65001);
//    if (!openDatabase()) return 1;  // Открываем базу данных
//
//    char choice;
//    do {
//        cout << "\nВыберите действие:\n";
//        cout << "1 - Добавить нового сотрудника\n";
//        cout << "2 - Показать всех сотрудников\n";
//        cout << "3 - Изменить email сотрудника\n";
//        cout << "4 - Удалить сотрудника\n";
//        cout << "0 - Выход\n";
//        cout << "Ваш выбор: ";
//        cin >> choice;
//        cin.ignore();  // Очищаем буфер ввода
//
//        switch (choice) {
//        case '1':
//            insertEmployee();
//            break;
//        case '2':
//            selectAllEmployees();
//            break;
//        case '3':
//            updateEmployee();
//            break;
//        case '4':
//            deleteEmployee();
//            break;
//        case '0':
//            cout << "Выход из программы.\n";
//            break;
//        default:
//            cout << "Некорректный ввод, попробуйте снова.\n";
//        }
//    } while (choice != '0');
//
//    closeDatabase();  // Закрываем базу перед выходом
//    return 0;
//}