#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024 //taille limite du message

//etablissemejnt de la connexion au serveur avec les sockets
void connect_to_server(int *sock, const char *server_ip, int port, const char *username) {
    struct sockaddr_in server_addr; 
    *sock = socket(AF_INET, SOCK_STREAM, 0); //lien de la connexion avec notre socket IPv4 /TCP
    if (*sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    //infos de l'adresse du serveur
    server_addr.sin_family = AF_INET;  //IPv4
    server_addr.sin_port = htons(port); //Port
    server_addr.sin_addr.s_addr = inet_addr(server_ip); //IP server

    //conditions d'accès au serveur
    if (connect(*sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        exit(EXIT_FAILURE);
    }
    printf("Connection to server... OK.\n");

    //envoie du nom d'utilisateur 
    send(*sock, username, strlen(username), 0);
}

//fonction pour gérer l'entrée utilisateur et envoyer les messages au serveur
void handle_user_input(int sock) {
    char buffer[BUFFER_SIZE]; //variable qui contient le message 
    printf("Send a new message: ");
    while (fgets(buffer, BUFFER_SIZE, stdin) != NULL) {
        if (send(sock, buffer, strlen(buffer), 0) < 0) {
            perror("Failed to send message");
            exit(EXIT_FAILURE);
        }
        printf("Send a new message: ");
    }
}

//fonction qui permet d'afficher les infos envoyer par le serveur
void handle_server_messages(int sock) {
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); //buffer qui permet d'afficher les nouveaux messages
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; 
            printf("\n %s\n", buffer);
            printf("Send a new message: ");
            fflush(stdout);
        } else if (bytes_received == 0) { //si jamais le serveur et couper
            printf("Server connection closed.\n");
            exit(EXIT_SUCCESS);
        } else {
            perror("Failed to receive message");
            exit(EXIT_FAILURE);
        }
    }
}

//fonction principale qui récupère les infos pour se co sur le serveur
int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <Server IP> <Port> <Username>\n", argv[0]);
        return 1;
    }

    int sock;
    connect_to_server(&sock, argv[1], atoi(argv[2]), argv[3]);

    pid_t pid = fork();
    if (pid == 0) { //processus entrée d'un utilisateur
        handle_user_input(sock);
    } else if (pid > 0) { //processus les messages du serveur
        handle_server_messages(sock);
    } else {
        perror("Failed to create process");
        return EXIT_FAILURE;
    }

    close(sock);
    return 0;
}
