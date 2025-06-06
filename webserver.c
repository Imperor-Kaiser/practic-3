#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>

#define PORT 8080
#define BUFFER_SIZE 4096 

void send_404_response(int client_fd) {
    const char* response =
        "HTTP/1.1 404 Not Found\r\n"
        "Content-Type: text/html; charset=utf-8\r\n\r\n"
        "<html><body><h1>404 Not Found</h1></body></html>";
    send(client_fd, response, strlen(response), 0);
}

void send_image_response(int client_fd, const char* message) {
    FILE* fp = fopen("mirea.png", "rb");
    if (!fp) {
        send_404_response(client_fd);
        return;
    }

    struct stat file_stat;
    stat("mirea.png", &file_stat);
    int img_size = file_stat.st_size;

    char header[BUFFER_SIZE];              

    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html; charset=utf-8\r\n\r\n"
             "<html><body>"
             "<h1>%s</h1>"
             "<img src='data:image/png;base64,", message);
    send(client_fd, header, strlen(header), 0);
    unsigned char* img_buffer = malloc(img_size);
    if (!img_buffer) {
        fclose(fp);
        send_404_response(client_fd);
        return;
    }
    fread(img_buffer, 1, img_size, fp);р
    fclose(fp);

    const char* b64_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";  

    for (int i = 0; i < img_size; i += 3) {
        unsigned char triplet[3] = {0, 0, 0};
        int segment_length = (img_size - i >= 3) ? 3 : (img_size - i);
        memcpy(triplet, img_buffer + i, segment_length);

        char encoded[5];
        encoded[0] = b64_table[(triplet[0] >> 2) & 0x3F];  
        encoded[1] = b64_table[((triplet[0] & 0x03) << 4) | ((triplet[1] >> 4) & 0x0F)];  
        encoded[2] = (segment_length > 1) ? b64_table[((triplet[1] & 0x0F) << 2) | ((triplet[2] >> 6) & 0x03)] : '=';  
        encoded[3] = (segment_length > 2) ? b64_table[triplet[2] & 0x3F] : '=';  
        encoded[4] = '\0';

        send(client_fd, encoded, 4, 0);
    }
    free(img_buffer);

    const char* footer = "'/></body></html>";  
    send(client_fd, footer, strlen(footer), 0);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    char buffer[BUFFER_SIZE];
    socklen_t addr_len = sizeof(client_addr);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Ошибка создания сокета");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT); 
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Ошибка привязки сокета");
        exit(1);
    }

    listen(server_fd, 10);
    printf("Сервер запущен на порту %d...\n", PORT);
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Ошибка accept");
            continue;
        }

        memset(buffer, 0, sizeof(buffer));
        read(client_fd, buffer, sizeof(buffer) - 1);

        char* query = strstr(buffer, "GET /?message=");
        if (query) {
            char* msg_start = query + strlen("GET /?message=");
            char* msg_end = strchr(msg_start, ' ');
            if (msg_end)
                *msg_end = '\0';
            for (char* p = msg_start; *p; ++p)
                if (*p == '+')
                    *p = ' ';

            send_image_response(client_fd, msg_start);
        } else {
            send_404_response(client_fd);
        }
        close(client_fd);
    }
    close(server_fd);
    return 0;
}
