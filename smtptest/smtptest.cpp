#define USERNAME "bo5sovb@yandex.com"
#define PASSWORD "ipcftirvudiodaox"
#define MAILTO "wylsomerice@gmail.com"
#define MAILFROM "bo5sovb@yandex.com"
#define SMTP "smtps://smtp.yandex.ru:465"

#include <stdio.h>
#include <curl/curl.h>

// создаем тестовый** текст*** сообщения, который будем отправлять пользователю*
// *далее необходимо работать с SQLite
// **далее необходим html-шаблон письма
// *** русский язык поддерживается
const char* payload_text =
"To: " MAILTO "\r\n"
"From: " MAILFROM "\r\n"
"Subject: Привет,ТЕМА\r\n"
"\r\n"                                                // перевод курсива на начало новой строки

"Приглашаю всех на свой день рождения!\r\n";

// структура ReadData, которая хранит данные, 
// которые могут быть использованы для чтения и/или передачи ее данных в программе.
struct ReadData
{                                                     // explicit - ключевое слово, которые предотвращает неявное преобразование типов
    explicit ReadData(const char* str)
    {
        source = str;
        size = strlen(str);
    }

    const char* source;
    size_t size;
};

// функция, которая используется для чтения данных из источника и передачи их в буфер
size_t read_function(char* buffer, size_t size, size_t nitems, ReadData* data)
{
    // вычисляем общий размер данных для чтения.
    // size - размер одного элемента данных (в байтах)
    // nitems - количество данных (в байтах) которое нужно посчитать                                             
    size_t len = size * nitems;
    if (len > data->size)
    {
        // проверяем, что не вышли за пределы доступных данных
        len = data->size;
    }
    memcpy(buffer, data->source, len);              // копируем данные в буфер обмена
    data->source += len;                            // обновлеяем указатель и размер данных
    data->size -= len;
    return len;
}

int main()
{
    CURL* curl = curl_easy_init();
    // если не удалось инициализировать сессию, то выводится ошибка.
    if (!curl)
    {

        fprintf(stderr, "curl_easy_init failed\n");
        return 1;
    }
    // устанавливает параметры username, password, url для curl
    curl_easy_setopt(curl, CURLOPT_USERNAME, USERNAME);
    curl_easy_setopt(curl, CURLOPT_PASSWORD, PASSWORD);
    curl_easy_setopt(curl, CURLOPT_URL, SMTP);
    curl_easy_setopt(curl, CURLOPT_MAIL_FROM, MAILFROM);
    // адрес отправителя
    // создаем односвязный список получателей (и заголовков)
    struct curl_slist* rcpt = NULL;               // добавляем строку (email пользователя) в список
    rcpt = curl_slist_append(rcpt, MAILTO);
    // получаем список получателей почты.
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