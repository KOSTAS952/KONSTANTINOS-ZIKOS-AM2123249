SERVER.c
  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <pthread.h>

#define PORT 8080
#define PRODUCT_COUNT 20
#define CLIENT_COUNT 5
#define MAX_BUFFER 1024

// Δομή για προϊόντα
typedef struct {
    char description[50];
    float price;
    int item_count;
    int request_count;
    int sold_count;
    char failed_users[CLIENT_COUNT][50];
    int failed_count;
} Product;

Product catalog[PRODUCT_COUNT];
int total_orders = 0, successful_orders = 0, failed_orders = 0;
float total_revenue = 0.0;

// Αρχικοποίηση προϊόντων
void initialize_catalog() {
    for (int i = 0; i < PRODUCT_COUNT; i++) {
        snprintf(catalog[i].description, 50, "Product_%d", i);
        catalog[i].price = (i + 1) * 10.0;
        catalog[i].item_count = 2;
        catalog[i].request_count = 0;
        catalog[i].sold_count = 0;
        catalog[i].failed_count = 0;
    }
}

// Επεξεργασία παραγγελίας
void process_order(int client_socket, char *client_name) {
    int product_id;
    read(client_socket, &product_id, sizeof(int));
    product_id %= PRODUCT_COUNT; // Διασφάλιση έγκυρου ID

    catalog[product_id].request_count++;
    if (catalog[product_id].item_count > 0) {
        // Επιτυχής παραγγελία
        catalog[product_id].item_count--;
        catalog[product_id].sold_count++;
        total_revenue += catalog[product_id].price;
        successful_orders++;

        char response[MAX_BUFFER];
        snprintf(response, MAX_BUFFER, "Order successful: %s, Total: $%.2f\n", 
                 catalog[product_id].description, catalog[product_id].price);
        write(client_socket, response, strlen(response) + 1);
    } else {
        // Αποτυχημένη παραγγελία
        failed_orders++;
        snprintf(catalog[product_id].failed_users[catalog[product_id].failed_count], 50, "%s", client_name);
        catalog[product_id].failed_count++;

        char response[MAX_BUFFER];
        snprintf(response, MAX_BUFFER, "Order failed: %s is out of stock\n", catalog[product_id].description);
        write(client_socket, response, strlen(response) + 1);
    }

    total_orders++;
    sleep(1); // Χρόνος διεκπεραίωσης
}

// Αναφορά
void generate_report() {
    printf("\n--- Eshop Report ---\n");
    for (int i = 0; i < PRODUCT_COUNT; i++) {
        printf("Product: %s\n", catalog[i].description);
        printf("  Requests: %d\n", catalog[i].request_count);
        printf("  Sold: %d\n", catalog[i].sold_count);
        printf("  Failed Users: ");
        for (int j = 0; j < catalog[i].failed_count; j++) {
            printf("%s ", catalog[i].failed_users[j]);
        }
        printf("\n");
    }
    printf("\nSummary:\n");
    printf("  Total Orders: %d\n", total_orders);
    printf("  Successful Orders: %d\n", successful_orders);
    printf("  Failed Orders: %d\n", failed_orders);
    printf("  Total Revenue: $%.2f\n", total_revenue);
}

// Server λειτουργία
void run_server() {
    initialize_catalog();

    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Δημιουργία socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Ρύθμιση διεύθυνσης
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, CLIENT_COUNT) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Eshop is running...\n");

    for (int i = 0; i < CLIENT_COUNT; i++) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        char client_name[50];
        snprintf(client_name, 50, "Client_%d", i + 1);

        if (fork() == 0) { // Child process
            for (int j = 0; j < 10; j++) {
                process_order(client_socket, client_name);
            }
            close(client_socket);
            exit(0);
        }
        close(client_socket);
    }

    // Αναμονή για όλους τους πελάτες
    while (wait(NULL) > 0);
    generate_report();

    close(server_fd);
}


CLIENT.c

  // Client λειτουργία
void run_client(int client_id) {
    int sock = 0;
    struct sockaddr_in serv_addr;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("Invalid address\n");
        return;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection failed\n");
        return;
    }

    char buffer[MAX_BUFFER] = {0};
    for (int i = 0; i < 10; i++) {
        int product_id = rand() % 20;
        write(sock, &product_id, sizeof(int));

        read(sock, buffer, MAX_BUFFER);
        printf("Client %d: %s", client_id, buffer);
        sleep(1);
    }

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <server|client>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        run_server();
    } else if (strcmp(argv[1], "client") == 0) {
        for (int i = 0; i < CLIENT_COUNT; i++) {
            if (fork() == 0) {
                run_client(i + 1);
                exit(0);
            }
        }

        while (wait(NULL) > 0); // Αναμονή για όλους τους πελάτες
    } else {
        printf("Invalid argument. Use 'server' or 'client'.\n");
    }

    return 0;
}
