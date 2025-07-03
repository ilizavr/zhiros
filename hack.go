package main

import (
    "fmt"
    "net"
    "sync"
    "time"
)

const (
    target    = "192.168.1.7" // Замените на нужный IP
    count     = 10000000      // Общее количество запросов
    timeout   = time.Second * 1
    workers   = 100           // Количество горутин
    batchSize = 1000          // Размер пачки запросов на одну горутину
)

func main() {
    fmt.Printf("Отправка %d ICMP-запросов на %s в %d потоках...\n", count, target, workers)

    var wg sync.WaitGroup
    successCh := make(chan int, workers)
    failCh := make(chan int, workers)

    startTime := time.Now()

    // Запускаем worker'ов
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go worker(i, &wg, successCh, failCh)
    }

    // Собираем результаты
    var success, fail int
    go func() {
        for s := range successCh {
            success += s
        }
    }()
    go func() {
        for f := range failCh {
            fail += f
        }
    }()

    wg.Wait()
    close(successCh)
    close(failCh)

    totalTime := time.Since(startTime)
    requestsPerSec := float64(count) / totalTime.Seconds()

    fmt.Printf("\nИтог:\n")
    fmt.Printf("Успешные отправки: %d\n", success)
    fmt.Printf("Неудачные отправки: %d\n", fail)
    fmt.Printf("Успешность: %.2f%%\n", float64(success)/float64(count)*100)
    fmt.Printf("Время выполнения: %v\n", totalTime)
    fmt.Printf("Запросов в секунду: %.0f\n", requestsPerSec)
}

func worker(id int, wg *sync.WaitGroup, successCh, failCh chan int) {
    defer wg.Done()

    localSuccess := 0
    localFail := 0

    for i := 0; i < batchSize; i++ {
        err := sendICMP(target, timeout)
        if err != nil {
            localFail++
        } else {
            localSuccess++
        }
    }

    successCh <- localSuccess
    failCh <- localFail
}

func sendICMP(ip string, timeout time.Duration) error {
    conn, err := net.DialTimeout("ip4:icmp", ip, timeout)
    if err != nil {
        return err
    }
    defer conn.Close()

    msg := make([]byte, 8)
    msg[0] = 8
    msg[1] = 0
    msg[2] = 0
    msg[3] = 0
    msg[4] = 0
    msg[5] = 1
    msg[6] = 0
    msg[7] = 1

    checksum := checkSum(msg)
    msg[2] = byte(checksum >> 8)
    msg[3] = byte(checksum & 0xFF)

    _, err = conn.Write(msg)
    return err
}

func checkSum(msg []byte) uint16 {
    sum := 0
    for i := 0; i < len(msg); i += 2 {
        sum += int(msg[i])<<8 + int(msg[i+1])
    }
    sum = (sum >> 16) + (sum & 0xFFFF)
    sum += sum >> 16
    return uint16(^sum)
}