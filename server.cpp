#include <iostream>
#include <cstdlib>
#include <fstream>
#include <cstring>
#include <cerrno>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <string>
#include <netdb.h>
#include <vector> 
#include <sys/stat.h>
#include <ctime>

#define PORT 7734

/**
 * Failing function to print to standard output 
*/
static void fail( char const *message ) {
  fprintf( stdout, "%s\n", message );
  exit( 1 );
}

//Structure for Client Node
struct Client_Node {
    char hostname[254];
    int port_number;
    char os_string[32];
    struct Client_Node *next;
};

//Structure for RFC Node
struct RFC_Node {
    int rfc_number;
    char title[80];
    char hostname[254];
    int port_number;
    char path[20];
    struct RFC_Node *next;
};

// Creation of global linked lists
Client_Node* client_list = NULL;
RFC_Node* rfc_list = NULL;

/** Mutex lock for all threads accessing the board */
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

/** Threads */
pthread_t userLock;

/**
 * Easy function to create client node
 * @param hostname name of the host 
 * @param port integer value of the port that the client is connected to
 * @return ClientNode new client node object
*/
Client_Node* createClientNode( char *hostname, int port, char *os_string ) {
  Client_Node* newNode = (Client_Node *)malloc(sizeof(Client_Node));
  if( newNode == NULL ) {
    fail("MALLOC call failed - Client node");
  }
  strcpy(newNode->hostname, hostname);
  newNode->port_number = port;
  strcpy(newNode->os_string, os_string);
  newNode->next = NULL;
  return newNode;
}

/**
 * Function to add Client node to client linked list
 * @param head head of the linked list 
 * @param newNode newNodeto be added to the linked list 
*/
void addClientNode( Client_Node** head, Client_Node* newNode ) {
  newNode->next = *head;
  *head = newNode;
}

/**
 * Once TCP client disconnects we must remove all instances regarding his port
 * There will only be one instance of his port in this linked list
 * @param port_toRemove integer value compared with all nodes in client_list 
*/
void deleteClientNode(int port_toRemove) {
    Client_Node* current = client_list;
    Client_Node * previous = NULL;
    while( current != NULL ) {
      if(current->port_number == port_toRemove) {
        if(previous == NULL) {
          client_list = current->next;
        } else {
          previous->next = current->next;
        }
        free( current );
        break;
      }
    }
}

/** 
 * Delete all RFC nodes containing the same hostname
 * This function is called upon the disconnect of a TCP Client
 * @param port_number number of the port we need to remove
 * 
*/
void deleteRFCNode(int port_number) {
    RFC_Node* current = rfc_list;
    RFC_Node* prev = NULL;

    // Loop to traverse linked list
    while (current != NULL) {
        if (current->port_number == port_number) { // Check if port matches
            if (prev != NULL) {
                prev->next = current->next;
                free(current);
                current = prev->next;
            } else {
                RFC_Node* temp = current;
                current = current->next;
                free(temp);
                rfc_list = current;
            }
        } else {
            prev = current;
            current = current->next;
        }
    }
}


/**
 * Easy function to create RFC related node
 * @param num number of rfc
 * @param title title of rfc
 * @param host name of host 
 * @return RFC_Node created node
*/
RFC_Node* createRFC_node(char *arrayString, int port, char *host) {
  RFC_Node* newNode = (RFC_Node*)malloc(sizeof(RFC_Node));
  if(newNode == NULL ) {
    fail("MALLOC call failed - RFC node");
  }
    char file_location[20];
    char file_name[12];
    int rfc_number_scan = 0;   

    sscanf(arrayString, "%s%s%d", file_location, file_name, &rfc_number_scan);
    strcpy(newNode->path, file_location);
    newNode->rfc_number = rfc_number_scan;
    strcpy(newNode->path, file_location);

    // Logic for title
    int count = 0;
    char *pos = arrayString;
    while(*pos != '\0' && count < 3) {
        if(*pos == ' ') {
            count++;
        }
        pos++;
    }
    if(count >= 3) {
        memmove(arrayString, pos, strlen(pos) + 1);
    } else {
        fail("error memmove");
    }

    strcat(newNode->title, arrayString);
    strcpy(newNode->hostname, host);
    newNode->port_number = port;
    newNode->next = NULL;

  return newNode;
}

/**
 * Function to add node to RFC related linked list
 * @param head head of the linked list
 * @param newNode node to add to the front of the linked list
*/
void addRFC_Node(RFC_Node** head, RFC_Node* newNode ) {
  if( *head == NULL ) {
    *head = newNode;
    newNode->next = NULL;
  } else {
    newNode->next = *head;
    *head = newNode;
  }
}

