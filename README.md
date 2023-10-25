# Description

Peer-to-Peer Server / Client socket program in C++ that can send and download 'rfc*.txt' files.
The server consists of two linked lists with one that contains unique clients connected to the server 
and the other list maintaining a list of the RFCs on the server. 
Once the client connection is made, the client automatically uploads its RFCs to the server.
Upon client disconnect the client's corresponding RFCs are deleted from the list.

Clients have four commands: 'GET', 'LOOKUP', 'ADD', and 'LIST'.

GET: the command responsible for retrieving and downloading the RFC text file.
LOOKUP: the command responsible for looking up the title of an RFC in the system given a number.
ADD: the command responsible for adding an RFC node to the server's list after calling 'GET'.
LIST: the command responsible for displaying all RFCs in the server's list database.

# Project Structure

https://github.com/averycrespo/TCP_project/wiki/Project-Structure

# Installation

For server.cpp and client.cpp(s) to run correctly, you must follow the same format as shown above.
server.cpp must be in the TCP_project directory, while all client.cpp programs are in their respective client_directory subdirectories.

Calling 'make' in the TCP_project directory will compile the files, but only up for client_directory1 and client_directory2. You will have to add lines to the makefile if you deviate from the names of client_directory1 and client_directory2.

# Instructions
### Programs are downloaded correctly and compiled using 'make' 

1. Call ./server from a bash terminal in the TCP_project directory.
2. Call ./client from a separate bash terminal in client_directory1.
3. Call ./client from another separate bash terminal in client_directory2.
4. Call one of the four commands (GET/ADD/LIST/LOOKUP)

## Notes (Important)
-Due to how this program was compiled using SSH my IDE would only run and configure to Linux.
As such, when testing the GET command, 'Linux' as my operating system would only work.

-When clients are connecting to the server their respective PORT NUMBER will be printed out on the server side. This number must be used by the respective client when calling ADD/LOOKUP/LIST. 

### Sample run instructions
a: = Server
b: = Client X
c: = Client Y

a: './server'

b: 'cd client_directory1'
b: './client'
b: 'LIST ALL P2P-CI/1.0
    localhost
    (port number)

c: 'cd client_directory2'
c: './client'
c: 'LOOKUP RFC 1234 P2P-CI/1.0
    localhost
    (port number)

# Commands

### (XXXX) Represents any valid RFC.txt
## GET
    GET RFC (XXXX) P2P-CI/1.0
    localhost
    Linux

## LOOKUP
    LOOKUP RFC (XXXX) P2P-CI/1.0
    localhost
    (port number)

## ADD
    ADD RFC (XXXX) P2P-CI/1.0
    localhost
    (port number)

## LIST
    LIST ALL P2P-CI/1.0
    localhost
    (port number)


