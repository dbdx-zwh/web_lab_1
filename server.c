#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>

#define MAX_CLIENTS 2
#define TEXT_MESSAGE_FLAG 0
#define FILE_MESSAGE_FLAG 1
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 1234
#define MAX_BUFFER_SIZE 1024
#define RECEIVED_FILE_PATH "recv.mp4" // 接收到的文件保存路径

void error(const char *msg)
{
    perror(msg);
    exit(1);
}

long getCurrentTimeInMilliseconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void slowRecvFile(FILE *file, int client_fd)
{
    long startTime = getCurrentTimeInMilliseconds();

    // Receive file size from client
    long file_size;
    recv(client_fd, &file_size, sizeof(file_size), 0);
    printf("Receiving file of size: %ld bytes\n", file_size);

    // Receive file in chunks
    char buffer[MAX_BUFFER_SIZE];
    size_t totalBytesReceived = 0;
    while (totalBytesReceived < file_size)
    {
        size_t bytesRead = recv(client_fd, buffer, sizeof(buffer), 0);
        fwrite(buffer, 1, bytesRead, file);
        totalBytesReceived += bytesRead;
    }

    long endTime = getCurrentTimeInMilliseconds();
    printf("File received in %ld milliseconds.\n", endTime - startTime);
    fclose(file);
}

typedef struct
{
    FILE *fp;
    int sockfd;
    int size;
    int thread_id;
    long offset;
} Thread_args;

void *threadRecvFile(void *args)
{
    FILE *fp = ((Thread_args *)args)->fp;
    int sockfd = ((Thread_args *)args)->sockfd;
    int thread_id = ((Thread_args *)args)->thread_id;
    int n;
    char buffer[MAX_BUFFER_SIZE];
    int total_in_bytes = 0;
    // recive offset_buffer from client
    char *offset_buffer = (char *)malloc(sizeof(char) * MAX_BUFFER_SIZE);
    bzero(offset_buffer, MAX_BUFFER_SIZE);
    read(sockfd, offset_buffer, MAX_BUFFER_SIZE);
    // convert offset_buffer to long
    long base_offset = atol(offset_buffer);
    // int port = SERVER_PORT + (thread_id + 1) * 2;
    // printf("[+]Thread %d recived offset %ld at port %d\n", thread_id, base_offset, port);
    while (1)
    {
        n = read(sockfd, buffer, MAX_BUFFER_SIZE);
        if (n <= 0)
        {
            break;
        }
        total_in_bytes += n;
        pwrite(fileno(fp), buffer, n, base_offset);
        base_offset += n;
        bzero(buffer, MAX_BUFFER_SIZE);
    }
    // printf("[+]Total bytes read on port %d in thread %d: %d\n", port, thread_id, total_in_bytes);
    free(offset_buffer);
    return NULL;
}

void threadsRecvFile(FILE *fp, int *sockfd, int num_threads)
{
    int i;
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        Thread_args *args = malloc(sizeof(Thread_args));
        args->sockfd = sockfd[i];
        args->thread_id = i;
        args->offset = 0;
        args->size = 0;
        args->fp = fp;
        pthread_create(&threads[i], NULL, threadRecvFile, args);
    }
    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }
    fclose(fp);
    return;
}

void quickRecvFile(FILE *file, int client_fd, int num_threads, int client_id)
{
    int thread_sockets[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("[-] Error creating socket");
            exit(1);
        }
        struct sockaddr_in server_addr, new_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = SERVER_PORT + (i + 1) * 2;
        server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);

        if (client_id % 2 == 0)
            server_addr.sin_port -= 1;

        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("[-]Error in connection");
            exit(1);
        }
        if (listen(sockfd, 10) < 0)
        {
            perror("[-]Error in listening");
            exit(1);
        }
        socklen_t addr_size = sizeof(new_addr);
        int new_sock = accept(sockfd, (struct sockaddr *)&new_addr, &addr_size);
        if (new_sock < 0)
        {
            perror("[-]Error in accepting");
            exit(1);
        }
        close(sockfd);
        thread_sockets[i] = new_sock;
        printf("created thread %d\n", i);
    }

    long start_time = getCurrentTimeInMilliseconds();
    threadsRecvFile(file, thread_sockets, num_threads);
    long end_time = getCurrentTimeInMilliseconds();

    for (int i = 0; i < num_threads; i++)
        close(thread_sockets[i]);

    printf("File transfer completed in %ld milliseconds.\n", end_time - start_time);
}

typedef struct
{
    int client_id;
    int client_fd;
} Client_args;

void *handle_client(void *args)
{
    int flag;
    int client_fd = ((Client_args *)args)->client_fd;
    int client_id = ((Client_args *)args)->client_id;

    while (read(client_fd, &flag, sizeof(flag)) > 0)
    {
        if (flag == TEXT_MESSAGE_FLAG)
        {
            // Receive text message from client
            char text_message[MAX_BUFFER_SIZE];
            recv(client_fd, text_message, sizeof(text_message), 0);
            printf("Received text message from client: %s", text_message);
        }
        else if (flag == FILE_MESSAGE_FLAG)
        {
            char filename[50];
            sprintf(filename, "%d_%s", client_fd, RECEIVED_FILE_PATH);
            FILE *file = fopen(filename, "wb");
            if (file == NULL)
                error("Error opening file");
            // slowRecvFile(file, client_fd);
            quickRecvFile(file, client_fd, 3, client_id);
            fclose(file);
        }
        else
            break;
    }

    printf("Client disconnected\n");
    close(client_fd);
    return NULL;
}

int main(int argc, char *argv[])
{
    int server_port = SERVER_PORT;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1)
        error("Error creating socket");

    struct sockaddr_in server_address, client_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(server_port);

    // Bind the socket
    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
        error("Error binding socket");
    // Listen for incoming connections
    if (listen(server_socket, MAX_CLIENTS) == -1)
        error("Error listening for connections");
    printf("Server listening on port %d...\n", SERVER_PORT);

    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        socklen_t client_address_len = sizeof(client_address);
        int client_fd = accept(server_socket, (struct sockaddr *)&client_address, &client_address_len);
        send(client_fd, &i, sizeof(i), 0);
        if (client_fd == -1)
            error("Error accepting connection");

        Client_args *args = malloc(sizeof(Client_args));
        args->client_fd = client_fd;
        args->client_id = i;
        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, args) != 0)
        {
            perror("Thread creation failed");
            exit(EXIT_FAILURE);
        }
        // if (pthread_create(&thread, NULL, handle_client, &client_fd) != 0)
        // {
        //     perror("Thread creation failed");
        //     exit(EXIT_FAILURE);
        // }
    }
    close(server_socket);
    pthread_exit(NULL);

    return 0;
}
