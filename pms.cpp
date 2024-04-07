#include "mpi.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>
#include <unistd.h>

static int mpi_rank, mpi_size;

static int QUEUE_UP = 0;
static int QUEUE_DOWN = 1;

using namespace std;

// file name constant
const char *FILE_NAME = "numbers";

std::vector<unsigned char> readNumbersFromFile() {
  ifstream file(FILE_NAME,
                ios::binary);          // Open the file in binary mode
  vector<unsigned char> input_numbers; // Vector to store the numbers

  if (!file.is_open()) {
    cerr << "Failed to open file." << endl;
    return input_numbers; // Return an empty vector
  }

  char num;
  while (file.read(&num, sizeof(num))) { // Read one byte at a time
    input_numbers.push_back(
        static_cast<unsigned char>(num)); // Store the number in the vector
  }

  file.close();         // Close the file

  //return input_numbers; // Return the vector
  //unsigned char arr[] = {5, 2, 9, 4, 7, 6, 8, 1};
  unsigned char arr[] = {139, 126, 207, 134, 62, 81, 184, 156}; 
  std::vector<unsigned char> vec(arr, arr + sizeof(arr)/sizeof(arr[0]));
  return vec;
}

void printNumbers(vector<unsigned char> &input_numbers) {
  for (size_t i = 0; i < input_numbers.size(); ++i) {
    cout << static_cast<int>(input_numbers[i]) << " ";
  }
  cout << endl;
}

void firstProcessor(vector<unsigned char> &input_numbers) {
    int queue_current = QUEUE_UP;
    // Sequentially send numbers down the pipeline
    for (int i = input_numbers.size() - 1; i >= 0; i--) {
      MPI_Send(&input_numbers[i], 1, MPI_UNSIGNED_CHAR, 1, queue_current,
               MPI_COMM_WORLD);
      queue_current = (queue_current + 1) % 2;
    }
}

void lastProcessor(int numbers_total_count ) {
    MPI_Status status;

    static int queue_max_size = pow(2, mpi_rank - 1);
    int queue_current = QUEUE_UP;
    int numbers_processed_count = 0;
    int queue_current_received = 0;
    queue<unsigned char> queues[2];
    vector<unsigned char> output_numbers;

    bool ready_to_compare = false;

    while (numbers_processed_count < numbers_total_count) {
      unsigned char recv_number;
      if (queue_current_received < numbers_total_count) {
        MPI_Recv(&recv_number, 1, MPI_UNSIGNED_CHAR, mpi_rank - 1, queue_current, MPI_COMM_WORLD, &status);
        queue_current_received++;
        queues[queue_current].push(recv_number);
      }
      if (queues[QUEUE_UP].size() == queue_max_size && queues[QUEUE_DOWN].size() == 1)
        ready_to_compare = true;

      if (ready_to_compare && !queues[QUEUE_UP].empty() && !queues[QUEUE_DOWN].empty()) {
        // We can compare numbers
        unsigned char number_up = queues[QUEUE_UP].front();
        unsigned char number_down = queues[QUEUE_DOWN].front();

        if (number_up < number_down) {
          output_numbers.push_back(number_up);
          queues[QUEUE_UP].pop();
        } else {
          output_numbers.push_back(number_down);
          queues[QUEUE_DOWN].pop();
        } 
        numbers_processed_count++;
      }
      if ((queues[QUEUE_DOWN].empty() || queues[QUEUE_UP].empty()) && (numbers_total_count - numbers_processed_count <= queue_max_size)) {
        int queue_empty = queues[QUEUE_DOWN].empty() ? QUEUE_DOWN : QUEUE_UP;

        while (!queues[!queue_empty].empty()) {
          output_numbers.push_back(queues[!queue_empty].front());
          queues[!queue_empty].pop();
          numbers_processed_count++;
        }
      }
      if (queue_current_received == queue_max_size) {
        // Switch the input queue
        queue_current = (queue_current + 1) % 2;
      } 
    }

    // Write the output numbers to stdout
    for (size_t i = 0; i < output_numbers.size(); ++i) {
      cout << static_cast<int>(output_numbers[i]) << endl;
    }
}

void middleProcessor(int numbers_total_count) {
    MPI_Status status;
    queue<unsigned char> queues[2];

    static int queue_max_size = pow(2, mpi_rank - 1);

    int numbers_processed_count = 0;
    int queue_input_current = QUEUE_UP;
    int queue_output_current = QUEUE_UP;
    int queue_current_received = 0;

    bool ready_to_compare = false;

    while (numbers_processed_count < numbers_total_count) {
    }
}
int main(int argc, char *argv[]) {

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  vector<unsigned char> input_numbers;

  if (mpi_rank == 0) {
    // First processor
    input_numbers = readNumbersFromFile();
    printNumbers(input_numbers);
  }

  // Broadcast the size of input_numbers to all processors
  int numbers_total_count;
  if (mpi_rank == 0) {
    numbers_total_count = input_numbers.size();
  }
  MPI_Bcast(&numbers_total_count, 1, MPI_INT, 0, MPI_COMM_WORLD);

  // Allocate memory for input_numbers in all processors
  input_numbers.resize(numbers_total_count);

  // Broadcast the input_numbers to all processors
  //MPI_Bcast(input_numbers.data(), numbers_total_count, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);

  if (mpi_rank == 0) {
    firstProcessor(input_numbers);
  } else if (mpi_rank == mpi_size - 1) {
    lastProcessor(numbers_total_count);
  } else {
    middleProcessor(numbers_total_count);
  }

  MPI_Barrier(MPI_COMM_WORLD); // Synchronize all processes before finalizing
  MPI_Finalize();
  return 0;
}
