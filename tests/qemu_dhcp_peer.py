#!/usr/bin/env python3
import socket
import struct
import sys
import time

SERVER_MAC = bytes.fromhex("525400123401")
OFFERED_IP = bytes([10, 0, 2, 20])
SERVER_IP = bytes([10, 0, 2, 1])
BROADCAST_IP = bytes([255, 255, 255, 255])


def checksum(data):
    if len(data) & 1:
        data += b"\0"
    total = 0
    for index in range(0, len(data), 2):
        total += (data[index] << 8) | data[index + 1]
        total = (total & 0xFFFF) + (total >> 16)
    while total >> 16:
        total = (total & 0xFFFF) + (total >> 16)
    return (~total) & 0xFFFF


def read_exact(sock, length):
    result = bytearray()
    while len(result) < length:
        chunk = sock.recv(length - len(result))
        if not chunk:
            raise RuntimeError("QEMU socket closed")
        result.extend(chunk)
    return bytes(result)


def recv_frame(sock):
    length = struct.unpack("!I", read_exact(sock, 4))[0]
    if length < 14 or length > 2048:
        raise RuntimeError("invalid frame length")
    return read_exact(sock, length)


def send_frame(sock, frame):
    sock.sendall(struct.pack("!I", len(frame)) + frame)


def option(options, code, value):
    options.extend(bytes([code, len(value)]))
    options.extend(value)


def dhcp_payload(request, message_type):
    payload = bytearray(236)
    payload[0] = 2
    payload[1] = 1
    payload[2] = 6
    payload[4:8] = request[4:8]
    payload[16:20] = OFFERED_IP
    payload[20:24] = SERVER_IP
    payload[28:34] = request[28:34]
    options = bytearray(b"\x63\x82\x53\x63")
    option(options, 53, bytes([message_type]))
    option(options, 54, SERVER_IP)
    option(options, 1, bytes([255, 255, 255, 0]))
    option(options, 3, SERVER_IP)
    option(options, 51, struct.pack("!I", 3600))
    options.append(255)
    return bytes(payload + options)


def make_frame(request, payload):
    peer_mac = request[6:12]
    udp = struct.pack("!HHHH", 67, 68, 8 + len(payload), 0) + payload
    pseudo = SERVER_IP + BROADCAST_IP + struct.pack("!BBH", 0, 17, len(udp))
    udp = udp[:6] + struct.pack("!H", checksum(pseudo + udp)) + udp[8:]
    ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(udp), 1, 0,
                     64, 17, 0, SERVER_IP, BROADCAST_IP)
    ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
    return b"\xff" * 6 + SERVER_MAC + struct.pack("!H", 0x0800) + ip + udp


def message_type(payload):
    offset = 240
    while offset < len(payload):
        code = payload[offset]
        offset += 1
        if code == 0:
            continue
        if code == 255:
            break
        if offset >= len(payload):
            break
        length = payload[offset]
        offset += 1
        if offset + length > len(payload):
            break
        if code == 53 and length == 1:
            return payload[offset]
        offset += length
    return 0


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 12345
    peer = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    deadline = time.time() + 5
    while True:
        try:
            peer.connect(("127.0.0.1", port))
            break
        except ConnectionRefusedError:
            if time.time() >= deadline:
                raise
            time.sleep(0.05)
    seen_offer = False
    seen_ack = False
    while not seen_ack:
        frame = recv_frame(peer)
        if len(frame) < 42 or struct.unpack("!H", frame[12:14])[0] != 0x0800:
            continue
        udp_offset = 14 + ((frame[14] & 0x0F) * 4)
        source_port, destination_port = struct.unpack("!HH", frame[udp_offset:udp_offset + 4])
        if source_port != 68 or destination_port != 67:
            continue
        payload = frame[udp_offset + 8:]
        message = message_type(payload)
        if message == 1:
            send_frame(peer, make_frame(frame, dhcp_payload(payload, 2)))
            seen_offer = True
            print("DHCP offer sent", flush=True)
        elif message == 3:
            send_frame(peer, make_frame(frame, dhcp_payload(payload, 5)))
            seen_ack = True
            print("DHCP ACK sent", flush=True)
    peer.close()
    if not seen_offer or not seen_ack:
        raise RuntimeError("incomplete DHCP exchange")


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print("DHCP peer: FAIL", error)
        sys.exit(1)
