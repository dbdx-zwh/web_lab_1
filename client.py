import socket, os, struct

TEXT_MESSAGE_FLAG = 1
SEND_FILE_FLAG = 2
RECV_FILE_FLAG = 3
SERVER_PORT_1 = 12345
SERVER_PORT_2 = 12345
MAX_BUFFER_SIZE = 1024
SERVER_IP_1 = "10.211.55.3"
SERVER_IP_2 = "10.47.128.157"
SEND_FILE_PATH = "./client_file/file.pdf"
RECV_FILE_PATH = "./client_file/file.txt"

# Create a socket object
client_socket_1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
client_socket_2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Set up the server address
server_address_1 = (SERVER_IP_1, SERVER_PORT_1)
server_address_2 = (SERVER_IP_2, SERVER_PORT_2)

# Connect to the server
client_socket_1.connect(server_address_1)
print("Connected to server 1")
client_socket_2.connect(server_address_2)
print("Connected to server 2")


def pyRecvNum(byte_str):
    return struct.unpack("i", byte_str)[0]


while True:
    while True:
        print("select one server you want to interact with: ")
        print("1: means this server")
        print("2: means that server")
        choice = input()
        if int(choice) == 1:
            client_socket = client_socket_1
            break
        elif int(choice) == 2:
            client_socket = client_socket_2
            break
        else:
            print("you should input 1 or 2!")
            break

    print("select your option: ")
    print("1: send text message")
    print("2: send large file")
    print("3: recv large file")
    print("4: exit")
    flag = input()
    client_socket.send(str(flag).encode())

    if int(flag) == TEXT_MESSAGE_FLAG:
        mesg = input("Enter the message: ")
        client_socket.send(mesg.encode())

    elif int(flag) == SEND_FILE_FLAG:
        with open(SEND_FILE_PATH, "rb") as file:
            file_size = os.path.getsize(SEND_FILE_PATH)
            print("send file size: ", file_size)
            client_socket.send(str(file_size).encode())

            while True:
                data = file.read(MAX_BUFFER_SIZE)
                if not data:
                    break  # Reached the end of the file
                client_socket.send(data)

        print("File sent successfully")

    elif int(flag) == RECV_FILE_FLAG:
        # alert: the first recv file_size is 0, I don't know why, so i user "while" to get file size
        # PS: i am MA LE

        # wrong version
        # file_size_str = client_socket.recv(4)
        # print("file size str: ", file_size_str)
        # file_size = pyRecvNum(file_size_str)
        # print("recv file size: ", file_size)

        # right version
        while True:
            file_size_str = client_socket.recv(4)
            file_size = pyRecvNum(file_size_str)
            if file_size > 1:
                break

        print("file size: ", file_size)

        with open(RECV_FILE_PATH, "wb") as file:
            while file_size > 0:
                data = client_socket.recv(MAX_BUFFER_SIZE)
                if not data:
                    break  # Connection closed by client
                file.write(data)
                file_size -= len(data)

        print("File received successfully")

    else:
        if client_socket != client_socket_1:
            client_socket_1.send(str(flag).encode())
        else:
            client_socket_2.send(str(flag).encode())
        break

client_socket_1.close()
client_socket_2.close()
