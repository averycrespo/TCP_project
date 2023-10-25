#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <dirent.h>
#include <array>

#define PORT 7734

/**
 * Failing function to print to standard output 
 * @param message failure message
*/
static void fail( char const *message ) {
  fprintf( stdout, "%s\n", message );
  exit( 1 );
}

/**
 * Function to find the last portion of a file path 
 * @param path path name
 * @return lastSlashPosition string where pointer is located after the last '/'
*/
const char* extractLastSlash(const char* path) {
    const char* lastSlashPosition = std::strrchr(path, '/');
    return (lastSlashPosition != nullptr && *(lastSlashPosition + 1) != '\0') ? lastSlashPosition + 1 : path;
}

/**
 * Removes the trailing whitespace of a string
 * @param str char array string that has whitespace
*/
void removeWhiteSpace(char *str) {
    if(str == NULL) {
        fail( "NULL STRING.");
    } 

    int len = strlen(str);
    int leadingWhiteSpace = 0;

    while(str[leadingWhiteSpace] == ' ' || str[leadingWhiteSpace] == '\t') {
        leadingWhiteSpace++;
    }

    if(leadingWhiteSpace > 0) {
        for(int i = 0; i <= len - leadingWhiteSpace; i++) {
            str[i] = str[i + leadingWhiteSpace];
        }
    }
}



/**
 * Main function of client
*/
int main() {

    // Create a socket
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        fail("Problem creating socket");
        exit(EXIT_FAILURE);
    }

    // Connect to the server
    struct sockaddr_in serverAddr;
    memset(&serverAddr, '\0', sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons( PORT );
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    if (connect(clientSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        fail("Connection failure. Please retry in a few seconds.");
        close(clientSocket);
        exit(EXIT_FAILURE);
    }

 
    //Getting the path to the RFC 
    char currentPath[256]; 
    if (getcwd(currentPath, sizeof(currentPath)) != nullptr) {
    } else {
        fail("getcwd");
    }
    
    const char* lastPart = extractLastSlash(currentPath);
    char fullPath[20];
    strcpy(fullPath, lastPart);

    //Open directory 
    DIR* dir = opendir(currentPath);
    //Line string for each line of the text file
    char line[75];
    char title[70];
    //Variables for the rfc number and the array that it will be stored in
    int rfc_number_from_document = 0;
    char rfc_number_to_array[16];
    //Title of rfc follows the format where two blank lines proceed title.
    int title_newline_counter = 0;
    char nodeInformationArray[256];

    //Getting OS
    const char* os_command = "uname";
    std::array<char, 64> os_buffer;
    char tempArr[64];
    std::string uname_res;
    FILE* uname_pipe = popen(os_command, "r");
    if( uname_pipe ) {
         while (fgets(os_buffer.data(), os_buffer.size(), uname_pipe) != nullptr) {
            uname_res += os_buffer.data();
        }
        pclose(uname_pipe);
    }
    std::copy(os_buffer.begin(), os_buffer.end(), tempArr);
    //Replace the newline character
    for(int i = 0; tempArr[i] != '\0'; i++) {
        if(tempArr[i] == '\n') {
            tempArr[i] = '\0';
            break;
        }
    }
    
    //Send OS
    send(clientSocket, tempArr, strlen(tempArr), 0);
  
    // Logic to open directory and upload file information. 
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_type == DT_REG && strncmp(entry->d_name, "rfc", 3) == 0 && strstr(entry->d_name, ".txt") != NULL) {
                // std::cout << "File: " << entry->d_name << std::endl;
                FILE *fp = fopen(entry->d_name, "r");
                if(fp) {
                    //Looking for RFC number from file.
                    while(fgets(line, sizeof(line), fp)) {
                        if( strstr(line, "Request for Comments:") != NULL) {
                            if(sscanf(line, "Request for Comments: %d", &rfc_number_from_document) == 1) {
                                break;
                            }
                        }
                    }
                    //Searching for title in .txt file
                    while(fgets(line, sizeof(line), fp)) {
                        if(title_newline_counter == 2)  {
                            // std::cout << "Title: " << line;
                            strcpy(title, line);
                            title_newline_counter = 0;
                            break;
                        }

                        if( strlen(line) == 1 && line[0] == '\n') {
                            title_newline_counter++;
                        } else {
                            title_newline_counter = 0;
                        }
                    }
                   

                } else {
                     fail("Failed to open the file: ");
                }

                //Formatting the string for server send.
                memset(nodeInformationArray, 0, sizeof(nodeInformationArray));
                memset(rfc_number_to_array, 0, sizeof(rfc_number_to_array));
                //Path
                strcat(nodeInformationArray, fullPath);
                strcat(nodeInformationArray, " "); 
                //rfc name
                strcat(nodeInformationArray, entry->d_name); 
                strcat(nodeInformationArray, " "); 
                //Number
                snprintf(rfc_number_to_array, sizeof(rfc_number_to_array), "%d", rfc_number_from_document); 
                strcat(nodeInformationArray, rfc_number_to_array); 
                strcat(nodeInformationArray, " ");
                //Title
                removeWhiteSpace(title);
                title[strlen(title) - 1] = '\0';
                strcat(nodeInformationArray, title); 
                
                if(send(clientSocket, nodeInformationArray, strlen(nodeInformationArray), 0) == -1) {
                    fail("Error uploading rfc.");
                }
                fclose(fp);
            }
        }
        closedir(dir);
    } else {
        fail("Failed to open directory.");
    }

    char hostname[128];
    gethostname(hostname, sizeof(hostname));

    // Communication with the server
    char end[] = "END";
    send(clientSocket, end, strlen(end), 0);
    char buffer[1024];
    char input[512];
    char inputToSend[1024];
    bool OS_flag = false;
    char command[7];


    //Loop for client-server communication 
    while( true ) {
            memset(buffer, 0, sizeof(buffer));
            memset(inputToSend,'\0', sizeof(inputToSend));
            memset(input,'\0', sizeof(input));

        //Loop for input
        for(int i = 0; i < 3; i++) {
            if(i == 0) {
                fgets(input, sizeof(input), stdin);
                sscanf(input, "%s", command);
                // std::cout << "   YOUR COMMAND: '" << command << "'" << std::endl; 
                if(strncmp( "GET ", command, 3) == 0) {
                    OS_flag = true;
                }
            }
            //First line
            if(i == 1) {
                std::cout << "Host: ";
                fgets(input, sizeof(input), stdin);  
            } 
            //Third line and NOT GET call
            if(i == 2 && OS_flag == false) {
                std::cout << "Port: ";
                fgets(input, sizeof(input), stdin);  
            }
            //Third line and GET call
            if( i == 2 && OS_flag == true ) {
                std::cout << "OS: ";
                fgets(input, sizeof(input), stdin);  
                OS_flag = false;
            }
            //Send
            strcat(inputToSend, input); 
            if( i == 2 ) { 
                send(clientSocket , inputToSend, strlen(inputToSend), 0);
                memset(input, '\0', sizeof(input));
            } 
        }

            recv(clientSocket, &buffer, sizeof( buffer ), 0);
            std::cout << buffer << std::endl;
    }


    close(clientSocket);

    return 0;
}