#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>

// Error function used for reporting issues
void error(const char *msg) {
    perror(msg);
    exit(1);
} 

// Set up the address struct for the server socket
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber){
 
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address)); 

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    // Allow a client at any address to connect to this server
    address->sin_addr.s_addr = INADDR_ANY;
}

// Encrypt the string that is passed in according to the key
// Key should be at least equal or longer in length than plaintext
char* encrypt(char* plaintext, char* key)
{
    size_t pt_len = strlen(plaintext);
    char* ciphertext = calloc(pt_len, sizeof(char));
    for (int i = 0; i < pt_len; i++)
    {
        // Get the corresponding "value" that will be converted to the encrypted character
        // A-Z = 0 - 25, <space> = 26
        int pt_val;
        if (plaintext[i] == ' ')
        {
            pt_val = 26; // 26 = space (' ')
        }
        else
        {
            pt_val = plaintext[i] - 'A';
        }
        // Check if the key has a space to handle it correctly
        int k_val;
        if (key[i] == ' ')
        {
            k_val = 26;
        }
        else
        {
            k_val = key[i] - 'A';
        }
        // Result = resulting character (A-Z or ' ') after encryption
        int result = (pt_val + k_val) % 27;
        if (result == 26)
        {
            ciphertext[i] = ' ';
        }
        else
        {
            ciphertext[i] = result + 'A';
        }
    }
    return ciphertext;
}

/*
* Main program that handles socket and reading/writing to socket(s) with client(s)
*/
int main(int argc, char *argv[]){
    int connectionSocket, charsRead;
    char buffer[100000];
    char key[100000];
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t sizeOfClientInfo = sizeof(clientAddress);

    // Check usage & args
    if (argc < 2) { 
        fprintf(stderr,"USAGE: %s port\n", argv[0]); 
        exit(1);
    } 
    
    // Create the socket that will listen for connections
    int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listenSocket < 0) {
        error("ENC_SERVER: ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
        error("ENC_SERVER: ERROR on binding");
    }

    // Start listening for connetions. Allow up to 5 connections to queue up
    listen(listenSocket, 5); 
    
    // Start accepting connections (max of 5 at a time),
    // blocking if one is not available until one connects
    while(1)
    {
        // Accept the connection request which creates a connection socket
        connectionSocket = accept(listenSocket, 
                    (struct sockaddr *)&clientAddress, 
                    &sizeOfClientInfo); 
        if (connectionSocket < 0) {
            fprintf(stderr, "ENC_SERVER: ERROR on accept");
            close(connectionSocket);
            continue;
        }

        pid_t spawn_pid = fork();
        if (spawn_pid == -1) {
            // Error: Can't fork
            fprintf(stderr, "ENC_SERVER: fork() failed for this connection!");
            close(connectionSocket);
            continue;
        }
        else if (spawn_pid == 0)
        {
            /* Child */

            // Verify that the connection came from enc_client
            // Only proceed if enc_client is trying to connect (exit if it is any other client)
            char clientName[11];
            memset(clientName, '\0', sizeof(clientName));
            charsRead = recv(connectionSocket, clientName, sizeof(clientName), 0);
            clientName[strcspn(clientName, "|")] = '\0';
            if (strcmp(clientName, "enc_client") != 0)
            {
                fprintf(stderr, "ENC_SERVER: ERROR verifying client (must be \"enc_client\")\n");
                close(connectionSocket);
                exit(1);
            }

            // Get the message from the client and display it
            memset(buffer, '\0', sizeof(buffer));
            while (strstr(buffer, "|") == NULL)
            {
                char tempBuff[100000];
                memset(tempBuff, '\0', sizeof(tempBuff));
                charsRead = recv(connectionSocket, tempBuff, sizeof(tempBuff) - 1, 0);
                if (charsRead < 0){
                    perror("ENC_SERVER: ERROR reading from socket");
                    close(connectionSocket);
                    exit(1);
                }
                tempBuff[charsRead] = '\0'; // need '\0' to concatinate properly
                strcat(buffer, tempBuff);
            }

            // Fill out the key buffer it has already been sent
            memset(key, '\0', sizeof(key));
            char* remaining_info = strstr(buffer, "|");
            if (strlen(remaining_info) > 1)
            {
                strcat(key, remaining_info + 1);
            }
            buffer[remaining_info - buffer] = '\0';
            // Keep reading from the socket if the key has not yet been fully sent through
            while (strstr(key, "|") == NULL)
            {
                char tempBuff[100000];
                memset(tempBuff, '\0', sizeof(tempBuff));
                charsRead = recv(connectionSocket, tempBuff, sizeof(tempBuff) - 1, 0);
                if (charsRead < 0){
                    perror("ENC_SERVER: ERROR reading from socket");
                    close(connectionSocket);
                    exit(1);
                }
                if (charsRead == 0) {
                    printf("client socket closed\n");
                }
                tempBuff[charsRead] = '\0'; // need '\0' to concatinate properly
                strcat(key, tempBuff);
            }

            // Remove the terminating '|'
            key[strcspn(key, "|")] = '\0';

            // Encrypt the message
            char* encrypted_message = encrypt(buffer, key);
            
            // Send a Success message back to the client
            charsRead = send(connectionSocket, 
                            encrypted_message, strlen(encrypted_message), 0); 
            if (charsRead < 0){
                perror("ENC_SERVER: ERROR writing to socket");
                close(connectionSocket);
                exit(1);
            }
            // Close the connection socket and free the encrypted message for this client
            free(encrypted_message);

            close(connectionSocket);
            exit(0);
        }
        else 
        {
            /* Parent */
            close(connectionSocket); // close in case

            int child_status; 
            pid_t terminated_child;
            // Check if there are any child processes that need to be fully terminated
            while ((terminated_child = waitpid(-1, &child_status, WNOHANG)) > 0) {}
        }
    }
    // Close the listening socket
    close(listenSocket); 
    return 0;
}