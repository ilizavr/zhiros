package main

import (
    "fmt"
    "net"
    "time"
)

func main() {
    target := "192.168.1.7" // Замените на нужный IP
    count := 10000000
    timeout := time.Second * 1

    fmt.Printf("Отправка %d ICMP-запросов на %s...\n", count, target)

    success := 0
    fail := 0

    for i := 0; i < count; i++ {
        err := sendICMP(target, timeout)
        if err != nil {
            fmt.Printf("Запрос %d: ошибка отправки - %v\n", i+1, err)
            fail++
        } else {
            fmt.Printf("Запрос %d: отправлен\n", i+1)
            success++
        }
    }

    fmt.Printf("\nИтог:\n")
    fmt.Printf("Успешные отправки: %d\n", success)
    fmt.Printf("Неудачные отправки: %d\n", fail)
    fmt.Printf("Успешность: %.2f%%\n", float64(success)/float64(count)*100)
}

// Функция только отправляет ICMP-запрос (без ожидания ответа)
func sendICMP(ip string, timeout time.Duration) error {
    conn, err := net.DialTimeout("ip4:icmp", ip, timeout)
    if err != nil {
        return err
    }
    defer conn.Close()

    // Формируем ICMP-сообщение (Echo Request)
    msg := make([]byte, 8)
    msg[0] = 8  // Тип: Echo Request (8)
    msg[1] = 0  // Код: 0
    msg[2] = 0  // Контрольная сумма (пока 0)
    msg[3] = 0  // Контрольная сумма (пока 0)
    msg[4] = 0  // Идентификатор
    msg[5] = 1  // Идентификатор
    msg[6] = 0  // Номер последовательности
    msg[7] = 1  // Номер последовательности

    // Вычисляем контрольную сумму
    checksum := checkSum(msg)
    msg[2] = byte(checksum >> 8)
    msg[3] = byte(checksum & 0xFF)

    // Отправляем ICMP-пакет (без чтения ответа)
    _, err = conn.Write(msg)
    return err
}

// Вычисление контрольной суммы ICMP-пакета
func checkSum(msg []byte) uint16 {
    sum := 0
    for i := 0; i < len(msg); i += 2 {
        sum += int(msg[i])<<8 + int(msg[i+1])
    }
    sum = (sum >> 16) + (sum & 0xFFFF)
    sum += sum >> 16
    return uint16(^sum)
}