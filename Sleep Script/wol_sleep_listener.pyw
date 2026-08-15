import socket
import os
import psutil

WOL_PORT = 9
INTERFACE_NAME = 'Ethernet'


def get_ip_mac_address(interface_name: str) -> tuple:
    ip_addr = mac_addr = None

    for item in psutil.net_if_addrs().get(interface_name, []):
        addr = item.address

        
        if '.' in addr:
            ip_addr = addr
        elif ('-' in addr or ':' in addr) and '::' not in addr:
            
            mac_addr = addr.replace(':', '-').upper()

    if not ip_addr or not mac_addr or ip_addr == '127.0.0.1':
        raise Exception('Не удалось получить IP или MAC-адрес сетевого интерфейса')

    return ip_addr, mac_addr


def assemble_wol_packet(mac_address: str) -> str:
    return f'{"FF-" * 6}{(mac_address + "-") * 16}'

def check_is_sleep_packet(raw_bytes: bytes, target_mac: str) -> bool:
   
    try:
        data = raw_bytes.decode('utf-8')
        if data.startswith("SLEEP:"):
            
            received_mac = data.split("SLEEP:")[1].strip().upper().replace(':', '-')
            if received_mac == target_mac:
                return True
    except Exception:
        pass

    return False

def run_udp_port_listener(port: int, interface_name: str):
    ip_addr, mac_addr = get_ip_mac_address(interface_name)

    server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server_socket.bind((ip_addr, port))

    while True:
       
        data, addr = server_socket.recvfrom(1024)

        if check_is_sleep_packet(data, mac_addr):
            
            server_socket.sendto(b"SLEEP_ACK", addr)
            if os.name == 'posix':
                os.system('sudo shutdown -h now')
            elif os.name == 'nt':
                os.system('shutdown -s -t 0 -f')


if __name__ == '__main__':
    run_udp_port_listener(WOL_PORT, INTERFACE_NAME)