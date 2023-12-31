#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>
#include "settings.h"

#define FILE_PATH "./server_file/file.txt"

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
        server_addr.sin_addr.s_addr = inet_addr(SERVER_IP_1);

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

void slowSendFile(FILE *file, int client_socket)
{
    long start_time = getCurrentTimeInMilliseconds();

    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
        send(client_socket, buffer, bytes_read, 0);

    long end_time = getCurrentTimeInMilliseconds();
    printf("File transfer completed in %ld milliseconds.\n", end_time - start_time);
}

typedef struct
{
    int client_id;
    int client_fd;
} Client_args;

int cRecvNum(char *byte_str)
{
    int length = strlen(byte_str);
    byte_str[length] = '\0';
    return atoi(byte_str);
}

void *handle_client(void *args)
{
    char byte_flag[50];
    int client_fd = ((Client_args *)args)->client_fd;
    int client_id = ((Client_args *)args)->client_id;

    while (read(client_fd, &byte_flag, sizeof(byte_flag)) > 0)
    {
        int flag = (int)byte_flag[0];
        if (flag >= 4)
            flag = cRecvNum(byte_flag);
        fflush(stdin);

        if (flag == TEXT_MESSAGE_FLAG)
        {
            // Receive text message from client
            char text_message[MAX_BUFFER_SIZE];
            recv(client_fd, text_message, sizeof(text_message), 0);
            printf("Received text message from client: %s\n", text_message);
        }

        else if (flag == SEND_FILE_FLAG)
        {
            // "send" is the client view, so server needs recv
            FILE *file = fopen(FILE_PATH, "wb");
            if (file == NULL)
                error("Error opening file");
            quickRecvFile(file, client_fd, NUM_THREADS, client_id);
        }

        else if (flag == RECV_FILE_FLAG)
        {
            // "recv" is the client view, so server needs send
            FILE *file = fopen(FILE_PATH, "rb");
            if (file == NULL)
            {
                printf("another client didn't send file to server");
                continue;
            }

            fseek(file, 0, SEEK_END);
            int file_size = ftell(file);
            rewind(file);
            send(client_fd, &file_size, sizeof(file_size), 0);
            printf("send file size: %d\n", file_size);

            slowSendFile(file, client_fd);
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
    }
    close(server_socket);
    pthread_exit(NULL);

    return 0;
}
