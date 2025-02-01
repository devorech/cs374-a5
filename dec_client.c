#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()
#include <arpa/inet.h>  // inet_pton()

#define LOCALHOST "127.0.0.1"

/*
* Client code
* 1. Create a socket and connect to the decryption server.
* 2. Sends the verification, key, and message to the server, and recieves the decrypted message back.
*/

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
        fprintf(stderr, "DEC_CLIENT: Invalid address, address not supported.\n"); 
        exit(1); 
    }
}

int main(int argc, char *argv[]) {
    int socketFD, portNumber, charsWritten, charsRead;
    struct sockaddr_in serverAddress;
    char buffer[100000];
    // Check usage & args
    if (argc < 3) { 
        fprintf(stderr,"USAGE: %s hostname port\n", argv[0]); 
        exit(0); 
    } 

    // Get input message from user
    FILE *ciphertext = fopen(argv[1], "r");
    // Clear out the buffer array
    memset(buffer, '\0', sizeof(buffer));
    // Get input from the user, trunc to buffer - 1 chars, leaving \0
    int input_length = fread(buffer, sizeof(char), sizeof(buffer), ciphertext);
    buffer[strcspn(buffer, "\n")] = '|';  // Remove the trailing \n that fgets adds and replace with | (terminating char)
    input_length--;
    fclose(ciphertext);

    // Get the key out of the key file and make sure it's equal to or longer than the message
    FILE *key_file = fopen(argv[2], "r");
    char key[100000];
    int key_length = fread(key, sizeof(char), sizeof(key), key_file);
    key[strcspn(key, "\n")] = '|';  // Remove the trailing \n that fgets adds
    key_length--;
    fclose(key_file);
    //printf("input is %d, key is %d\n", input_length, key_length);
    if (key_length < input_length)
    {
        fprintf(stderr, "DEC_CLIENT: Key length is too short\n");
        exit(1);
    }

    // Output error and exit if the ciphertext or key has ANY invalid characters
    char invalid_chars[50] = "`1234567890-=[]\\;',./~!@#$%^&*()_+{}:\"<>?";
    if (strcspn(buffer, invalid_chars) != strlen(buffer) || 
        strcspn(key, invalid_chars) != strlen(key))
    {
        fprintf(stderr, "DEC_CLIENT: file \"%s\" contains invalid characters\n", argv[1]);
        exit(1); 
    }

    // Create a socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0); 
    if (socketFD < 0){
        fprintf(stderr, "DEC_CLIENT: ERROR opening socket\n");
        exit(1);
    }

    // Set up the server address struct
    setupAddressStruct(&serverAddress, atoi(argv[3]));

    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0){
        fprintf(stderr, "DEC_CLIENT: ERROR connecting\n");
        exit(1);
    }

    // Inform the server that dec_client is trying to connect (this is valid)
    int verify = send(socketFD, "dec_client|", 11, 0);

    // Send the ciphertext message to server
    // Write to the server (if the full message isn't send, keep sending fragments until it is fully sent over)
    int chars_written = 0;
    int total_written = 0;
    size_t message_length = strlen(buffer);
    while (total_written < message_length)
    {
        // Write the amount of characters that are left to write
        chars_written = send(socketFD, buffer + total_written, message_length - total_written, 0);
        if (chars_written < 0) {
            fprintf(stderr, "DEC_CLIENT: ERROR writing to socket\n");
            exit(1);
        }
        total_written += chars_written;
    }

    // Send the key to the server
    chars_written, total_written = 0;
    key_length = strlen(key);
    while (total_written < key_length)
    {
        // Write the amount of characters that are left to write
        chars_written = send(socketFD, key + total_written, key_length - total_written, 0);
        if (chars_written < 0) {
            fprintf(stderr, "DEC_CLIENT: ERROR writing to socket\n");
            exit(1);
        }
        total_written += chars_written;
    }

    // Get return message from server
    // Clear out the buffer again for reuse
    memset(buffer, '\0', sizeof(buffer));
    // Read data from the socket, leaving \0 at end
    charsRead = recv(socketFD, buffer, sizeof(buffer) - 1, 0); 
    if (charsRead < 0){
        fprintf(stderr, "DEC_CLIENT: ERROR reading from socket using port %s\n", argv[3]);
        exit(2);
    }
    printf("%s\n", buffer);

    // Close the socket
    close(socketFD); 
    return 0;
}