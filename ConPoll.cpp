#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>
#include <string>
#include <sstream>  // For std::stringstream

#pragma comment(lib, "Ws2_32.lib")

// Function declarations
SOCKET createConnection(const char* ipAddress, int port);
int checkSocketStatus(SOCKET sock, int timeout);
int sendData(SOCKET sock, const std::string& data);
std::string getPollEventText(int events);
std::string getTime();

// Global constants and variables
int timeSpan = 5;
bool abortLoop = false;
int loopCount = 0;
std::string ipAddress = "192.168.2.1";
int portNum = 80;

// Main function

// USAGE WSAPollTest.exe <IP> <IntervalInSeconds>
// 
// Arguments are technically optional but the hardcoded defaults probably aren't useful (192.168.2.1:80)

int main(int argc, char* argv[]) {

    // Check if an IP address was passed as a command-line argument
    if (argc > 1) {
        ipAddress = argv[1];
    }

    // Check if a time span was passed as the second command-line argument
    if (argc > 2) {
        // Convert the second argument from C-string to integer
        int argTimeSpan = std::atoi(argv[2]);
        // Update timeSpan only if the passed argument is greater than 0
        if (argTimeSpan > 0) {
            timeSpan = argTimeSpan;
        }
        else {
            std::cerr << "Invalid time span provided, using default." << std::endl;
        }
    }

    std::cout << "Using IP address: " << ipAddress << std::endl;
    std::cout << "Using time span: " << timeSpan << " seconds\r\n" << std::endl;

    WSADATA wsaData;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Create a connection
    SOCKET sock = createConnection(ipAddress.c_str(), portNum);

    if (checkSocketStatus(sock, timeSpan * 1000) == SOCKET_ERROR) {
        std::cerr << "Socket status check failed." << std::endl;
    }

    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket = INVALID_SOCKET" << std::endl;
        WSACleanup();  // Cleanup and exit if connection failed
        return 1;
    }

    auto startTime = std::chrono::steady_clock::now();

    // Continuously check socket status every 'timeSpan' seconds
    while (!abortLoop) {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count();
        std::cout << "Elapsed time since connection: " << elapsedTime << " seconds" << std::endl;

        if (checkSocketStatus(sock, timeSpan * 1000) == SOCKET_ERROR) {
            std::cerr << "Socket status check failed, exiting loop." << std::endl;
            break; // Exit the loop if checking the socket status failed
        }

        if (loopCount == 2) {
            std::string httpGetRequest = "GET / HTTP/1.1\r\nHost: " +  ipAddress + "\r\nConnection: close\r\n\r\n";
            int bytesSent = sendData(sock, httpGetRequest); // Send the HTTP GET request
            if (bytesSent == SOCKET_ERROR) {
                std::cerr << getTime() << ": Failed to send data: " << WSAGetLastError() << std::endl;
                abortLoop = true; // Signal to close the socket and clean up
            }
            else {
                std::cout << getTime() << ": Successfully sent " << bytesSent << " bytes." << std::endl;
            }
            loopCount = 0; // Reset the loop count after sending data
        }

        std::this_thread::sleep_for(std::chrono::seconds(timeSpan)); // Sleep before the next status check
        loopCount++; // Increment the loop count
        std::cout <<  std::endl;
    }

    std::cout << getTime() << ": Closing socket and cleaning up." << std::endl;
    closesocket(sock); // Close the socket when done
    WSACleanup();      // Cleanup Winsock

    return 0;
}

// Function implementations
SOCKET createConnection(const char* ipAddress, int port) {
    SOCKET connectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connectSocket == INVALID_SOCKET) {
        std::cerr << "Error at socket(): " << WSAGetLastError() << std::endl;
        return INVALID_SOCKET;
    }

    sockaddr_in clientService = {};
    clientService.sin_family = AF_INET;
    clientService.sin_addr.s_addr = inet_addr(ipAddress); // Set IP address
    clientService.sin_port = htons(port); // Set port number

    if (connect(connectSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect: " << WSAGetLastError() << std::endl;
        closesocket(connectSocket);
        return INVALID_SOCKET;
    }

    std::cout << getTime() << ": Successfully connected to " << ipAddress << " on port " << port << std::endl;
    return connectSocket; // Return the socket used to connect to the server
}

int checkSocketStatus(SOCKET sock, int timeout) {
    WSAPOLLFD fdArray[1];
    fdArray[0].fd = sock;
    fdArray[0].events = POLLRDNORM | POLLWRNORM | POLLPRI | POLLRDBAND; // Now checking for both readability and writability

    int result = WSAPoll(fdArray, 1, timeout);
    std::cout << getTime() << ": WSAPoll result=" << result << std::endl;

    std::cout <<  getPollEventText(fdArray[0].events) << std::endl;

    if (result > 0) {
        // Check for normal data readability
        if (fdArray[0].revents & POLLRDNORM) {
            std::cout << getTime() << ": Socket is readable (normal data)." << std::endl;
        }
        // Check for normal data writability
        if (fdArray[0].revents & POLLWRNORM) {
            std::cout << getTime() << ": Socket is writable (normal data)." << std::endl;
        }
        // Check for high-priority data readability
        if (fdArray[0].revents & POLLPRI) {
            std::cout << getTime() << ": Socket has high-priority data available to read." << std::endl;
        }
        // Check for out-of-band data readability
        if (fdArray[0].revents & POLLRDBAND) {
            std::cout << getTime() << ": Socket can read out-of-band data." << std::endl;
        }
        // Check for socket errors
        if (fdArray[0].revents & POLLERR) {
            std::cerr << getTime() << ": Socket error." << std::endl;
            abortLoop = true;
            return SOCKET_ERROR; // Socket error
        }
        // Check if the socket has been closed
        if (fdArray[0].revents & POLLHUP) {
            std::cerr << getTime() << ": Socket has been closed." << std::endl;
            abortLoop = true;
            return SOCKET_ERROR; // Connection closed
        }
    }

    return 0; // No events occurred, or only readability was indicated
}


std::string getTime() {
    // Get current time
    auto currentTime = std::chrono::system_clock::now();
    std::time_t currentTimeT = std::chrono::system_clock::to_time_t(currentTime);

    // Convert to local time safely
    struct tm localTime;
    localtime_s(&localTime, &currentTimeT); // Using localtime_s for safer conversion

    // Create a buffer to hold the formatted time
    char timeBuffer[80];
    strftime(timeBuffer, sizeof(timeBuffer), "%m/%d/%Y %I:%M:%S %p", &localTime); // Format the time

    return std::string(timeBuffer); // Return the formatted time as a string
}

std::string getPollEventText(int events) {
    std::stringstream ss;
    if (events & POLLPRI) {
        ss << "POLLPRI: Priority data may be read without blocking.\n";
    }
    if (events & POLLRDBAND) {
        ss << "POLLRDBAND: Priority band (out-of-band) data can be read without blocking.\n";
    }
    if (events & POLLRDNORM) {
        ss << "POLLRDNORM: Normal data can be read without blocking.\n";
    }
    if (events & POLLWRNORM) {
        ss << "POLLWRNORM: Normal data can be written without blocking.\n";
    }
    if (ss.str().empty()) {
        ss << "UNKNOWN: No recognized events.\n";
    }
    return ss.str();
}

int sendData(SOCKET sock, const std::string& data) {
    return send(sock, data.c_str(), data.length(), 0);
}

