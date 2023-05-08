#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <cstring>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/wait.h>
#include <time.h>

using namespace std;

struct Node
{
    char letter;
    int freq;
    Node *left, *right;
};

// Merge two nodes into a single parent node
Node *mergeNodes(Node *node1, Node *node2)
{
    Node *parent = new Node;
    parent->letter = '\0'; // \0 to mark the parent node
    parent->freq = node1->freq + node2->freq;
    parent->left = node1;
    parent->right = node2;
    return parent;
}

bool compareNodes(Node *a, Node *b)
{
    return a->freq < b->freq;
}

void sortNodes(Node *nodes[], int n)
{
    sort(nodes, nodes + n, compareNodes);
}

// Construct Huffman tree
Node *buildHuffmanTree(char data[], int freq[], int n)
{
    Node **nodes = new Node *[n];
    for (int i = 0; i < n; i++)
    {
        nodes[i] = new Node;
        nodes[i]->letter = data[i];
        nodes[i]->freq = freq[i];
        nodes[i]->left = nodes[i]->right = NULL;
    }

    sortNodes(nodes, n);

    while (n > 1)
    {
        Node *parent = mergeNodes(nodes[0], nodes[1]);
        nodes[0] = parent;
        for (int i = 1; i < n - 1; i++)
        {
            nodes[i] = nodes[i + 1];
        }
        n--;
        sortNodes(nodes, n);
    }

    Node *root = nodes[0];
    delete[] nodes;
    return root;
}

// function to print the edges left->0  right->1
void label_edges(Node *root, string code_path)
{
    if (!root)
    {
        return;
    }
    label_edges(root->left, code_path + "0");
    if (root->letter != '\0')
    {
        cout << "Symbol: " << char(root->letter) << ", Frequency: " << root->freq << ", Code: " << code_path << endl;
    }
    label_edges(root->right, code_path + "1");
}


// recursively sorts the pairs vector
bool pairsort(const pair<char, int> &a, const pair<char, int> &b)
{
    if (a.second != b.second)
    {
        return a.second > b.second;
    }
    else
    {
        return int(a.first) < int(b.first);
    }
}

// creating huffman tree code above this line

struct ClientData
{
    int client_socket;
    Node *root;
};

char decode(Node *&root, string code)
{
    //cout << code << endl;
    Node *current = root;
    // string num_s = to_string(num);
    for (int i = 0; i < code.length(); i++)
    {
        char c = code[i];
        if (c == '0')
        {
            current = current->left;
        }
        else
        {
            current = current->right;
        }
    }
    return current->letter;
}

void *handle_client_request(void *data)
{
    //cout << "entered HCR function" << endl;
    ClientData *client_data = (ClientData *)data;
    int client_socket = client_data->client_socket;
    Node *root = client_data->root;
    char buffer[1024];

    // receive the binary code from the client program
    string binary_code;
    char buf;
    recv(client_socket, buffer, sizeof(buffer), 0);
    //cout << "Received data from client"<< buffer << endl;


    // Use Huffman tree to decode code
    char decoded_char = decode(root, buffer);
    //cout << decoded_char << endl;
    

    // Return the character to the client program using sockets
    send(client_socket, &decoded_char, sizeof(decoded_char), 0);

    close(client_socket);
    pthread_exit(NULL);
}

void fireman(int sig)
{
    while (waitpid(-1, 0, WNOHANG) > 0);
}

int main(int argc, char *argv[])
{

    int port_no = stoi(argv[1]);
    // string inputfile = argv[2];

    // Initialize the Huffman 
    int num_lines;
    string line;

    // string filename;
    // cin >> filename;

    // vector of pairs (letter, freq)
    vector<pair<char, int> > pairs;

    // counting lines in the .txt to define array size and storing letters/freq in a vector
    int line_count = 0;
    while (getline(cin, line))
    {
        line_count++;
        // converting char freq to int
        int num = line[2] - 48;

        pairs.push_back(make_pair(line[0], num));
    }

    // arrays
    char letters_array[line_count];
    int frequency_array[line_count];

    sort(pairs.begin(), pairs.end(), pairsort);

    for (int i = 0; i < line_count; i++)
    {
        letters_array[i] = pairs[i].first;
        frequency_array[i] = pairs[i].second;
    }
    int size = line_count;
    // cout << "test" << endl;

    Node *root = buildHuffmanTree(letters_array, frequency_array, size);

    label_edges(root, "");

    //server stuff
    //cout << "server" << endl;
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    signal(SIGCHLD, fireman);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_no);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    //cout << server_addr.sin_port << endl;
    //cout << server_addr.sin_addr.s_addr << endl;
    
    int option = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));


    // Bind socket to server address and port
    int b = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)); //bind returns a 0 working fine
    if (b == -1){
        cout << "bind failed" << endl;
    }
    //  Start listening for incoming connections
    listen(server_socket, 50); //working fine returns 0

    while (1)
    {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_len);
        //cout << "Received client socket: " << client_socket << endl;
        ClientData client_data;
        client_data.client_socket = client_socket;
        client_data.root = root;
        
        pid_t pid = fork();
        
        if (pid == 0)
        { // Child process
            //cout << "test1" << endl;
            handle_client_request((void *)&client_data);
            //cout << "test2" << endl;
            close(client_socket); // Close the client socket in the child process before exiting
            exit(0);
        }
        // Parent process
        close(client_socket); // Close the client socket in the parent process
    
  
    }

    

    return 0;
}