/**
 * Add command logic
 * Adds an existing rfc to the rfc_list 
 * Used after calling the GET command to update the list
 * @param buffer input of the client
 * @param client_hostname client's hostname
 * @param port number of client
 * @return response of the server
*/
char* addCommand(char *buffer, char *client_hostname, int __port) {
  char *response = new char[1024];
  response[0] = '\0';
  char command[4];
  char rfc[4];
  int rfc_number_str = 0;
  char version[12];
  char str_host[50];
  int port_user = 0;
  
  sscanf(buffer, "%s%s%d%s%s%d", command, rfc, &rfc_number_str, version, str_host, &port_user);
  //Invalid command
  if(strcmp(command, "ADD") != 0) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }
  //Invalid command
  if(strcmp(rfc, "RFC") != 0) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }
  //Invalid P2P
  if(strcmp(version, "P2P-CI/1.0") != 0) {
    strcat(response, "P2P-CI/1.0 505 P2P-CI Version Not Supported\n");
    return response;
  }
  //Invalid port
  if(__port != port_user) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }

  bool client_flag_found = false;
  bool rfc_flag = false;
  Client_Node *search = client_list;
  while( search != NULL) {
      if(strcmp(str_host, search->hostname) == 0 && strcmp(client_hostname, search->hostname) == 0) {
        client_flag_found = true; 
        break;
      }
      search = search->next;
  }

  if(client_flag_found == false) {
    strcat(response, "P2P-CI/1.0 404 Not Found\n");
    return response;
  }

  RFC_Node *current = rfc_list;
  while( current != NULL ) {
    if(rfc_number_str == current->rfc_number) {
      rfc_flag = true;
      break;
    }
    current = current->next;
  }

  if(rfc_flag == false) {
    strcat(response, "P2P-CI/1.0 404 Not Found\n");
    return response;
  }

  RFC_Node *nodeToAdd = (RFC_Node*)malloc(sizeof(RFC_Node));
  

  RFC_Node *verifySearch = rfc_list;
  while( verifySearch != NULL) {
    if(__port == verifySearch->port_number) {
      strcpy(nodeToAdd->path, verifySearch->path);
      break;
    }
    verifySearch = verifySearch->next;
  }

  strcpy(nodeToAdd->hostname, client_hostname);
  nodeToAdd->port_number = __port;
  nodeToAdd->rfc_number = rfc_number_str;
  strcpy(nodeToAdd->title, current->title);

  pthread_mutex_lock(&lock);
  addRFC_Node(&rfc_list, nodeToAdd);
  pthread_mutex_unlock(&lock);

  strcat(response, "Title: ");
  strcat(response, nodeToAdd->title);

  return response;
}

/**
 * Lookup command to lookup the title of an RFC given the number
 * @param buffer client input
 * @param client_hostname hostname of client
 * @return response of the server
*/
char* lookupCommand(char *buffer, char *client_hostname, int __port) {

  char *response = new char[1024];
  response[0] = '\0';
  char command[7];
  char rfc[4];
  int rfc_number_str = 0;
  char version[12];
  char str_host[50];
  int user_port = 0;
  sscanf(buffer, "%s%s%d%s%s%d", command, rfc, &rfc_number_str, version, str_host, &user_port);

  //Invalid command
  if(strcmp(command, "LOOKUP") != 0) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }

  //Invalid RFC 
  if(strcmp(rfc, "RFC") != 0) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }

  //Invalid version
  if(strcmp(version, "P2P-CI/1.0") != 0) {
    strcat(response, "P2P-CI/1.0 505 P2P-CI Version Not Supported\n");
    return response;
  }

  if(__port != user_port) {
     strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }


  // Look for the host
  bool client_flag_found = false;
  bool rfc_flag = false;
  Client_Node *search = client_list;
  while( search != NULL) {
      if(strcmp(str_host, search->hostname) == 0 && strcmp(client_hostname, search->hostname) == 0) {
        client_flag_found = true; 
        break;
      }
      search = search->next;
  }

  RFC_Node *current = rfc_list;
  while( current != NULL ) {
    if(rfc_number_str == current->rfc_number) {
      rfc_flag = true;
      break;
    }
    current = current->next;
  }

  if(rfc_flag == false) {
    strcat(response, "P2P-CI/1.0 404 Not Found\n");
    return response;
  }

  if(client_flag_found == false) {
    strcat(response, "P2P-CI/1.0 404 Not Found\n");
    return response;
  }

  if( rfc_flag == true && client_flag_found == true) {
    strcat(response, "Title: ");
    strcat(response, current->title);
    strcat(response, "\n");
  }

  return response;
}

