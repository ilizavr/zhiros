//this utility depent on my PC. since it  here's the code that prayer.sh invokes:
/*
package main

import (
	"context"
	"fmt"
	"github.com/PuerkitoBio/goquery"
	"log"
	"net/http"
	"time"
)

func main() {
	url := "https://мусульмане33.рф/"

	client := &http.Client{}
	ctx, canselreq := context.WithTimeout(context.Background(), 7 * time.Second)
	defer canselreq()
	request, err := http.NewRequestWithContext(ctx, "GET", url, nil)
	if err != nil {
		log.Fatalf("Ошибка создания запроса: %s", err)
	}

	request.Header.Set("Host", "мусульмане33.рф") // Важно: Убираем https://  Это имя хоста.
	request.Header.Set("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36")
	request.Header.Set("Accept", "text/html")
	request.Header.Set("Connection", "close")

	resp, err := client.Do(request)
	if err != nil {
		log.Fatalf("Ошибка выполнения запроса: %s", err)
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		log.Fatalf("Ошибка: Код статуса %d", resp.StatusCode)
	}

	doc, err := goquery.NewDocumentFromReader(resp.Body)
	if err != nil {
		log.Fatalf("Ошибка при парсинге HTML: %s", err)
	}

	//  Выбор div с классом "town"
	townDiv := doc.Find("div.topbarschedule")

	// Проверка, найден ли элемент
	if townDiv.Length() > 0 {
		siteName := townDiv.Text() // Получаем текст из div.town
		fmt.Println(siteName)      // Выводим текст
	} else {
		fmt.Println("Не найден div с классом 'town'")
	}


}
*/

#include<stdlib.h>
#include<stdio.h>

#define EXIT_SUCCSESS 0
#define EXIT_FAILURE 1
#define YOU_ARE_IDIOT 1111
int main()
{
int code = system("bash /home/ruslan/prayer.sh");

switch(code)
{
    case 0:
    exit(EXIT_SUCCSESS);
    case 1:
    exit(EXIT_FAILURE);
    default:
    exit(YOU_ARE_IDIOT);
}
}
