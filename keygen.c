/*
* Name: Christian DeVore
* Description: Generates a key of specified lenght containing all alphabet 
* characters (A-Z), including spaces (" ").
*/

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) 
{
    // Make sure at least one argument is passed through
    if (argc != 2)
    {
        fprintf(stderr, "Error: Please enter the length of the key you would like provided.\n");
        return EXIT_FAILURE;
    }

    // Initialize random number seed
    srand(time(0));

    // Generate the random key
    int key_length = atoi(argv[1]);
    char* key = malloc(key_length * sizeof(char));
    for(int i = 0; i < key_length; i++)
    {
        // Create random number between 0-26 (to be converted to characters later) 
        int num = rand() % 27;
        if (num == 26)
        {
            // This becomes a space
            key[i] = ' ';
        }
        else
        {
            // Converts the number to an A-Z char using ASCII
            key[i] = (char)(num + 'A');
        }
    }
    printf("%s\n", key);

    return 0;
}