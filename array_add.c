#include<stdio.h>
#include<string.h>    // for strlen
#include<stdlib.h>
#include<sys/socket.h>
#include<arpa/inet.h> // for inet_addr
#include<unistd.h>    // for write
#include<pthread.h>   // for threading, link with lpthread

#define DATA_SIZE 100
#define PORT_NUMBER 60000


    char input_string[DATA_SIZE];
    int first_array[DATA_SIZE];
    int second_array[DATA_SIZE];
    int result_array[DATA_SIZE];
    int carry_array[DATA_SIZE];
    int first_index_carry=0;

// Controling the chars 
int is_valid_char(const char c) {
     return (c >= '0' && c <= '9') || c == '\r' || c == '\n';
}

// Controling the mutlipe space 
void replace_multi_space_with_single_space(char *str) {
    char *dest = str;

    while (*str != '\0') {
        while (*str == ' ' && *(str + 1) == ' ')
            str++;

        *dest++ = *str++;
    }

    *dest = '\0';
}


int str_to_int_array(const char *str, int *array, int client_socket) {
    int count = 0;
    char modified_str[DATA_SIZE];  // Adjust the size as needed

    // Making a copy of the original string with replaced multiple spaces
    strcpy(modified_str, str);
    replace_multi_space_with_single_space(modified_str);

    char *token = strtok(modified_str, " ");

    while (token != NULL) {
        char *message;

        // Checking if the token contains only valid characters
        for (int i = 0; token[i] != '\0'; i++) {
            if (!is_valid_char(token[i])) {
                message = "ERROR: The inputted integer array contains non-integer characters. You must input only integers and empty spaces to separate inputted integers!\n";
                send(client_socket, message, strlen(message), 0);
                close(client_socket);
            }
        }

        // Converting the token to an integer and check if it's greater than 999
        int current_value = atoi(token);
        if (current_value > 999) {
            message = "ERROR: Integer greater than 999. You must send integers that are not bigger than 999!\n";
            send(client_socket, message, strlen(message), 0);
            close(client_socket);
        }
        // WE used MSB method
        array[count++] = current_value;
        token = strtok(NULL, " ");
    }

    return count;
}


void *add_arrays(void *arg) {
    int index = *((int *)arg);

    // Performing addition for the given index
    result_array[index+1] = first_array[index] + second_array[index] ;

    // Checking for carry
    if (result_array[index+1] > 999) {
        carry_array[index] = 1;
        result_array[index+1] %= 1000;
    } 
    else{// There is no any carry
        carry_array[index ] = 0;
    }

    free(arg);
    pthread_exit(NULL);
}
 

void handle_client(int client_socket) {
    
    char *message;

    message = "Hello, this is Array Addition Server!\n";
    write(client_socket, message, strlen(message));
    
    message = "Please enter the first array for addition:(MSB)\n";
    write(client_socket, message, strlen(message));

    // Receiving the first integer array
    int total_received = 0;
    while (1) {
        ssize_t received = recv(client_socket, input_string + total_received, sizeof(input_string) - total_received - 1, 0);
        if (received <= 0) {
            // Handling error and connection closed by the client
            message = "ERROR: Input data size exceeds the limit.\n";
            send(client_socket, message, strlen(message), 0);
            close(client_socket);
            return;
        }

        total_received += received;
        
        // Checking if the received data contains a complete line
        if (memchr(input_string, '\n', total_received) != NULL) {
            break;
        }
    }

    input_string[total_received] = '\0'; // Null-terminate the received data

    printf("Received data from client: %s\n", input_string);
    int size_first_array = str_to_int_array(input_string, first_array, client_socket);


    message = "Please enter the second array for addition:(MSB)\n";
    write(client_socket, message, strlen(message));

    // Receiving the second integer array
    total_received = 0;
    while (1) {
        ssize_t received = recv(client_socket, input_string + total_received, sizeof(input_string) - total_received - 1, 0);
        if (received <= 0) {
            // Handling error and connection closed by the client
            message = "ERROR: Input data size exceeds the limit.\n";
            send(client_socket, message, strlen(message), 0);
            close(client_socket);
            return;
        }

        total_received += received;

        // Checking if the received data contains a complete line
        if (memchr(input_string, '\n', total_received) != NULL) {
            break;
        }
    }

    input_string[total_received] = '\0'; // Null-terminate the received data

    printf("Received data from client: %s\n", input_string);
    int size_second_array = str_to_int_array(input_string, second_array, client_socket);


    printf("Size of the second array: %d\n", size_second_array); 
    
    // Controling the size of the arrays; must be equl
    if (size_first_array != size_second_array) {
        message = "ERROR: The number of integers are different for both arrays. You must send equal number of integers for both arrays!\n";
        send(client_socket, message, strlen(message), 0);
        close(client_socket);
        return;
    }

    // Determining the number of threads needed
    int num_threads = size_first_array > size_second_array ? size_first_array : size_second_array;

    // Creating threads for each elementte
    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; i++) {
        int *arg = malloc(sizeof(int));
        *arg = i;
        pthread_create(&threads[i], NULL, add_arrays, arg);
    }

    // Waiting for all threads to finish
    for (int i = 0; i < num_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    // Adding the carries to the result array
    for (int i = num_threads; i >= 0; i--) {
        result_array[i] += carry_array[i];
        if (i!=0 && result_array[i] > 999) {
            carry_array[i-1] += 1;
            result_array[i] %= 1000;
        } 
    }
    for(int i = DATA_SIZE; i >= 0; i--) {
        carry_array[i] = 0;
    }

    // Transforming the result array to a string and deleting the unnecessary zeros
    memset(input_string, 0, sizeof(input_string));
    for (int i = 0; i < num_threads+1; i++) {
        char temp_str[4];
        sprintf(temp_str, "%d", result_array[i]);
        strcat(input_string, temp_str);
            if (i < num_threads ) {
            strcat(input_string, " ");
        }
    }
    
    // Sending the result to the client
    message = "The result of array addition are given below:\n";
    write(client_socket, message, strlen(message));
    send(client_socket, input_string, sizeof(input_string), 0);

    message = "\nThank you for Array Addition Server! Good Bye!\n";
    write(client_socket, message, strlen(message));

    for(int i = DATA_SIZE; i >= 0; i--) {
        result_array[i] = 0;
    }

    // Closing the connection
    close(client_socket);
}

int main(int argc, char *argv[])
{
    int socket_desc, new_socket, c, *new_sock;
    struct sockaddr_in server, client;
    char *message;
     
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        puts("Could not create socket");
        return 1;
    }
     
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT_NUMBER);
     
    if(bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        puts("Binding failed");
        return 1;
    }
     
    listen(socket_desc, 3);
     
    puts("Waiting for incoming connections...");
    

    c = sizeof(struct sockaddr_in);

    while ((new_socket = accept(socket_desc, (struct sockaddr *)&client, (socklen_t *)&c))) {
        puts("Connection accepted");

        // Handling the client
        handle_client(new_socket);

        puts("Handler assigned");
    }

    if (new_socket < 0) {
        perror("Accept failed");
        return 1;
    }

    return 0;

   

    return 0;
}