#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>

#define TEXT_MESSAGE_FLAG 0
#define FILE_MESSAGE_FLAG 1
#define MAX_BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 1234
#define FILE_PATH "./video.mp4"

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

int *split_file(int n, int num_threads)
{
    int *arr = (int *)malloc(sizeof(int) * num_threads);
    if (n > MAX_BUFFER_SIZE)
    {
        int total_bytes_covered = 0;
        for (int i = 0; i < num_threads; i++)
        {
            if (i == num_threads - 1)
            {
                arr[i] = n - total_bytes_covered;
                break;
            }
            arr[i] = (n / num_threads);
            arr[i] = arr[i] - (n / num_threads) % MAX_BUFFER_SIZE;
            total_bytes_covered += arr[i];
        }
        return arr;
    }

    for (int i = 0; i < num_threads; i++)
    {
        arr[i] = 0;
    }
    arr[0] = n;
    return arr;
}

long *get_offsets(int *arr, int num_threads)
{
    long *offsets = (long *)malloc(sizeof(long) * num_threads);
    offsets[0] = 0;
    for (int j = 1; j < num_threads; j++)
    {
        offsets[j] = offsets[j - 1] + arr[j - 1];
    }
    return offsets;
}

void slowSendFile(FILE *file, int client_socket)
{
    long start_time = getCurrentTimeInMilliseconds();

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    send(client_socket, &file_size, sizeof(file_size), 0);

    char buffer[MAX_BUFFER_SIZE];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
        send(client_socket, buffer, bytes_read, 0);

    long end_time = getCurrentTimeInMilliseconds();
    printf("File transfer completed in %ld milliseconds.\n", end_time - start_time);
}

typedef struct
{
    FILE *fp;
    int sockfd;
    int size;
    int thread_id;
    long offset;
} thread_args;

void *threadSendFile(void *args)
{
    int size_in_bytes_to_send = ((thread_args *)args)->size;
    int sockfd = ((thread_args *)args)->sockfd;
    FILE *file = ((thread_args *)args)->fp;
    int thread_id = ((thread_args *)args)->thread_id;
    long base_offset = ((thread_args *)args)->offset;
    int n;
    char buffer[MAX_BUFFER_SIZE];
    // convert base_offset to char buffer'
    char *offset_buffer = (char *)malloc(sizeof(char) * MAX_BUFFER_SIZE);
    sprintf(offset_buffer, "%ld", base_offset);
    // send offset_buffer to server
    send(sockfd, offset_buffer, MAX_BUFFER_SIZE, 0);
    // int port = SERVER_PORT + (thread_id + 1) * 2;
    // printf("[+]Thread %d sent offset %ld on port %d\n", thread_id, base_offset, port);
    int bytes_sent = 0;
    while (1)
    {
        bzero(buffer, MAX_BUFFER_SIZE);
        if ((bytes_sent >= (size_in_bytes_to_send)))
        {
            break;
        }
        n = pread(fileno(file), buffer, MAX_BUFFER_SIZE, base_offset + bytes_sent);
        if (n <= 0)
        {
            break;
        }
        write(sockfd, buffer, n);
        bytes_sent += n;
    }
    // printf("[+]Total bytes sent by thread %d on port %d: %d\n", thread_id, port, bytes_sent);
    free(offset_buffer);
    return NULL;
}

void threadsSendFile(FILE *file, int *sockets, int num_threads)
{
    pthread_t threads[num_threads];

    fseek(file, 0L, SEEK_END);
    int file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    int *arr = split_file(file_size, num_threads);
    long *offsets = get_offsets(arr, num_threads);

    for (int i = 0; i < num_threads; i++)
    {
        thread_args *args = (thread_args *)malloc(sizeof(thread_args));
        args->sockfd = sockets[i];
        args->fp = file;
        args->thread_id = i;
        args->size = arr[i];
        args->offset = offsets[i];
        pthread_create(&threads[i], NULL, threadSendFile, (void *)args);
    }
    for (int i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }
    free(arr);
    free(offsets);
}

void quickSendFile(FILE *file, int client_socket, int num_threads, int client_id)
{
    int thread_sockets[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        sleep(1);
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0)
        {
            perror("[-] Error creating socket");
            exit(1);
        }
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
        server_addr.sin_port = SERVER_PORT + (i + 1) * 2;

        // avoid thread reuse, cause max clients is 2, so easy solution below
        if (client_id % 2 == 0)
            server_addr.sin_port -= 1;

        printf("sin port id: %d\n", server_addr.sin_port);

        if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            perror("[-]Error in thread connection");
            exit(1);
        }
        printf("created thread %d\n", i);
        thread_sockets[i] = sockfd;
    }

    long start_time = getCurrentTimeInMilliseconds();
    threadsSendFile(file, thread_sockets, num_threads);
    long end_time = getCurrentTimeInMilliseconds();

    for (int i = 0; i < num_threads; i++)
        close(thread_sockets[i]);

    printf("File transfer completed in %ld milliseconds.\n", end_time - start_time);
}

int main(int argc, char *argv[])
{
    const char *server_ip = SERVER_IP;
    int server_port = SERVER_PORT;

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1)
        error("Error creating socket");

    struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);

    if (inet_pton(AF_INET, server_ip, &server_address.sin_addr) <= 0)
        error("Error converting server IP address");
    if (connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1)
        error("Error connecting to the server");

    int client_id;
    recv(client_socket, &client_id, sizeof(client_id), 0);
    printf("Client id is %d\n", client_id);

    while (1)
    {
        int flag;
        printf("choose send text or large file: ");
        scanf("%d", &flag);
        send(client_socket, &flag, sizeof(flag), 0);
        fflush(stdin);

        if (flag == TEXT_MESSAGE_FLAG)
        {
            char buffer[MAX_BUFFER_SIZE];
            printf("Enter the message: ");
            fgets(buffer, sizeof(buffer), stdin);
            if (send(client_socket, buffer, sizeof(buffer), 0) == -1)
                error("Error sending message");
        }
        else if (flag == FILE_MESSAGE_FLAG)
        {
            FILE *file = fopen(FILE_PATH, "rb");
            if (file == NULL)
                error("Error opening file");

            fseek(file, 0, SEEK_SET);
            // slowSendFile(file, client_socket);
            quickSendFile(file, client_socket, 3, client_id);
            fclose(file);
        }
        else
            break;
    }

    close(client_socket);
    return 0;
}
