/******************
LE5:
A Practical Exercise in Concurrency
*******************/
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <mutex>

using timepoint = std::chrono::time_point<std::chrono::steady_clock>;

// BankAccount class definition
class BankAccount
{
private:
    int64_t balance;
    std::mutex m;

public:
    BankAccount() : balance(0) {}

    // Interface function to perform transactions
    void perform_transaction(const int64_t amount)
    {
        int64_t temp = balance;
        temp += amount;
        std::this_thread::sleep_for(std::chrono::microseconds(rand() % 50));
        balance = temp;
    }

    // Interface function to perform threadsafe transactions
    void perform_threadsafe_transaction(const int64_t amount)
    {
        // LOCK other threads from using transaction() at the same time
        std::lock_guard<std::mutex> lock(m);
        perform_transaction(amount);
        // UNLOCK for the next thread to access transaction()
    }

    void print_balance() const
    {
        std::string currency = this->balance < 0 ? "-$" : "$";
        std::cout << currency << llabs(this->balance);
    }
};

// Helper function for printing relevant information
void print_helper(const int _approach_num, const timepoint& start, const timepoint& end, const BankAccount& acc)
{
    using std::literals::chrono_literals::operator""ms;

    std::cout << "Approach " << _approach_num << " took " << (end - start) / 1ms;
    std::cout << "ms to achieve a final balance of ";
    acc.print_balance();
    std::cout << std::endl;
}

// Thread function the teller will use to process a transaction
void teller_thread(BankAccount* const acc, const int64_t _amount)
{
    // TODO #1: call perform_transaction() function for acc
    acc->perform_transaction(_amount);
}

// Thread function the teller will use to process a threadsafe transaction
void teller_threadsafe(BankAccount* const acc, const int64_t _amount)
{
    // TODO #2: call perform_threadsafe_transaction() function for acc
    acc->perform_threadsafe_transaction(_amount);
}

int main(int argc, char* argv[])
{
    // Variables for whole runtime
    std::string fname = "transactions.csv"; // name of input file
    constexpr size_t num_trans = 2000;      // number of transactions

    // getopt to read flags
    // "your program's input will be the =i flag for the name of the file"
    int opt;
    while ((opt = getopt(argc, argv, "i:")) != -1)
        if (opt == 'i') fname = optarg;

    // Fill the contents of the file into an array
    std::ifstream in_file(fname);
    if (!in_file.is_open())
    {
        std::cerr << "Could not open " << fname << " . Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }

    std::array<int64_t, num_trans> trans_arr;
    for (size_t i = 0; i < num_trans; ++i) in_file >> trans_arr[i];
    in_file.close();

    // (totally complete) Approach 1: completely synchronous
    BankAccount A;
    const timepoint start1 = std::chrono::steady_clock::now();
    for (const int64_t& t : trans_arr) A.perform_transaction(t);
    const timepoint end1 = std::chrono::steady_clock::now();
    print_helper(1, start1, end1, A);

    // (partially complete) APPROACH 2: a vector of threads to carry out each transaction
    BankAccount B;
    std::vector<std::thread> t_vec;
    const timepoint start2 = std::chrono::steady_clock::now();
    // TODO #3: create *num_trans* threads and push_back() each one to t_vec
    for (const int64_t& t : trans_arr)
    {
        t_vec.push_back(std::thread(teller_thread, &B, t));
    }
    for (std::thread& t : t_vec) t.join();
    const timepoint end2 = std::chrono::steady_clock::now();
    print_helper(2, start2, end2, B);

    // (totally incomplete) APPROACH 3: implement mutex to ensure that each transaction
    // will not affect another transaction
    BankAccount C;
    std::vector<std::thread> t_vec2;
    const timepoint start3 = std::chrono::steady_clock::now();
    // TODO #4: create *num_trans* threads and push_back() each one to t_vec2
    for (const int64_t& t : trans_arr)
    {
        t_vec2.push_back(std::thread(teller_threadsafe, &C, t));
    }
    // TODO #5: join all threads in t_vec2
    for (std::thread& t : t_vec2) t.join();
    
    // End timer and print result
    const timepoint end3 = std::chrono::steady_clock::now();
    print_helper(3, start3, end3, C);

    return 0;
}
