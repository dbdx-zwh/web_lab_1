from ctypes import sizeof
import socket, os
import threading
import struct


MAX_CLIENTS = 2
TEXT_MESSAGE_FLAG = 1
SEND_FILE_FLAG = 2
RECV_FILE_FLAG = 3
SERVER_PORT = 12345
MAX_BUFFER_SIZE = 1024
SERVER_IP = "10.211.55.3"
FILE_PATH = "./server_file/file.pdf"


def pyRecvNum(byte_str):
    return struct.unpack("i", byte_str)[0]


def decode_able(encoded_value):
    try:
        value = int(encoded_value.decode())
        return value <= 4
    except Exception:
        return False


def handle_client(client_socket):
    while True:
        try:
            flag = client_socket.recv(4)
            if decode_able(flag):
                flag = int(flag.decode())
            else:
                flag = pyRecvNum(flag)

            # flag = client_socket.recv(struct.calcsize('i'))
            if not flag:
                break

            if flag == TEXT_MESSAGE_FLAG:
                mesg = client_socket.recv(MAX_BUFFER_SIZE)
                print(f"Received: {mesg.decode()}")

            elif flag == SEND_FILE_FLAG:
                file_size = int(client_socket.recv(MAX_BUFFER_SIZE).decode())
                print("recv file size: ", file_size)

                with open(FILE_PATH, "wb") as file:
                    while file_size > 0:
                        data = client_socket.recv(MAX_BUFFER_SIZE)
                        if not data:
                            break  # Connection closed by client
                        file.write(data)
                        file_size -= len(data)

                print("File received successfully")

            elif flag == RECV_FILE_FLAG:
                with open(FILE_PATH, "rb") as file:
                    file_size = os.path.getsize(FILE_PATH)
                    print("send file size: ", file_size)
                    client_socket.send(str(file_size).encode())

                    while True:
                        data = file.read(MAX_BUFFER_SIZE)
                        if not data:
                            break  # Reached the end of the file
                        client_socket.send(data)

                print("File sent successfully")

            else:
                break

        except OSError as e:
            raise

    print("Client disconnected")
    client_socket.close()


server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server_socket.bind((SERVER_IP, SERVER_PORT))
server_socket.listen(MAX_CLIENTS)
print(f"Server listening on port {SERVER_PORT}...")

# Accept connections and handle them in separate threads
client_sockets, client_threads = [], []
for i in range(MAX_CLIENTS):
    client_socket, addr = server_socket.accept()
    client_socket.send(str(i).encode())
    client_sockets.append(client_socket)

    client_thread = threading.Thread(target=handle_client, args=(client_socket,))
    client_threads.append(client_thread)
    client_thread.start()

for thread in client_threads:
    thread.join()

# Wait for all threads to finish
for client_socket in client_sockets:
    client_socket.close()
