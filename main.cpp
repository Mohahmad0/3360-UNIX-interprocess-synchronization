#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <pthread.h>
#include <mutex>


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
    parent->letter = '\0'; // '\0' is used to mark the parent node
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

// Construct the Huffman tree
Node *buildHuffmanTree(char data[], int freq[], int n)
{
    Node *nodes[n];
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

    return nodes[0];
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

struct thread_data
{
    int thread_num;
    struct Node *huffman_root;
    vector<int> vec;
    string code;
    char *word_array;
    pthread_mutex_t *mutex; // mutex for word_array
    bool is_done;           // flag to see if thread is done
    pthread_cond_t *cond;   // cond. variable for signal
};

char decompress_file(Node *&root, string code, pthread_mutex_t *mutex)
{
    Node *current = root;
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

    cout << "Symbol: " << current->letter << ", Frequency: " << current->freq << ", Code: " << code << endl;

    return current->letter;
}

// storing the ddecompressed characters on a memory loction accessible by the main thread
void *thread_task(void *arg)
{
    
    struct thread_data *data = (struct thread_data *)arg;
    pthread_mutex_t *mutex = data->mutex;

//Critical Section 2
// ---------------------
    //lock mutex for accessing shared data 
    pthread_mutex_lock(mutex);
    int thread_number = data->thread_num;
    Node *root = data->huffman_root;
    vector<int> nums = data->vec;
    char *word = data->word_array;
    string code = data->code;
    pthread_mutex_unlock(mutex);
// ---------------------

    // find matching code in huffman tree, print "symbol: frequency: code:" 
    // then retun the letter
    char let = decompress_file(root, code, mutex);

    if (let == '\0')
    {
        let = '-';
    }
    
//critical section 3
// ---------------------
    // lock for accessing the shared array
    pthread_mutex_lock(mutex);
    for (int i = 0; i < nums.size(); i++)
    {
        // Building the "original message" in the shared array
        word[nums[i]] = let;
    }

    // signal to parent that thread has finished
    pthread_cond_signal(data->cond);
    data->is_done = true;

    pthread_mutex_unlock(mutex);
// ---------------------


    pthread_exit(NULL);
}

int main()
{

    // read line count from first line of input
    int line_count;
    cin >> line_count;

    // vector of pairs (letter, freq)
    vector<pair<char, int> > pairs;

    // cin adds a newline character as buffer
    // this discards the newline character after reading the alphabet so there isnt an empty vector
    string extra_line;
    getline(cin, extra_line);

    // read letters and frequencies from input
    char letter;
    int freq;
    string line;

    for (int i = 0; i < line_count; i++)
    {
        getline(cin, line);
        // cout << line << endl;
        letter = line[0];
        freq = line[2] - 48;
        pairs.push_back(make_pair(letter, freq));
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
    int size = sizeof(letters_array);

    Node *root = buildHuffmanTree(letters_array, frequency_array, size);

    //--------------------------------------------------------------
    // pthreads code to decompress the compressed file
    //--------------------------------------------------------------

    vector<string> comp_vec;

    // stores each line from input into a vector 
    for (int i = 0; i < line_count; i++)
    {
        getline(cin, line);
        // cout << line << endl;
        comp_vec.push_back(line);
    }

    // initialization
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_cond_t cond;
    pthread_cond_init(&cond, NULL);

    int num_threads = line_count;
    vector<int> comp_int;
    struct thread_data thread_data_instance;
    pthread_t thread;

    // defining the original message size 
    char size_array[root->freq];

    int th;

    // for loop that breaks each line of code and positions then adds it to a struct and starts a child thread
    // uses posix semaphores to achieve synchronization
    for (int i = 0; i < line_count; i++)
    {
        istringstream ss(comp_vec[i]);
        int num;
        string code;
        ss >> code;
        while (ss >> num){
            comp_int.push_back(num);
        }

        thread_data_instance.thread_num = i;
        thread_data_instance.huffman_root = root;
        thread_data_instance.code = code;
        thread_data_instance.vec = comp_int;
        thread_data_instance.word_array = size_array;
        thread_data_instance.mutex = &mutex;
        thread_data_instance.cond = &cond;
        thread_data_instance.is_done = false;

        th = pthread_create(&thread, NULL, thread_task, &thread_data_instance);


    //critical section 1
    // ---------------------
        // lock before checking if thread has finished
        pthread_mutex_lock(&mutex);
        while (!thread_data_instance.is_done)
        {
            // wait for thread to signal if finished
            pthread_cond_wait(&cond, &mutex);
        }
        // unlock after thread is finished 
        pthread_mutex_unlock(&mutex);
    // ---------------------

        // clear vector for next line of positions
        comp_int.clear();

    }

    // join threads
    for (int i = 0; i < line_count; i++){
        th = pthread_join(thread, NULL);
    }

    // print shared array
    cout << "Original message: ";
    for (int i = 0; i < root->freq; i++)
    {
        cout << thread_data_instance.word_array[i];
    }

    // Destroy the mutex and condition variable
    pthread_mutex_destroy(&mutex);

    return 0;
}