/**
 * List command function 
 * @param buffer client's input char array
 * @param client_hostname clients hostname
 * @return response message for server to send to client
*/
char* listCommand(char *buffer, char *client_hostname, int __port) {

  char *response = new char[1024];
  response[0] = '\0';
  char f_line[512];
  int user_port = 0;
  char command[5];
  char all[4];
  char version[12];
  char str_host[50];
  sscanf(buffer, "%s%s%s%s%d", command, all, version, str_host, &user_port);

  //First string should be list
  if(strcmp(command, "LIST") != 0) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }
  //Second string should be all
  if(strcmp(all, "ALL") != 0) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }
  //Thrid string should be P2P
  if(strcmp(version, "P2P-CI/1.0") != 0) {
    strcat(response, "P2P-CI/1.0 505 P2P-CI Version Not Supported\n");
    return response;
  }
  //Fifth string should be user's port
  if(__port != user_port) {
    strcat(response, "P2P-CI/1.0 400 Bad Request\n");
    return response;
  }

  //Flag for searching if host is correct
  bool client_flag_found = false;
  //Search for client and verify host name
  Client_Node *search = client_list;
  while( search != NULL) {
      if(strcmp(str_host, search->hostname) == 0 && strcmp(client_hostname, search->hostname) == 0) {
        client_flag_found = true;
      }
      search = search->next;
  }
  //If host not found then print error response
  if(client_flag_found == false) {
    strcat(response, "P2P-CI/1.0 404 Not Found\n");
    return response;
  }

  //Once validated
   RFC_Node *current = rfc_list;
       while( current != NULL ) {
        //remove of new line character
          std::cout << "RFC " << current->rfc_number << " ";
          std::cout << current->title << " ";
          std::cout << current->hostname << " ";
          std::cout << current->port_number << std::endl;
          snprintf(f_line, sizeof(f_line), "RFC %d %s %s %d\n", current->rfc_number, current->title, current->hostname, current->port_number);
          strcat(response, f_line);
         current = current->next;
        }
    return response;
}

