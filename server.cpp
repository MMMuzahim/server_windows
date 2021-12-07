#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <queue>
#include <mutex>

#define _WIN32_WINNT 0x501
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")

using namespace std;
using std::cerr;


queue<int> client_queue;
CRITICAL_SECTION m;


struct Data
{
    int result;
};


DWORD WINAPI request_processing(void *dd)
{
    while (1) {
        bool get_mutex = false;
        while (!get_mutex) {
            try {//cout << 8;
                EnterCriticalSection(&m);
                get_mutex = true;
            }
            catch(const exception& e) {}
        }
        //cout << 9;
        if (!client_queue.empty()) {
            int client_socket = client_queue.front();
            client_queue.pop();
            LeaveCriticalSection(&m);
            
            const int max_client_buffer_size = 1024;
            char buf[max_client_buffer_size];

            Data * d = (Data *)dd;

            d->result = recv(client_socket, buf, max_client_buffer_size, 0);

            stringstream response;
            stringstream response_body;

            cout << "Hello from " << GetCurrentThreadId() << endl;
            if (d->result == SOCKET_ERROR) {
                cerr << "recv failed: " << d->result << "\n";
                closesocket(client_socket);
            } else if (d->result == 0) {
                cerr << "connection closed...\n";
            } else if (d->result > 0) {
                buf[d->result] = '\0';
                cout << buf << endl;
                
                
                char *p = strchr(buf, ' ');
                if (!p) {
                    closesocket(client_socket);
                    return -1;
                }
                while (*p == ' ') p++;
                char *s = strchr(p, ' ');
                if (!s) {
                    closesocket(client_socket);
                    return -1;
                }
                char file_name[256];
                strncpy(file_name, p, (int)(s-p));
                file_name[(int)(s-p)] = '\0';
                cout << "file name: " << file_name << endl;
                if(strcmp(file_name, "/") == 0) {
                    strcpy(file_name, "/index.html");
                }
                char local[256];
                for (size_t i = 0; i < 256; i++)
                {
                    local[i] = 0;
                }
                
                sprintf(local,"data%s", file_name);


                if (strstr(local, ".html") != NULL || strstr(local, ".txt") != NULL) {
                    ifstream file (local);
                    if (file.is_open())
                    {
                        string line;
                        while (getline(file,line))
                        {
                            response_body << line << '\n';
                        }
                    file.close();
                    }
                    
                    response << "HTTP/1.1 200 OK\r\n"
                        << "Version: HTTP/1.1\r\n"
                        << "Content-Type: text/html; charset=utf-8\r\n"
                        << "Content-Length: " << response_body.str().length()
                        << "\r\n\r\n"
                        << response_body.str();

                    d->result = send(client_socket, response.str().c_str(),
                        response.str().length(), 0);
                }
                
                if (strstr(local, ".png") != NULL || strstr(local, ".jpg") != NULL || strstr(local, ".ico") != NULL) {
                    FILE  *bfile = fopen(local, "rb");
                    char x;
                    if (bfile != NULL) {
                        while (!feof(bfile))
                        {
                            fscanf(bfile, "%c", &x);
                            response_body << x;
                        }

                        string type;
                        unsigned int i = 255;
                        while (local[i] == 0)
                        {
                            i--;
                        }
                        while (local[i] != '.')
                        {
                            type += local[i];
                            i--;
                        }
                        vector<char> type_v;
                        for (size_t i = 0; i < type.length(); i++)
                        {
                            type_v.push_back(type[i]);
                        }
                        reverse(type_v.begin(), type_v.end());
                        type = "";
                        for (size_t i = 0; i < type_v.size(); i++)
                        {
                            type += type_v[i];
                        }
                        type += "\0";
                        
                        response << "HTTP/1.1 200 OK\r\n"
                            << "Version: HTTP/1.1\r\n"
                            << "Content-Type: image/" << type << "\r\n"
                            << "Content-Length: " << response_body.str().length()
                            << "\r\n\r\n"
                            << response_body.str();

                        d->result = send(client_socket, response.str().c_str(),
                            response.str().length(), 0);
                    }
                }
            }

            
            if (d->result == SOCKET_ERROR) {
                
                cerr << "send failed: " << WSAGetLastError() << "\n";
            }
            
            closesocket(client_socket);
        }
        else {
            LeaveCriticalSection(&m);
        }
    }
}


int main()
{
    WSADATA wsaData;
    if (!InitializeCriticalSectionAndSpinCount(&m, 
        0x00000400) ) 
        return -1;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);

    if (result != 0) {
        cerr << "WSAStartup failed: " << result << "\n";
        return result;
    }

    struct addrinfo* addr = NULL;

    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    

    result = getaddrinfo("127.0.0.1", "8097", &hints, &addr);

    if (result != 0) {
        cerr << "getaddrinfo failed: " << result << "\n";
        WSACleanup();
        return 1;
    }

    int listen_socket = socket(addr->ai_family, addr->ai_socktype,
        addr->ai_protocol);
    
    if (listen_socket == INVALID_SOCKET) {
        cerr << "Error at socket: " << WSAGetLastError() << "\n";
        freeaddrinfo(addr);
        WSACleanup();
        return 1;
    }

    result = bind(listen_socket, addr->ai_addr, (int)addr->ai_addrlen);

    if (result == SOCKET_ERROR) {
        cerr << "bind failed with error: " << WSAGetLastError() << "\n";
        freeaddrinfo(addr);
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }

    if (listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "listen failed with error: " << WSAGetLastError() << "\n";
        closesocket(listen_socket);
        WSACleanup();
        return 1;
    }
    
    
    int client_socket;
    Data * d = new Data{result};
    HANDLE hThreads[2];
    hThreads[0] = CreateThread(NULL, 0, request_processing, (void *)d, 0, NULL);
    hThreads[1] = CreateThread(NULL, 0, request_processing, (void *)d, 0, NULL);


    while (1) {
        client_socket = accept(listen_socket, NULL, NULL);
        if (client_socket == INVALID_SOCKET) {
            cerr << "accept failed: " << WSAGetLastError() << "\n";
            closesocket(listen_socket);
            WSACleanup();
            return 1;
        }
        //cout << 1;
        bool get_mutex = false;
        while (!get_mutex) {
            try {//cout << 4;
                EnterCriticalSection(&m);//cout << 5;
                get_mutex = true;
            }
            catch(const exception& e) {}
        }
        //cout << 2;
        client_queue.push(client_socket);
        LeaveCriticalSection(&m);
    }

    closesocket(listen_socket);
    freeaddrinfo(addr);
    WSACleanup();
    DeleteCriticalSection(&m);
    return 0;
}
