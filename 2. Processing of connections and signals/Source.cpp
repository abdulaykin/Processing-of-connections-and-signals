#include <iostream>
#include <vector>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <signal.h>

#pragma comment(lib, "Ws2_32.lib")  // Подключение библиотеки Winsock

volatile sig_atomic_t signalReceived = 0;  

void handleSignal() {
    signalReceived = 1;  
    std::cout << "Received SIGHUP signal. Closing the server.\n";  
}

int main() {
    // Инициализация Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {  
        std::cerr << "WSAStartup failed.\n";  

        return 1;  
    }

    
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);  

    if (serverSocket == INVALID_SOCKET) {  
        std::cerr << "Error creating socket: " << WSAGetLastError() << "\n";  
        WSACleanup(); 

        return 1;  
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;  
    serverAddress.sin_addr.s_addr = INADDR_ANY;  
    serverAddress.sin_port = htons(8080);  

    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {  
        std::cerr << "Error binding socket: " << WSAGetLastError() << "\n";  

        closesocket(serverSocket);  

        WSACleanup();  
        return 1;  
    }

    if (listen(serverSocket, 1) == SOCKET_ERROR) {  
        std::cerr << "Error listening for connections: " << WSAGetLastError() << "\n"; 

        closesocket(serverSocket);  

        WSACleanup();  
        return 1;  
    }

    std::cout << "Server listening on port 8080\n";  

    std::vector<SOCKET> clientSockets; 
    fd_set readFileDescriptors;  
    int maxFileDescriptor = serverSocket; 

    // Создание потока для обработки сигнала
    std::thread signalThread(handleSignal);
    signalThread.detach();  

    while (true) {  
        if (signalReceived) {  
            break;  
        }

        FD_ZERO(&readFileDescriptors);  
        FD_SET(serverSocket, &readFileDescriptors);  

        for (SOCKET clientSocket : clientSockets) {  
            FD_SET(clientSocket, &readFileDescriptors);

            if (clientSocket > maxFileDescriptor) {
                maxFileDescriptor = clientSocket; 
            }
        }

        struct timeval timeout;
        timeout.tv_sec = 1;  
        timeout.tv_usec = 0;  

        int selectResult = select(maxFileDescriptor + 1, &readFileDescriptors, NULL, NULL, &timeout); 

        if (selectResult == SOCKET_ERROR) {  
            if (WSAGetLastError() == WSAEINTR) {  
                std::cout << "select was interrupted by a signal.\n"; 

                continue;  
            } else {
                std::cerr << "Error in select: " << WSAGetLastError() << "\n";  

                break;
            }
        }

        if (FD_ISSET(serverSocket, &readFileDescriptors)) {  
            struct sockaddr_in clientAddress;  
            int clientAddressLength = sizeof(clientAddress);  
            SOCKET clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressLength); 
            
            if (clientSocket == INVALID_SOCKET) {  
                std::cerr << "Error accepting connection: " << WSAGetLastError() << "\n";  

                continue;  
            }

            char clientIP[INET_ADDRSTRLEN];  
            inet_ntop(AF_INET, &clientAddress.sin_addr, clientIP, INET_ADDRSTRLEN);  
            std::cout << "Accepted connection from " << clientIP << ":" << ntohs(clientAddress.sin_port) << "\n"; 
            clientSockets.push_back(clientSocket); 
        }

        for (auto it = clientSockets.begin(); it != clientSockets.end();) { 
            SOCKET clientSocket = *it;  

            if (FD_ISSET(clientSocket, &readFileDescriptors)) {  
                char messageBuffer[1024];  
                int bytesReceived = recv(clientSocket, messageBuffer, sizeof(messageBuffer), 0); 

                if (bytesReceived > 0) {  
                    messageBuffer[bytesReceived] = '\0';  
                    std::cout << "Received " << bytesReceived << " bytes: " << messageBuffer << "\n";  
                } else if (bytesReceived == 0) {  
                    std::cout << "Connection closed by client.\n";  

                    closesocket(clientSocket); 
                    it = clientSockets.erase(it);  

                    continue;  
                } else {  
                    std::cerr << "Error receiving data: " << WSAGetLastError() << "\n";  

                    closesocket(clientSocket); 

                    it = clientSockets.erase(it); 

                    continue;  
                }
            }

            ++it; 
        }
    }

    // Закрытие всех сокетов
    closesocket(serverSocket);  
    for (SOCKET clientSocket : clientSockets) {  
        closesocket(clientSocket); 
    }

    WSACleanup();  
    return 0;  
}