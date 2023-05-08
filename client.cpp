#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <vector>
#include <string.h>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

using namespace std;

struct ThreadData
{
    int index;
    string binary_code;
    char decoded_char;
    string hostname;
    int port_number;
    vector<vector<int> > positions;
    string original_message;
    string *shared_mem;
    int num_threads;
};

void *child_thread(void *data)
{
    ThreadData *thread_data = (ThreadData *)data;

    // Create a socket to communicate with the server program
    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(thread_data->port_number);
    server_addr.sin_addr.s_addr = inet_addr(thread_data->hostname.c_str());
    // cout << server_addr.sin_port << endl;

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        cerr << "Connection to server failed." << endl;
        pthread_exit(NULL);
    }
    // cout <<"Code being sent: " << thread_data->binary_code << endl;

    // Send code to the server program using sockets
    send(client_socket, thread_data->binary_code.c_str(), thread_data->binary_code.size() + 1, 0);

    // Wait for char from the server program
    char buffer[1024];
    recv(client_socket, buffer, sizeof(buffer), 0);
    thread_data->decoded_char = buffer[0];
    // cout << "decoded char: " << thread_data->decoded_char << endl;

    close(client_socket);
    pthread_exit(NULL);
}

// Construct the original message from the decoded characters and positions
string build_word(void *data, int m)
{

    ThreadData *thread_data = (ThreadData *)data;
    vector<vector<int> > positions = thread_data->positions;
    string original_message = thread_data->original_message;

    vector<char> decoded_chars(m);
    for (int i = 0; i < m; i++)
    {
        decoded_chars[i] = thread_data[i].decoded_char; // Fix: Use "decoded_chars" instead of "thread_data[i].decoded_char"
    }

    vector<char> message;
    int max_position = 0;
    bool finished = false;
    while (!finished)
    {
        finished = true;
        for (int i = 0; i < positions.size(); i++)
        {
            if (positions[i].empty())
            {
                continue;
            }
            if (positions[i][0] == max_position)
            {
                original_message.push_back(decoded_chars[i]);
                positions[i].erase(positions[i].begin());
                finished = false;
            }
        }
        max_position++;
    }

    message.insert(message.end(), original_message.begin(), original_message.end());
    string mess(message.begin(), message.end());
    *thread_data->shared_mem = mess;
    return string(message.begin(), message.end());                                  
}

bool compare_pairs(const pair<string, vector<int> > &a, const pair<string, vector<int> > &b)
{
    return a.second[0] < b.second[0];
}

int main(int argc, char *argv[])
{
    string hostname = argv[1];
    int port_number = stoi(argv[2]);
    // string compressed_file_name = argv[3];

    // localhost to ip
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(hostname.c_str(), NULL, &hints, &res) != 0)
    {
        cerr << "Failed to resolve hostname." << endl;
        return 1;
    }

    struct sockaddr_in server_addr;
    memcpy(&server_addr, res->ai_addr, res->ai_addrlen);
    server_addr.sin_port = htons(port_number);
    char ipstr[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &server_addr.sin_addr, ipstr, sizeof(ipstr));
    string ip_address = ipstr;

    freeaddrinfo(res);

    vector<string> binary_codes;
    vector<vector<int> > positions; // 2D vector to store positions for each binary code
    string line;
    while (getline(cin, line))
    {
        stringstream ss(line);
        string binary_code;
        ss >> binary_code;
        binary_codes.push_back(binary_code);
        vector<int> line_positions;
        int position;
        while (ss >> position)
        {
            line_positions.push_back(position);
        }
        positions.push_back(line_positions);
    }

    // store information for child threads
    int m = binary_codes.size();
    pthread_t threads[m];
    ThreadData thread_data[m];

    string shared_mem;

    for (int i = 0; i < m; i++)
    {
        thread_data[i].index = i;
        thread_data[i].binary_code = binary_codes[i];
        thread_data[i].hostname = ip_address;
        thread_data[i].port_number = port_number;
        thread_data[i].positions = positions;
        thread_data[i].shared_mem = &shared_mem;
        thread_data[i].num_threads = m;
    }

    // Create child threads
    for (int i = 0; i < m; i++)
    {
        pthread_create(&threads[i], NULL, child_thread, (void *)&thread_data[i]);
    }
    // Joiin child threads
    for (int i = 0; i < m; i++)
    {
        pthread_join(threads[i], NULL);
    }
    build_word(&thread_data, m);

    //Write the received information into a memory location accessible by the main thread.
    cout << "Original message: " << *thread_data->shared_mem;

    return 0;
}