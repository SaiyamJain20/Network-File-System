#include "header.h"

void ss_handle_read_request(int client_sock, char *file_path)
{
    send_to_server(OK, REQUEST_READ_STARTED, file_path, -1);

    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", root_directory, file_path + 2);

    int file = open(full_path, O_RDONLY);
    if (file < 0)
    {
        // If the file can't be opened, send an error request
        Request error_request = {.error_code = FILE_NOT_FOUND};
        send(client_sock, &error_request, sizeof(error_request), 0);
        send_to_server(FILE_NOT_FOUND, REQUEST_READ_COMPLETED, file_path, -1);
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    // Read and send the file in requests
    while ((bytes_read = read(file, buffer, BUFFER_SIZE)) > 0)
    {
        Request request;
        memset(&request, 0, sizeof(Request));
        request.type = REQ;
        request.error_code = OK;
        memcpy(request.info, buffer, bytes_read);

        printf("Sending data to client: %.*s\n", BUFFER_SIZE, buffer);

        int status = send(client_sock, &request, sizeof(request), 0);

        if (status < 0)
        {
            perror("Failed to send request");
            close(file);
            return;
        }

        memset(buffer, 0, BUFFER_SIZE); // Clear data buffer for next read
    }

    close(file);
    printf("File sent successfully.\n");

    Request request;
    memset(&request, 0, sizeof(Request));
    request.type = ACK;
    request.error_code = OK;
    send(client_sock, &request, sizeof(request), 0);
    close(client_sock);
    send_to_server(OK, REQUEST_READ_COMPLETED, file_path, -1);
}

void ss_handle_write_request(int client_sock, char *file_path, bool isSync)
{
    send_to_server(OK, REQUEST_WRITE_STARTED, file_path, -1);

    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", root_directory, file_path + 2);

    int file = open(full_path, O_WRONLY | O_TRUNC);
    if (file < 0)
    {
        // If the file can't be opened, send an error request
        Request error_request = {.error_code = FILE_NOT_FOUND};
        send(client_sock, &error_request, sizeof(error_request), 0);
        send_to_server(FILE_NOT_FOUND, REQUEST_WRITE_COMPLETED, file_path, -1);
        return;
    }

    ListNode *requestList;
    requestList = NULL;
    int numberOfNodes = 0;

    Request request;
    // Receive requests and write data to file
    while (recv(client_sock, &request, sizeof(request), MSG_WAITALL) > 0)
    {
        if (request.type == ACK)
        {
            printf("All requests received and written successfully.\n");
            break;
        }

        printf("%s\n", request.info);

        char i[MAX_INFO_SIZE];
        strcpy(i, request.info);

        append_to_list(&requestList, i);
        numberOfNodes++;

        memset(request.info, 0, MAX_PACKET_SIZE); // Clear data buffer for next read
    }

    printf("%d\n", numberOfNodes);
    int bytesWritten = 0;

    if (isSync || numberOfNodes <= MAX_SYNC_SIZE_ALLOWED)
    {
        send_to_server(OK, REQUEST_WRITE_STARTED_SYNC, file_path, -1);
        bytesWritten = ss_write(client_sock, file, requestList);

        Request request;
        memset(&request, 0, sizeof(Request));
        request.type = ACK;
        if (bytesWritten == 0)
        {
            request.error_code = WRITE_FAILED;
        }
        else
        {
            request.error_code = OK;
        }

        send(client_sock, &request, sizeof(request), 0);
        close(client_sock);
    }
    else
    {
        Request request;
        memset(&request, 0, sizeof(Request));
        request.type = ACK_ASYNC;
        request.error_code = OK;
        send(client_sock, &request, sizeof(request), 0);
        close(client_sock);
        
        send_to_server(OK, REQUEST_WRITE_STARTED_ASYNC, file_path, -1);
        bytesWritten = ss_write(client_sock, file, requestList);
    }

    if (bytesWritten == 0)
    {
        send_to_server(WRITE_FAILED, REQUEST_WRITE_COMPLETED, file_path, -1);
    }
    else
    {
        send_to_server(OK, REQUEST_WRITE_COMPLETED, file_path, -1);
    }

    close(file);
}

int ss_write(int client_sock, int file, ListNode *requestList)
{
    size_t total_bytes_written = 0;
    ListNode *current = requestList;

    while (current != NULL)
    {
        size_t bytes_written = write(file, (char *)current->data, strlen(current->data));
        printf("%ld\n", strlen(current->data));
        if (bytes_written < strlen(current->data))
        {
            printf("Error writing to file.\n");
            return 0; // Error occurred during writing
        }
        total_bytes_written += bytes_written;
        current = current->next;
    }

    printf("Wrote %ld bytes to file.\n", total_bytes_written);
    return total_bytes_written;
}

char *get_permissions(mode_t permissions)
{
    char *perm_str = malloc(10); // Allocate memory for permissions string
    if (!perm_str)
    {
        perror("Memory allocation failed");
        return NULL;
    }
    perm_str[9] = '\0'; // Null-terminate the string

    for (int i = 0; i < 9; ++i)
    {
        switch (i)
        {
        case 0:
            perm_str[i] = (permissions & S_IRUSR) ? 'r' : '-';
            break;
        case 1:
            perm_str[i] = (permissions & S_IWUSR) ? 'w' : '-';
            break;
        case 2:
            perm_str[i] = (permissions & S_IXUSR) ? 'x' : '-';
            break;
        case 3:
            perm_str[i] = (permissions & S_IRGRP) ? 'r' : '-';
            break;
        case 4:
            perm_str[i] = (permissions & S_IWGRP) ? 'w' : '-';
            break;
        case 5:
            perm_str[i] = (permissions & S_IXGRP) ? 'x' : '-';
            break;
        case 6:
            perm_str[i] = (permissions & S_IROTH) ? 'r' : '-';
            break;
        case 7:
            perm_str[i] = (permissions & S_IWOTH) ? 'w' : '-';
            break;
        case 8:
            perm_str[i] = (permissions & S_IXOTH) ? 'x' : '-';
            break;
        default:
            perm_str[i] = '-';
            break; // Fallback, shouldn't occur
        }
    }

    return perm_str;
}

// Function to retrieve file size and permissions
void ss_handle_get_size_and_permissions(int client_sock, char *file_path)
{
    send_to_server(OK, REQUEST_GET_SIZE_PERMISSIONS_STARTED, file_path, -1);
    struct stat file_stat;
    Request request;
    memset(&request, 0, sizeof(request));

    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", root_directory, file_path + 2);

    // Attempt to get file metadata using stat
    if (stat(full_path, &file_stat) == -1)
    {
        perror("Failed to retrieve file information");
        if (errno == ENOENT)
        {
            request.error_code = FILE_NOT_FOUND;
            send_to_server(FILE_NOT_FOUND, REQUEST_GET_SIZE_PERMISSIONS_COMPLETED, file_path, -1);
        }
        else if (errno == EACCES)
        {
            request.error_code = PERMISSION_DENIED;
            send_to_server(PERMISSION_DENIED, REQUEST_GET_SIZE_PERMISSIONS_COMPLETED, file_path, -1);
        }
        else
        {
            request.error_code = STAT_FAILED;
            send_to_server(STAT_FAILED, REQUEST_GET_SIZE_PERMISSIONS_COMPLETED, file_path, -1);
        }
        send(client_sock, &request, sizeof(request), 0);
        return;
    }

    request.error_code = OK;
    snprintf(request.info, sizeof(request.info), "%ld|%s", file_stat.st_size, get_permissions(file_stat.st_mode)); // Store file size in data field

    // Send the file information request to the client
    send(client_sock, &request, sizeof(request), 0);

    memset(&request, 0, sizeof(Request));
    request.type = ACK;
    request.error_code = OK;
    send(client_sock, &request, sizeof(request), 0);
    close(client_sock);
    send_to_server(OK, REQUEST_GET_SIZE_PERMISSIONS_COMPLETED, file_path, -1);

    printf("Sent file size and permissions to client.\n");
}

void ss_handle_stream_audio(int client_sock, char *file_path)
{
    send_to_server(OK, REQUEST_AUDIO_STARTED, file_path, -1);
    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", root_directory, file_path + 2);

    int file = open(full_path, O_RDONLY);
    if (file < 0) {
        // If the file can't be opened, send an error response
        Request error_request = {.error_code = FILE_NOT_FOUND};
        send(client_sock, &error_request, sizeof(error_request), 0);
        send_to_server(FILE_NOT_FOUND, REQUEST_READ_COMPLETED, file_path, -1);
        close(client_sock);
        return;
    }

    char info[MAX_INFO_SIZE];
    ssize_t bytes_read;

    // Read and send the file in chunks
    while ((bytes_read = read(file, info, MAX_INFO_SIZE)) > 0) {
        Request request;
        memset(&request, 0, sizeof(Request));
        request.type = REQ;
        request.error_code = OK;
        memcpy(request.info, info, bytes_read);

        printf("Sending data to client: %.*s\n", (int)bytes_read, info);

        int status = send(client_sock, &request, sizeof(request), 0);

        if (status < 0) {
            perror("Failed to send request");
            close(file);
            close(client_sock);
            return;
        }

        memset(info, 0, sizeof(info)); // Clear data buffer for the next read
    }

    close(file);
    printf("File sent successfully.\n");

    // Notify client that the transfer is complete
    Request completion_request = {.error_code = OK, .type = REQUEST_READ_COMPLETED};
    send(client_sock, &completion_request, sizeof(completion_request), 0);

    close(client_sock);
    send_to_server(OK, REQUEST_AUDIO_COMPLETED, file_path, -1);
    return;
}