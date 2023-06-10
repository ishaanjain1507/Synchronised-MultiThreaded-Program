#include <iostream>
#include <fstream>
#include <vector>
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <random>
#include <atomic>
#include <semaphore.h>
#include <queue>

using namespace std;

// Parameters read from file
int P, C, k;
double lambda_p, lambda_c;

class combination
{
public:
    int rider, car;
};

queue<int> pass_id;
queue<combination> riding;

// Variables for logging
ofstream log_file;
mutex log_mutex, que_mutex;

// Semaphores for synchronization
sem_t request, car_available, passenger_done;
sem_t *car_mutex;

// Counters for number of rides completed
atomic_int rides_completed;

// ofstream fdata;          // Uncomment to store data to plot graphs

// Functions

// To get the system time in HH:MM:SS:mmm format.
string get_time()
{
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::tm *now_tm = std::localtime(&now_c);
    std::stringstream ss;
    ss << std::put_time(now_tm, "%H:%M:%S") << ":" << std::setfill('0') << std::setw(3) << now_ms.count();
    return ss.str();
}

// to print the message in out file without clashing with any other thread.
void log_message(const string &message)
{
    log_mutex.lock();
    log_file << message << endl;
    log_mutex.unlock();
}

void passenger_thread(int id)
{
    // Initialize random number generator
    default_random_engine generator(id);
    exponential_distribution<double> distribution_p(lambda_p);

    // Enter museum
    double enter_time = distribution_p(generator);
    log_message("Passenger " + to_string(id + 1) + " enters the museum at " + get_time());
    this_thread::sleep_for(chrono::duration<double>(enter_time));
    // auto start = chrono::high_resolution_clock::now();              ///uncomment to store data to plot graph

    // Ride k times
    for (int i = 0; i < k; i++)
    {
        // Request a ride
        log_message("Passenger " + to_string(id + 1) + " made a ride request at " + get_time());

        // Wait for a car to become available
        sem_wait(&car_available);

        que_mutex.lock();
        pass_id.push(id + 1);
        que_mutex.unlock();
        sem_post(&request);

        // wait for the ride to complet in the car_thread()
        sem_wait(&car_mutex[id]);

        // Exit car
        sem_post(&car_available);

        // sleep for sometime before next ride
        if (i != k - 1)
        {
            double ride_request_time = distribution_p(generator);
            this_thread::sleep_for(chrono::duration<double>(ride_request_time));
        }
    }

    sem_wait(&passenger_done);

    // Exit museum
    double exit_time = distribution_p(generator);
    log_message("Passenger " + to_string(id + 1) + " exits the museum at " + get_time());

    /*              // uncomment the bolow code to get data to plot graphs
        // auto stop = chrono::high_resolution_clock::now();
        // auto time_taken = std::chrono::duration_cast<std::chrono::microseconds>(stop - start);
        // fdata << time_taken.count() << endl;
    */
}

void car_thread(int id)
{
    // Initialize random number generator
    default_random_engine generator(id + P);
    exponential_distribution<double> distribution_c(lambda_c);

    combination temp;
    while (true)
    {
        // check if all rides are completed or not
        if (rides_completed == P * k)
            break;

        // Request for a ride is recieved
        sem_wait(&request);

        // logging with protection to the queues
        // ride has started
        que_mutex.lock();
        log_message("Car " + to_string(id + 1) + " accepts passenger " + to_string(pass_id.front()) + "\'s request at " + get_time());
        log_message("Car " + to_string(id + 1) + " is riding passenger " + to_string(pass_id.front()));
        temp.car = id + 1;
        temp.rider = pass_id.front();
        riding.push(temp);
        pass_id.pop();
        que_mutex.unlock();

        // simulating the ride time by sleeping
        double ride_time = distribution_c(generator);
        this_thread::sleep_for(chrono::duration<double>(ride_time));

        // ride completed
        que_mutex.lock();
        log_message("Car " + to_string(riding.front().car) + " finishes ride with passenger " + to_string(riding.front().rider) + " at " + get_time());

        // Signalling that the below passenger has completed its ride.
        sem_post(&car_mutex[riding.front().rider - 1]);
        riding.pop();
        que_mutex.unlock();

        // Incrementing the atomic_int variable to keep count of the total rides
        rides_completed++;

        sem_post(&passenger_done);
    }
}

int main()
{
    // Read parameters from file
    ifstream params_file("inp-params.txt");
    params_file >> P >> C >> lambda_p >> lambda_c >> k;
    params_file.close();

    // fdata.open("data.txt");          // Uncomment to store the data used to plot graphs

    // Initialize logging
    log_file.open("output.txt");

    // mutex to access each passenger entering/exiting a car.
    car_mutex = new sem_t[P];

    // Initialize semaphores
    sem_init(&car_available, 0, C);
    sem_init(&passenger_done, 0, 0);
    sem_init(&request, 0, 0);

    for (int i = 0; i < P; i++)
    {
        sem_init(&car_mutex[i], 0, 0);
    }

    // Initialize rides completed counter
    rides_completed = 0;

    // Start passenger threads
    vector<thread> passenger_threads;
    for (int i = 0; i < P; i++)
    {
        passenger_threads.emplace_back(passenger_thread, i);
    }

    // Start car threads
    vector<thread> car_threads;
    for (int i = 0; i < C; i++)
    {
        car_threads.emplace_back(car_thread, i);
    }

    // Wait for passenger threads to finish
    for (int i = 0; i < P; i++)
    {
        passenger_threads[i].join();
    }

    // Signal car threads to finish
    for (int i = 0; i < C; i++)
    {
        sem_post(&car_available);
    }

    // Wait for car threads to finish
    for (int i = 0; i < C; i++)
    {
        car_threads[i].join();
    }

    // Clean up
    sem_destroy(&car_available);
    sem_destroy(&passenger_done);
    sem_destroy(&request);
    log_file.close();
    for (int i = 0; i < P; i++)
        sem_destroy(&car_mutex[i]);
    delete car_mutex;
    // fdata.close();                  uncomment to store data to plot graphs
    return 0;
}