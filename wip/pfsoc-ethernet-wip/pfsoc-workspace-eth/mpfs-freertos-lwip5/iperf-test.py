import argparse
import socket
import sys
import time

DEFAULT_IP = "192.168.2.207"
DEFAULT_PORT = 5001
DEFAULT_DURATION = 10
BUFFER_SIZE = 1460 * 4  # 5840 bytes payload chunks


def parse_arguments():
    parser = argparse.ArgumentParser(
        description="PolarFire SoC Bare-Metal lwIP TCP Bandwidth Tester",
        formatter_class=argparse.RawTextHelpFormatter,
        epilog="""Examples:
  python iperf_test.py                         (Uses default IP 192.168.2.207)
  python iperf_test.py 192.168.2.145           (Tests specific IP for 10s)
  python iperf_test.py 192.168.2.145 -t 20     (Tests specific IP for 20s)
  python iperf_test.py -h                      (Displays this help menu)""",
    )

    parser.add_argument(
        "ip",
        nargs="?",
        default=None,
        help=f"Target Board IP Address (Default: {DEFAULT_IP})",
    )
    parser.add_argument(
        "-t",
        "--time",
        type=int,
        default=DEFAULT_DURATION,
        help=f"Test duration in seconds (Default: {DEFAULT_DURATION}s)",
    )
    parser.add_argument(
        "-p",
        "--port",
        type=int,
        default=DEFAULT_PORT,
        help=f"Target TCP Port (Default: {DEFAULT_PORT})",
    )

    return parser.parse_args()


def run_throughput_test(host, port, duration):
    payload = b"X" * BUFFER_SIZE
    total_bytes = 0

    print(f"\n[*] Target Board IP : {host}")
    print(f"[*] Target TCP Port: {port}")
    print(f"[*] Test Duration  : {duration} seconds")
    print(f"[*] Connecting...")

    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((host, port))
        sock.settimeout(None)

        print(f"[*] Connected! Streaming TCP traffic...")

        start_time = time.time()
        end_time = start_time + duration

        while time.time() < end_time:
            sent = sock.send(payload)
            if sent == 0:
                raise RuntimeError("Socket connection lost")
            total_bytes += sent

        actual_duration = time.time() - start_time
        sock.close()

        mbits = (total_bytes * 8) / 1_000_000
        throughput_mbps = mbits / actual_duration

        print("\n================ PC Throughput Results ================")
        print(f" Target IP   : {host}")
        print(f" Transferred : {total_bytes / (1024 * 1024):.2f} MB")
        print(f" Duration    : {actual_duration:.2f} seconds")
        print(f" Throughput  : {throughput_mbps:.2f} Mbps")
        print("=======================================================")
        print("[*] Check board terminal for board-side hardware report!\n")

    except socket.timeout:
        print(
            f"[!] Error: Connection timed out. Is the board online at {host}?"
        )
    except Exception as e:
        print(f"[!] Error: {e}")


if __name__ == "__main__":
    args = parse_arguments()

    # Determine IP address
    target_ip = args.ip
    if target_ip is None:
        user_input = input(
            f"Enter Board IP address [Press Enter for {DEFAULT_IP}]: "
        ).strip()
        target_ip = user_input if user_input else DEFAULT_IP

    run_throughput_test(target_ip, args.port, args.time)