#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
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
        // Now we should have the correct numbers
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

int main(int argc, char *argv[]){
    int connectionSocket, charsRead;
    char buffer[1000];
    char key[1000];
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
        error("ERROR opening socket");
    }

    // Set up the address struct for the server socket
    setupAddressStruct(&serverAddress, atoi(argv[1]));

    // Associate the socket to the port
    if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0){
        error("ERROR on binding");
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
            error("ERROR on accept");
        }

        // Verify that the connection came from enc_client
        char clientName[11];
        memset(clientName, '\0', sizeof(clientName));
        charsRead = recv(connectionSocket, clientName, sizeof(clientName), 0);
        clientName[strcspn(clientName, "|")] = '\0';
        printf("Clients name is: %s\n", clientName);
        if (strcmp(clientName, "enc_client") != 0)
        {
            error("ERROR verifying client (must be \"enc_client\")");
        }

        printf("SERVER: Connected to client running at host %d port %d\n", 
                            ntohs(clientAddress.sin_addr.s_addr),
                            ntohs(clientAddress.sin_port));

        // Get the message from the client and display it
        memset(buffer, '\0', sizeof(buffer));
        while (strstr(buffer, "|") == NULL)
        {
            char tempBuff[1000];
            memset(tempBuff, '\0', sizeof(tempBuff));
            charsRead = recv(connectionSocket, tempBuff, sizeof(tempBuff) - 1, 0);
            if (charsRead < 0){
                error("ERROR reading from socket");
            }
            tempBuff[charsRead] = '\0'; // need '\0' to concatinate properly
            strcat(buffer, tempBuff);
        }

        // Remove the terminating character '|'
        //buffer[strcspn(buffer, "|")] = '\0';

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
            char tempBuff[1000];
            memset(tempBuff, '\0', sizeof(tempBuff));
            charsRead = recv(connectionSocket, tempBuff, sizeof(tempBuff) - 1, 0);
            if (charsRead < 0){
                error("ERROR reading from socket");
            }
            if (charsRead == 0) {
                printf("client socket closed\n");
            }
            tempBuff[charsRead] = '\0'; // need '\0' to concatinate properly
            strcat(key, tempBuff);
        }

        // Remove the terminating '|'
        key[strcspn(key, "|")] = '\0';
        // Server prints out messages for debugging to show that message and key is correctly transfered
        //printf("SERVER: I received this message from the client: \"%s\"\n", buffer);
        //printf("SERVER: I received this key from the client: \"%s\"\n", key);

        // Encrypt the message
        char* encrypted_message = encrypt(buffer, key);
        
        // Send a Success message back to the client
        charsRead = send(connectionSocket, 
                        encrypted_message, strlen(encrypted_message), 0); 
        if (charsRead < 0){
            error("ERROR writing to socket");
        }
        // Close the connection socket and free the encrypted message for this client
        free(encrypted_message);
        close(connectionSocket);
    }
    // Close the listening socket
    close(listenSocket); 
    return 0;
}
