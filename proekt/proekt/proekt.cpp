//#include <iostream>
//#include <sqlite3.h>
//using namespace std;
//
//int main() {
//    setlocale(LC_ALL, "RU");
//    sqlite3* db; 
//    int exit = sqlite3_open("C:\\Program Files\\SQLiteStudio\\file\\employees.db", &db); // подключение к базе
//
//    if (exit != SQLITE_OK) {
//        cerr << "Ошибка при открытии БД: " << sqlite3_errmsg(db) << endl;
//        return exit;
//    }
//
//    cout << "База данных подключена успешно!" << endl;
//
//    sqlite3_close(db); // закрываем соединение
//    return 0;
//}
#include <iostream>
#include <sqlite3.h>
#include <string>
#include <windows.h>
#include <regex>

using namespace std;

sqlite3* db;  // Указатель на базу данных

// Функция для подключения к базе данных
bool openDatabase() {
    int exit = sqlite3_open("C:\\Program Files\\SQLiteStudio\\file\\employees.db", &db); // Открываем базу данных
    if (exit != SQLITE_OK) {
        cerr << "Ошибка при открытии БД: " << sqlite3_errmsg(db) << endl;
        return false;
    }
    sqlite3_exec(db, "PRAGMA encoding = 'UTF-8';", nullptr, 0, nullptr); // Устанавливаем кодировку
    cout << "База данных подключена успешно!\n";
    return true;
}

// Функция для закрытия соединения с базой данных
void closeDatabase() {
    sqlite3_close(db);
    cout << "База данных закрыта.\n";
}

//Проверка корректности даты рождения 
bool isValidDate(const string& date) {
    regex datePattern(R"(^\d{2}\.\d{2}\.\d{4}$)");
    return regex_match(date, datePattern);
}

// Функция для добавления нового сотрудника
void insertEmployee() {
    string name, email, birthday;
    cout << "Введите имя: ";
    getline(cin, name);
    cout << "Введите email: ";
    getline(cin, email);
    while (true) { // проверка на корректность даты рождения
        cout << "Введите дату рождения (ДД-ММ-ГГГГ): ";
        getline(cin, birthday);
        if (isValidDate(birthday)) {
            break;
        }
        else cout << "Неверный формат даты. Используйте ДД.ММ.ГГГГ.\n";
    }

    string sql = "INSERT INTO Employees (name, email, birthday) VALUES ('" + name + "', '" + email + "', '" + birthday + "');"; //добавление

    char* errorMessage; // указатель на строку, в которую SQLite запишет сообщение о ошибке, если запрос выполнится неудачно
    //если оишбки нет, то этот запрос останется пустым (nullptr)
    int exit = sqlite3_exec(db, sql.c_str(), nullptr, 0, &errorMessage); // выполнение SQL запроса
    //sqlite3_exec() - функция для выполнения SQL-запросов
    //db-указатель на открытую бд;     sql.c_str() - сам SQL-запрос в виде const* char
    //nullptr - функция обратного вызова, она не исп. поэтому nullptr;      0 - доп. данные для callback (не исп)
    //&errorMessage - адрес переменной, куда SQLite запишет сообщение об ошибке если она есть
    //еслизапрос успешен, то sqlite3_exec вернет SQLITE_OK

    if (exit != SQLITE_OK) { //проверка на ошибку
        cerr << "Ошибка при добавлении сотрудника: " << errorMessage << endl; //выведет текст ошибки в консоль
        sqlite3_free(errorMessage); //освободит память, выделенную SQLite для ошибки
    }
    else {
        cout << "Сотрудник добавлен успешно!\n";
    }
}

// Функция-обработчик для вывода данных из SELECT-запросов. SELECT - это команда для получения данных из бд
//void* data - дополнительные данных (обычно нуллптр)
//int argc - количество столбцов в текущей строке
//char** argv - массив значение (результаты запроса)
//char** azColName - массив с именами столбцов
int callback(void* data, int argc, char** argv, char** azColName) {
    for (int i = 0; i < argc; i++) {
        cout << azColName[i] << ": " << (argv[i] ? argv[i] : "NULL") << " | ";
    }
    cout << endl;
    return 0;
}

// Функция для вывода всех сотрудников
void selectAllEmployees() {
    const char* sql = "SELECT * FROM Employees;"; //получение данных из таблицы //* = вывод всех колонок из таблицы
    // можно также const char* sql = "SELECT name, email FROM Employees;"; выведет имя и имейлы без айди и даты др

    char* errorMessage;
    int exit = sqlite3_exec(db, sql, callback, nullptr, &errorMessage);
    //callback - функция обработчик. мы обрабатываем и выводим

    if (exit != SQLITE_OK) {
        cerr << "Ошибка при получении данных: " << errorMessage << endl;
        sqlite3_free(errorMessage);
    }
}

// Функция для обновления email сотрудника по ID
void updateEmployee() {
    int id;
    string newEmail;
    cout << "Введите ID сотрудника, которого нужно изменить: ";
    cin >> id;
    cin.ignore();  // очищаем буфер ввода
    cout << "Введите новый email: ";
    getline(cin, newEmail);

    string sql = "UPDATE Employees SET email = '" + newEmail + "' WHERE id = " + to_string(id) + ";"; //изменение имейла
    
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

// Функция для удаления сотрудника по ID
void deleteEmployee() {
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

// Главная функция программы
int main() {
    setlocale(LC_ALL, "RU");
    SetConsoleOutputCP(65001);
    if (!openDatabase()) return 1;  // Открываем базу данных

    char choice;
    do {
        cout << "\nВыберите действие:\n";
        cout << "1 - Добавить нового сотрудника\n";
        cout << "2 - Показать всех сотрудников\n";
        cout << "3 - Изменить email сотрудника\n";
        cout << "4 - Удалить сотрудника\n";
        cout << "0 - Выход\n";
        cout << "Ваш выбор: ";
        cin >> choice;
        cin.ignore();  // Очищаем буфер ввода

        switch (choice) {
        case '1':
            insertEmployee();
            break;
        case '2':
            selectAllEmployees();
            break;
        case '3':
            updateEmployee();
            break;
        case '4':
            deleteEmployee();
            break;
        case '0':
            cout << "Выход из программы.\n";
            break;
        default:
            cout << "Некорректный ввод, попробуйте снова.\n";
        }
    } while (choice != '0');

    closeDatabase();  // Закрываем базу перед выходом
    return 0;
}