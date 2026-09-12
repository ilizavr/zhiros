# Сеть жирОС

Сетевой стек жирОС развивается по слоям. Первый целевой адаптер — QEMU E1000.

## Слои

```text
E1000 driver -> network device -> Ethernet -> ARP -> IPv4 -> ICMP/UDP/TCP -> shell
```

Драйвер знает про PCI, MMIO, DMA и descriptor rings. Протоколы не должны знать про регистры E1000.

## План

1. Найти E1000 и прочитать BAR0.
2. Включить Memory Space и Bus Mastering.
3. Сбросить устройство и прочитать MAC/link status.
4. Настроить RX/TX rings и polling.
5. Принять и отправить raw Ethernet frame.
6. Добавить ARP cache и request/reply.
7. Добавить статический IPv4.
8. Добавить ICMP echo и shell-команду `ping`.
9. Добавить UDP echo.
10. Добавить DHCP lease lifecycle.
11. Добавить TCP over IPv4 MVP.
12. После стабилизации протоколов отдельно рассмотреть IRQ.

## Текущее состояние

- E1000 raw TX/RX polling реализован через callback-интерфейс.
- ARP request/reply и небольшой статический cache реализованы в `net/arp.h`.
- IPv4 header/checksum и ICMP echo request/reply реализованы в `net/ipv4.h`.
- UDP datagram/echo path и общий network poll реализованы в `net/udp.h`.
- Background network process постоянно polling RX ring и отвечает на ARP/ICMP/UDP.
- Shell smoke-команды: `nettx`, `netrx`, `netarp`, `netping`, `netudp`, `netpoll`, `netstats`.
- `netarp` и `netping` принимают необязательный IPv4; `netudp` принимает `[ipv4] [port]`.
- ARP cache имеет age/TTL, lookup counters, expiry и explicit invalidate.
- `netping` автоматически ждёт ARP resolution и ICMP reply, делает до трёх попыток и инвалидирует stale ARP entry перед retry.
- UDP endpoint table поддерживает bind/unbind до 4 ports, очереди до 4 datagrams на port и bounded payload до 512 байт.
- Shell UDP API: `udplisten <port>`, `udpunbind <port>`, `udprecv <port>`, `udpsend <ipv4> <port> <text>`.
- Минимальный DHCP client в `net/dhcp.h` реализует DISCOVER/OFFER/REQUEST/ACK через broadcast UDP 68/67.
- `dhcp` команда получает lease и обновляет IPv4 state; static `10.0.2.15` остаётся fallback до успешного ACK.
- DHCP protocol-level DORA входит в `make net_test`; внешний peer для QEMU находится в `tests/qemu_dhcp_peer.py`.
- Lease state хранит T1/T2/expiry; background worker запускает renewal/rebind, а по expiry очищает адрес и ARP cache.
- TCP over IPv4 MVP в `net/tcp.h` поддерживает LISTEN, handshake, bounded receive buffer 512 байт, ACK/PSH, FIN и RST cleanup.
- TCP worker polling запускает bounded retransmission для SYN, payload и FIN: RTO 250 ticks, до 3 повторов, затем connection cleanup; idle established connection ограничен 30 секундами.
- Для zero-window есть bounded persist timer: до 3 probes с интервалом 500 ticks; после исчерпания budget connection очищается, а window update сбрасывает persist state и возобновляет отправку.
- TCP connection хранит до 4 unacknowledged segments, соблюдает advertised receive window, обрабатывает cumulative ACK, duplicate ACK/fast retransmit и duplicate/out-of-order payload с повторным ACK.
- TCP negotiates MSS, window scale и SACK-permitted; ACK может нести до 3 SACK blocks.
- TCP parser отклоняет битые IPv4/TCP checksum, невозможные header length/total length, malformed options и ACK за пределами отправленной sequence space; причины считаются отдельно.
- Упрощённый Reno congestion window ограничивает передачу, растёт на ACK и уменьшается при timeout/fast retransmit.
- `tcpinfo` показывает state/ports, cwnd/ssthresh, remote window/MSS, TX/reorder queue, duplicate ACK и retransmit/SACK/persist counters; malformed/checksum/options/ACK errors выводятся отдельной строкой.
- Zero-window ACK блокирует новую передачу до window update; persist probes ограничены timer/retry budget и не обходят advertised window.
- Для входящих данных есть bounded reorder queue до 4 сегментов; contiguous delivery автоматически дренирует очередь в receive buffer.
- FIN lifecycle разделён на FIN-WAIT-1, FIN-WAIT-2, CLOSING, CLOSE-WAIT, LAST-ACK и TIME-WAIT с bounded TIME-WAIT cleanup.
- TCP shell API: `tcplisten <port>`, `tcpconnect <ipv4> <port>`, `tcpsend <text>`, `tcprecv`, `tcpclose`, `tcpinfo`.
- Статическая IPv4-конфигурация первого теста: `10.0.2.15`, gateway `10.0.2.2`.
- Synthetic protocol test запускается через `make net_test`: проверяет ARP reply, ICMP echo reply, UDP echo reply, DHCP lease lifecycle, malformed TCP headers/options/checksum/flags, zero-window persist/reopen/budget cleanup, RST/stale connection cleanup и sequence wraparound.
- End-to-end test через QEMU socket backend запускается `python3 tests/qemu_socket_test.py 12345` при QEMU с `-netdev socket,id=net0,listen=127.0.0.1:12345`.
- TCP raw Ethernet smoke-test запускается на test-only build `make build_tcp_test`, затем `python3 tests/qemu_tcp_test.py 12345` с listener на `4001`; проверяет drop malformed options/checksum/flags, SYN retransmission, controlled loss/retransmission исходящих segments, MSS/window-scale/SACK negotiation, SACK recovery, out-of-order payload, SACK ACK, duplicate data, FIN ACK и test-only serial `tcpinfo` output.
- `-netdev user` по-прежнему не используется для подтверждения входящего raw Ethernet: socket backend даёт контролируемого L2 peer.

## Ограничения первого этапа

- Только 32-битная жирОС.
- Только QEMU E1000.
- Сначала polling, не IRQ.
- Никакого выделения памяти на каждый пакет.
- DMA rings и буферы получают явное выравнивание и живут до остановки устройства.
- IPv4 использует статический fallback и DHCP lease lifecycle; IPv6/TCPv6 пока не добавлялись.
- TCP использует упрощённый Reno/SACK/window scaling; базовое SACK recovery покрыто synthetic/runtime tests, но полноценные RFC edge cases и production-grade congestion control пока не являются целью.

## Рабочие правила

- RX/TX ownership устройства и CPU не смешивается.
- Ошибки и переполнения rings обрабатываются явно.
- Драйвер и протоколы тестируются по отдельности.
- Изменения сети не должны требовать массового переноса старых файлов.
