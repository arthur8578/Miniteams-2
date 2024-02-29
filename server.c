#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_CLIENTS 30 //limite de connexion au serveur
#define BUFFER_SIZE 1024 //taille du message
#define NAME_SIZE 32 //taille des pseudo

//tableaux pour stocker les infos de nos clients
int client_socks[MAX_CLIENTS] = {0};
char client_names[MAX_CLIENTS][NAME_SIZE] = {{0}};
int total_clients = 0;

void init_server(int *server_sock, struct sockaddr_in *server_addr, int port) {
    //initialisation du socket seveur
    *server_sock = socket(AF_INET, SOCK_STREAM, 0); //lien de la connexion avec notre socket IPv4 /TCP
    if (*server_sock < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    //socket pour reutiliser l'adresse
    int optval = 1;
    setsockopt(*server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    //infos de l'adresse du serveur
    server_addr->sin_family = AF_INET;  //IPv4
    server_addr->sin_addr.s_addr = INADDR_ANY; //IP server
    server_addr->sin_port = htons(port);  //Port

    //rattacher le socket serveur à l'adresse
    if (bind(*server_sock, (struct sockaddr *)server_addr, sizeof(*server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    //mettre le socket en mode écoute sur le réseau
    if (listen(*server_sock, MAX_CLIENTS) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }
}

//Plaisir 2 conction pour envoyer la liste des clients en ligne sur le serveur
void send_current_users(int client_sock) {
    char message[BUFFER_SIZE] = "Connected users:\n";
    for (int i = 0; i < total_clients; i++) {
        if (client_socks[i] != client_sock) { //n'inclut pas le nouveau client dans la liste
            strcat(message, "- ");
            strcat(message, client_names[i]);      //Structure d'affichage pour les personnes co
            strcat(message, "\n");
        }
    }
    send(client_sock, message, strlen(message), 0);
}

//fonction pour diffuser un message aux clients
void broadcast_message(char *message, int sender_sock) {
    char formatted_message[BUFFER_SIZE + NAME_SIZE]; //augmentation de la taille mémoire pour le nom d'utilisateur avec son message
    int sender_index = -1;

    //index expéditeur pour obtenir son nom
    for (int i = 0; i < total_clients; i++) {
        if (client_socks[i] == sender_sock) {
            sender_index = i;
            break;
        }
    }

    //affichage  du message avec le nom de l'expéditeur
    if (sender_index != -1) {
        snprintf(formatted_message, sizeof(formatted_message), "%s: %s", client_names[sender_index], message);
        for (int i = 0; i < total_clients; i++) {
            if (client_socks[i] != sender_sock) { // Ne pas renvoyer le message à l'expéditeur
                send(client_socks[i], formatted_message, strlen(formatted_message), 0);
            }
        }
    }
}

//fonction pour gérer une nouvelle connexion
void handle_new_connection(int server_sock) {
    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_size);
    if (client_sock >= 0) {
        //reception du nom du client
        char name[NAME_SIZE];
        recv(client_sock, name, NAME_SIZE, 0); //
        strncpy(client_names[total_clients], name, NAME_SIZE);
        client_socks[total_clients++] = client_sock;
        printf("New client connected: %s\n", name);

        //envoie la liste des clients
        send_current_users(client_sock);

        //message d'arrivée du nouveau client à tous les autres clients
        char welcome_message[BUFFER_SIZE];
        snprintf(welcome_message, sizeof(welcome_message), "%s has joined.\n", name);
        broadcast_message(welcome_message, client_sock);
    } else {
        perror("accept failed");
    }
}

//fonction pour gérer un message d'un client
void handle_client_message(int client_sock) {
    char buffer[BUFFER_SIZE];
    int len = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
    if (len > 0) {
        buffer[len] = '\0';
        broadcast_message(buffer, client_sock);
    } else if (len == 0) {
        //Client déconnecté
        int i;
        for (i = 0; i < total_clients; i++) {
            if (client_socks[i] == client_sock) {
                printf("%s disconnected.\n", client_names[i]);
                break;
            }
        }
        //suppressions du client dans les tableaux
        for (; i < total_clients - 1; i++) {
            client_socks[i] = client_socks[i + 1];
            strncpy(client_names[i], client_names[i + 1], NAME_SIZE);
        }
        total_clients--;
        close(client_sock);
    } else {
        perror("recv failed");
    }
}

//fonction principale 
int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <Port>\n", argv[0]);
        return 1;
    }
    //Variables
    int port = atoi(argv[1]);
    int server_sock;
    struct sockaddr_in server_addr;

    //initialisation du serveur sur le port donné
    init_server(&server_sock, &server_addr, port);
    printf("Server reachable on port %d\n", port);

    //boucle principale de notre serveur
    while (1) { //descripteurs
        fd_set read_fds;
        FD_ZERO(&read_fds); 
        FD_SET(server_sock, &read_fds);
        int max_fd = server_sock;
        for (int i = 0; i < total_clients; i++) {
            FD_SET(client_socks[i], &read_fds);
            if (client_socks[i] > max_fd) {
                max_fd = client_socks[i];
            }
        }

        //surveillance des sockets en attente de lecture
        select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        //gestion des nouvelles connexion sur le serveur
        if (FD_ISSET(server_sock, &read_fds)) {
            handle_new_connection(server_sock);
        }

        //gestion des messages des clients déjà présent
        for (int i = 0; i < total_clients; i++) {
            if (FD_ISSET(client_socks[i], &read_fds)) {
                handle_client_message(client_socks[i]);
            }
        }
    }

    return 0;
}
