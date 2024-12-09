#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()
#include <arpa/inet.h>  // inet_pton()

#define LOCALHOST "127.0.0.1"

/**
* Client code
* 1. Create a socket and connect to the server specified in the command arugments.
* 2. Prompt the user for input and send that input as a message to the server.
* 3. Print the message received from the server and exit the program.
*/

// Error function used for reporting issues
void error(const char *msg) { 
    perror(msg); 
    exit(0); 
} 

// Set up the address struct
void setupAddressStruct(struct sockaddr_in* address, int portNumber)
{
    // Clear out the address struct
    memset((char*) address, '\0', sizeof(*address)); 

    // The address should be network capable
    address->sin_family = AF_INET;
    // Store the port number
    address->sin_port = htons(portNumber);
    //printf("\nport is: %d\n", portNumber);

    // Convert IP address from text to binary (Citation: Adapted from lecture #13 slides - Sockets)
    if (inet_pton(AF_INET, LOCALHOST, &address->sin_addr) <= 0) {
        error("Invalid address, address not supported.\n");
    }
    /*
    // Get the DNS entry for this host name
    struct hostent* hostInfo = gethostbyname(hostname); 
    if (hostInfo == NULL) { 
        fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
        exit(0); 
    }
    // Copy the first IP address from the DNS entry to sin_addr.s_addr
    memcpy((char*) &address->sin_addr.s_addr, 
            hostInfo->h_addr_list[0],
            hostInfo->h_length); 
    */
}

int main(int argc, char *argv[]) {
    int socketFD, portNumber, charsWritten, charsRead;
    struct sockaddr_in serverAddress;
    char buffer[1000];
    // Check usage & args
    if (argc < 3) { 
        fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); 
        exit(0); 
    } 

    // Get input message from user
    FILE *plaintext = fopen(argv[1], "r");
    // Clear out the buffer array
    memset(buffer, '\0', sizeof(buffer));
    // Get input from the user, trunc to buffer - 1 chars, leaving \0
    int input_length = fread(buffer, sizeof(char), sizeof(buffer), plaintext);
    buffer[strcspn(buffer, "\n")] = '|';  // Remove the trailing \n that fgets adds and replace with | (terminating char)
    input_length--;
    fclose(plaintext);

    // Get the key out of the key file and make sure it's equal to or longer than the message
    FILE *key_file = fopen(argv[2], "r");
    char key[1000];
    int key_length = fread(key, sizeof(char), sizeof(key), key_file);
    key[strcspn(key, "\n")] = '|';  // Remove the trailing \n that fgets adds
    key_length--;
    fclose(key_file);
    //printf("input is %d, key is %d\n", input_length, key_length);
    if (key_length < input_length)
    {
        error("CLIENT: Key length is too short");
    }

    // Create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0); 
    if (socketFD < 0){
        error("CLIENT: ERROR opening socket");
    }

    // Set up the server address struct
    setupAddressStruct(&serverAddress, atoi(argv[3]));

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        error("CLIENT: ERROR connecting");
    }

    // Inform the user that enc_client is trying to connect (enc_server should only
    // proceed if it can verify that it is the enc_client, not dec_client, trying to connect)
    int verify = send(socketFD, "enc_client|", 11, 0);

    // Send the plaintext message to server
    // Write to the server (if the full message isn't send, keep sending fragments until it is fully sent over)
    int chars_written = 0;
    int total_written = 0;
    size_t message_length = strlen(buffer);
    while (total_written < message_length)
    {
        // Write the amount of characters that are left to write
        chars_written = send(socketFD, buffer + total_written, message_length - total_written, 0);
        if (chars_written < 0) {
            error("CLIENT: ERROR writing to socket");
        }
        total_written += chars_written;
        if (total_written < message_length){
            printf("CLIENT: Not all message written to socket yet!\n");
        }
    }

    // Send the key to the server
    chars_written, total_written = 0;
    key_length = strlen(key);
    while (total_written < key_length)
    {
        // Write the amount of characters that are left to write
        chars_written = send(socketFD, key + total_written, key_length - total_written, 0);
        if (chars_written < 0) {
            error("CLIENT: ERROR writing to socket");
        }
        total_written += chars_written;
        if (total_written < key_length){
            printf("CLIENT: Not all key written to socket yet!\n");
        }
    }

    // Get return message from server
    // Clear out the buffer again for reuse
    memset(buffer, '\0', sizeof(buffer));
    // Read data from the socket, leaving \0 at end
    charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0); 
    if (charsRead < 0){
        error("CLIENT: ERROR reading from socket");
    }
    printf("%s\n", buffer);

    // Close the socket
    close(socketFD); 
    return 0;
}