/**
 * Thread function responsible for dealing with client requests
 * @param socket socket connection to the client
*/
void *handleClient( void *socket ) {

    //Setup connection
    int clntSocket = *( ( int* ) socket );
    struct sockaddr_in clntAddr;
    socklen_t clntAddrLen = sizeof( clntAddr );
    // Get the port name and host
    int status = getpeername(clntSocket, (struct sockaddr* ) &clntAddr, &clntAddrLen);
    if( status != 0 ) {
      fail("getpeername() error");
    } 


    // Addition of client connection
    int client_port = ntohs( clntAddr.sin_port);
    struct hostent* hostInfo;
    hostInfo = gethostbyaddr( (char *)&clntAddr.sin_addr, sizeof(clntAddr), AF_INET );

    char intital_OS[32];
    ssize_t bytes_recieved = recv(clntSocket, &intital_OS, sizeof(intital_OS), 0);
    if(bytes_recieved == -1 ) {
      fail("Problems with recieving OS");
    } 

    pthread_mutex_lock( &lock );
    //Create client node 
    Client_Node *node = createClientNode(hostInfo->h_name, client_port, intital_OS);
    addClientNode(&client_list, node);
    pthread_mutex_unlock( &lock );

    //Client's buffer
    char buffer[254];

    //Printout Client Port
    std::cout << "Client is listening on port: " << client_port << std::endl;
    
    //Uploading rfcs to list 
    while( true )  {
      ssize_t bytes_recieved = recv(clntSocket, &buffer, sizeof(buffer), 0);
      if( bytes_recieved == -1) {
        /** Filler */
      }
    
      //END call from client to stop adding rfc nodes
      if( strncmp("END", buffer, 3) == 0) {
        break;
      } 

      RFC_Node *node = createRFC_node(buffer, client_port, hostInfo->h_name);
      addRFC_Node(&rfc_list, node);
    }

    char clientSentBuffer[512];
    char serverSendBuffer[512];


   //Loop for server-side constant connection and commands
    while( true ) {
      memset(clientSentBuffer,'\0', sizeof(clientSentBuffer)); 
      memset(serverSendBuffer,'\0', sizeof(serverSendBuffer));               
      ssize_t bytesRead = recv(clntSocket, clientSentBuffer, sizeof(clientSentBuffer), 0);

      //Disconnect client
      if(bytesRead <= 0) {
        pthread_mutex_lock(&lock);
        deleteRFCNode(client_port);
        deleteClientNode(client_port);
        pthread_mutex_unlock(&lock);
        break;
      }

      char command[7];
      sscanf(clientSentBuffer, "%s", command);

        // List command
      if( strncmp("LIST", command, 4) == 0) {

        char *response = listCommand(clientSentBuffer, hostInfo->h_name, client_port);
        strncpy(serverSendBuffer, response, sizeof(serverSendBuffer) - 1);
        serverSendBuffer[sizeof(serverSendBuffer) - 1] = '\0';
        send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);

        // Lookup command
      } else if (strncmp("LOOKUP", command, 6) == 0) {
        char *response = lookupCommand(clientSentBuffer, hostInfo->h_name, client_port);
        strncpy(serverSendBuffer, response, sizeof(serverSendBuffer) - 1);
        serverSendBuffer[sizeof(serverSendBuffer) - 1] = '\0';
        send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
        //Add command
      } else if( strncmp("ADD", command, 3) == 0 ) {
        char *response = addCommand(clientSentBuffer, hostInfo->h_name, client_port);
        strncpy(serverSendBuffer, response, sizeof(serverSendBuffer) - 1);
        serverSendBuffer[sizeof(serverSendBuffer) - 1] = '\0';
        send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
        //Get command (inlined)
      } else if(strncmp("GET", command, 3) == 0) {
  
        char command[4];
        char rfc[4];
        int rfc_num = 0;
        char version[11];
        char host_name_parse[70];
        char os_input_from_string[32];
        sscanf(clientSentBuffer, "%s%s%d%s%s%s", command, rfc, &rfc_num, version, host_name_parse, os_input_from_string);
        bool flag = false;
        bool port_flag = false;
        char file_name[35];
        file_name[0] = '\0';

        if(strcmp(command, "GET") != 0) {
          strcat(serverSendBuffer, "P2P-CI/1.0 400 Bad Request\n");
          send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
          break;
        }

        if(strcmp(rfc, "RFC") != 0) {
          strcat(serverSendBuffer, "P2P-CI/1.0 400 Bad Request\n");
          send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
          break;
        }

       if(strcmp(version, "P2P-CI/1.0") != 0) {
          strcat(serverSendBuffer, "P2P-CI/1.0 505 P2P-CI Version Not Supported\n");
          send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
          break;
       }

        int current_port = 0;
        RFC_Node *current = rfc_list;
        while( current != NULL ) {
          if(rfc_num == current->rfc_number) {
             flag = true;
             strcat(file_name, current->path);
             current_port = current->port_number;
             break;
          }
          current = current->next;
        }
        //Not found
        if(flag == false) {
            strcat(serverSendBuffer, "P2P-CI/1.0 400 Bad Request\n");
            send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
            break;
        }

        char temp_os_arr[32];
        bool os_flag = false;
        Client_Node *look = client_list;
        while( look != NULL ) {
          if(current_port == look->port_number) {
             os_flag = true;
             strcpy(temp_os_arr, look->os_string);
             break;
          }
          look = look->next;
        }

        //Client not found
        if(os_flag == false) {
            strcat(serverSendBuffer, "P2P-CI/1.0 400 Bad Request\n");
            send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
            break;
        }

        //Checking for OS match
        if(strcmp(os_input_from_string, temp_os_arr) != 0) {
            strcat(serverSendBuffer, "P2P-CI/1.0 400 Bad Request\n");
            send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
            break;
        }

        bool client_flag_found = false;
        Client_Node *search = client_list;
        while( search != NULL) {
          if(strcmp(hostInfo->h_name, search->hostname) == 0 && strcmp(host_name_parse, search->hostname) == 0) {
            client_flag_found = true; 
            break;
          }
          search = search->next;
        }

        if(client_flag_found == false) {
            strcat(serverSendBuffer, "P2P-CI/1.0 404 Not Found\n");
            send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
            break;
        }


        // Responsible for getting the response time 
        char time_string[50];
        time_t rawTime;
        struct tm *timeInfo;
        time(&rawTime); 
        timeInfo = localtime(&rawTime); 
        strftime(time_string, sizeof(time_string), "%a, %d %b %Y %H:%M:%S %Z", timeInfo);

        // Format for file string path/rfcXXX.txt
        char numberChar[5];
        numberChar[0] = '\0';
        strcat(file_name, "/rfc");
        snprintf(numberChar, sizeof(numberChar), "%d", rfc_num);
        strcat(file_name, numberChar);
        strcat(file_name, ".txt");

        struct stat fileStat;
        stat(file_name, &fileStat);
        time_t modificationTime = fileStat.st_mtime;
        struct tm* timeinfo = localtime(&modificationTime);
        char timeStr[100];
        strftime(timeStr, sizeof(timeStr), "%a, %d %b %Y %H:%M:%S EST", timeinfo);

        current = rfc_list;
        while( current != NULL ) {
          if(client_port == current->port_number) {
            port_flag = true;
            break;
          }
         current = current->next;
        }

        if( port_flag == false) {
          strcat(serverSendBuffer, "P2P-CI/1.0 404 Not Found\n");
          send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
          break;
        } 
        
        char file_name_write[35];
        file_name_write[0] = '\0';
        strcat(file_name_write, current->path);
        strcat(file_name_write, "/rfc");
        snprintf(numberChar, sizeof(numberChar), "%d", rfc_num);
        strcat(file_name_write, numberChar);
        strcat(file_name_write, ".txt");
     
        std::ifstream fp(file_name, std::ios::binary);
        fp.seekg(0, std::ios::end);
        std::streampos content_length = fp.tellg();
        fp.seekg(0, std::ios::beg);
        std::string fileSizeStr = std::to_string(content_length);
        char contentSizeCString[64];
        strcpy(contentSizeCString, fileSizeStr.c_str());
        fp.close();

        //OUTPUT   
        std::cout << "P2P-CI/1.0 200 OK" << std::endl;
        std::cout << "Date: " << time_string << std::endl;
        std::cout << "OS: " << temp_os_arr <<  std::endl; 
        std::cout << "Last-Modified: " << timeStr << std::endl;
        std::cout << "Content-Length: " << contentSizeCString << std::endl;


        std::cout << "Content-Type: text/text" << std::endl;
        strcat(serverSendBuffer, "P2P-CI/1.0 200 OK\n");
        strcat(serverSendBuffer, "Date: ");
        strcat(serverSendBuffer, time_string);
        strcat(serverSendBuffer, "\n");
        strcat(serverSendBuffer, "OS: ");
        strcat(serverSendBuffer, temp_os_arr);
        strcat(serverSendBuffer, "\n");
        strcat(serverSendBuffer, "Last-Modified: ");
        strcat(serverSendBuffer, timeStr);
        strcat(serverSendBuffer, "\n");
        strcat(serverSendBuffer, "Content-Length: ");
        strcat(serverSendBuffer, contentSizeCString);
        strcat(serverSendBuffer, "\n");
        strcat(serverSendBuffer, "Content-Type: text/text\n");



        if(strcmp(file_name, file_name_write) != 0) {

          std::ifstream inputFile(file_name);
          if( !inputFile.is_open() ) {
            fail("Trouble opening input file");
          }

          std::ofstream outputFile(file_name_write);
          if(!outputFile.is_open() ) {
            fail("Trouble opening output file");
          }

          std::string reading_line; 
          while(std::getline(inputFile, reading_line)) {
            // std::cout << reading_line << std::endl;
            outputFile << reading_line << std::endl;
          }

          inputFile.close();
          outputFile.close();
        }

        send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);


      
      }

      //Invalid command provided
      else {
        strcat(serverSendBuffer, "P2P-CI/1.0 400 Bad Request\n");
        send(clntSocket, serverSendBuffer, sizeof(serverSendBuffer), 0);
      }

    
   } // End of server-thread while loop logic 
  close(clntSocket);
  return NULL;
}

/**
 * Main function of the server.
 * Passes off function to thread
 * @return 0
*/
int main( void ) {
    // Create a socket
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        fail("Error creating socket");
        exit(EXIT_FAILURE);
    }

    // Bind to an IP address and port
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(  PORT );

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        fail("Error creating socket. Please wait a few seconds to re-try.");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverSocket, 10) == -1) {
        fail("Error listening on socket");
        close(serverSocket);
        exit(EXIT_FAILURE);
    }



    while ( true  ) {
      // Accept a client connection. -- structures that contain the address of client 
        struct sockaddr_in clientAddr;
        socklen_t clientAddrLen = sizeof(clientAddr);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientAddrLen);
        if (clientSocket == -1) {
            fail("Error accepting connection");
            close(serverSocket);
            exit(EXIT_FAILURE);
        }

      // printf("This is the client's port number:%d \n", ntohs(clntAddr.sin_port)); //Line for peer's port

      // Thread error checking 
      if( pthread_create( &userLock, NULL, handleClient, &clientSocket ) != 0 ) { 
        fail( "Thread incorrect ");
      }
    }

    // close(clientSocket);
    close(serverSocket);

    return 0;
}