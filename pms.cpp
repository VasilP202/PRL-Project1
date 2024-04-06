#include "mpi.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

static int mpi_rank, mpi_size;

static int QUEUE_UP = 0;
static int QUEUE_DOWN = 1;

static int numbers_total_count = 6;

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

  //unsigned char arr[] = {5, 2, 9, 4, 7, 6, 8, 1};
  //std::vector<unsigned char> vec(arr, arr + sizeof(arr)/sizeof(arr[0]));
  //return vec;
  return input_numbers; // Return the vector
}

void printNumbers(vector<unsigned char> &input_numbers) {
  cout << "Numbers read from file:" << endl;
  for (size_t i = 0; i < input_numbers.size(); ++i) {
    cout << static_cast<int>(input_numbers[i]) << " "; // Print each number
  }
  cout << endl;
}

int main(int argc, char *argv[]) {
  MPI_Status status;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
  MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);

  if (mpi_rank == 0) {
    // First processor
    vector<unsigned char> input_numbers = readNumbersFromFile();
    printNumbers(input_numbers);

    int queue_current = QUEUE_UP;
    // Sequentially send numbers down the pipeline
    for (int i = input_numbers.size() - 1; i >= 0; i--) {
      MPI_Send(&input_numbers[i], 1, MPI_UNSIGNED_CHAR, 1, queue_current,
               MPI_COMM_WORLD);
      queue_current = (queue_current + 1) % 2;
    }
  } else if (mpi_rank == mpi_size - 1) {
    // Last processor
    static int queue_max_size = pow(2, mpi_rank - 1);
    int queue_current = QUEUE_UP;
    int numbers_processed_count = 0;
    int queue_current_received = 0;
    queue<unsigned char> queues[2];
    vector<unsigned char> output_numbers;

    bool ready_to_compare = false;

    while (numbers_processed_count < numbers_total_count) {
      printf("Last processor: %d\n", numbers_processed_count);
      unsigned char recv_number;
      if (queue_current_received < numbers_total_count) {
        MPI_Recv(&recv_number, 1, MPI_UNSIGNED_CHAR, mpi_rank - 1, queue_current, MPI_COMM_WORLD, &status);
        queue_current_received++;
        printf("Last processor received number (%d): %d from processor %d\n", queue_current, static_cast<int>(recv_number), mpi_rank - 1);
        queues[queue_current].push(recv_number);
      }
      if (queues[QUEUE_UP].size() == queue_max_size && queues[QUEUE_DOWN].size() == 1)
        ready_to_compare = true;

      if (ready_to_compare && !queues[QUEUE_UP].empty() && !queues[QUEUE_DOWN].empty()) {
        // We can compare numbers
        unsigned char number_up = queues[QUEUE_UP].front();
        unsigned char number_down = queues[QUEUE_DOWN].front();
        printf("Last processor has enough numbers to compare: %d and %d\n", static_cast<int>(number_up), static_cast<int>(number_down));

        if (number_up < number_down) {
          output_numbers.push_back(number_up);
          queues[QUEUE_UP].pop();
          printf("Pushing %d \n", static_cast<int>(number_up));
        } else {
          output_numbers.push_back(number_down);
          queues[QUEUE_DOWN].pop();
          printf("Pushing %d \n", static_cast<int>(number_down));

        } 
        numbers_processed_count++;
      }
      if ((queues[QUEUE_DOWN].empty() || queues[QUEUE_UP].empty()) && (numbers_total_count - numbers_processed_count <= queue_max_size)) {
        // Send the remaining numbers to the last processor
        printf("Last processor has remaining numbers %d\n", numbers_total_count - numbers_processed_count);
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
    cout << "Numbers sorted in ascending order:" << endl;
    for (size_t i = 0; i < output_numbers.size(); ++i) {
      cout << static_cast<int>(output_numbers[i]) << " ";
    }

  } else {
    // Middle processors
    queue<unsigned char> queues[2];

    int numbers_processed_count = 0;
    static int queue_max_size = pow(2, mpi_rank - 1);
    int queue_input_current = QUEUE_UP;
    int queue_output_current = QUEUE_UP;
    int queue_current_sent = 0;
    int queue_current_received = 0;

    int current_number;

    bool ready_to_compare = false;

    while (numbers_processed_count < numbers_total_count) {
      unsigned char recv_number;
      MPI_Recv(&recv_number, 1, MPI_UNSIGNED_CHAR, mpi_rank - 1, queue_input_current, MPI_COMM_WORLD, &status);
      queue_current_received++;
      printf("Processor %d received number (%d): %d from processor %d\n", mpi_rank, queue_input_current, static_cast<int>(recv_number), mpi_rank - 1);
      queues[queue_input_current].push(recv_number);

      if (queues[QUEUE_UP].size() == queue_max_size && queues[QUEUE_DOWN].size() == 1)
        ready_to_compare = true;

      if (ready_to_compare && !queues[QUEUE_UP].empty() && !queues[QUEUE_DOWN].empty()){
        printf("\nProcessor %d has enough numbers to send\n", mpi_rank);
        // We can compare numbers
        unsigned char number_up = queues[QUEUE_UP].front();
        unsigned char number_down = queues[QUEUE_DOWN].front();

        if (number_up < number_down) {
          MPI_Send(&number_up, 1, MPI_UNSIGNED_CHAR, mpi_rank + 1, queue_output_current, MPI_COMM_WORLD);
          printf("Processor %d sent number (%d): %d to processor %d\n", mpi_rank, queue_output_current, static_cast<int>(number_up), mpi_rank + 1);
          queues[QUEUE_UP].pop();
          MPI_Send(&number_down, 1, MPI_UNSIGNED_CHAR, mpi_rank + 1, queue_output_current, MPI_COMM_WORLD);
          printf("Processor %d sent number (%d): %d to processor %d\n", mpi_rank, queue_output_current, static_cast<int>(number_down), mpi_rank + 1);
          queues[QUEUE_DOWN].pop();
          queue_current_sent += 2;
        } else {
          MPI_Send(&number_down, 1, MPI_UNSIGNED_CHAR, mpi_rank + 1, queue_output_current, MPI_COMM_WORLD);
          printf("Processor %d sent number (%d): %d to processor %d\n", mpi_rank, queue_output_current, static_cast<int>(number_down), mpi_rank + 1);
          queues[QUEUE_DOWN].pop();
          MPI_Send(&number_up, 1, MPI_UNSIGNED_CHAR, mpi_rank + 1, queue_output_current, MPI_COMM_WORLD);
          printf("Processor %d sent number (%d): %d to processor %d\n", mpi_rank, queue_output_current, static_cast<int>(number_up), mpi_rank + 1);
          queues[QUEUE_UP].pop();
          queue_current_sent += 2;
        }
        printf("\n");
        numbers_processed_count += 2;
      }
        //printf("%d processed count: %d, current sent: %d\n", mpi_rank, numbers_processed_count, queue_current_sent);

      if (queue_current_sent == queue_max_size * 2) {
        // Switch to the other output queue
        queue_output_current = (queue_output_current + 1) % 2;
        queue_current_sent = 0;
        //printf("%d switched to output queue %d\n", mpi_rank, queue_output_current);
      }
      if (queue_current_received == queue_max_size) {
        // Switch the input queue
        queue_input_current = (queue_input_current + 1) % 2;
        queue_current_received = 0;
        //printf("%d switched to input queue %d\n", mpi_rank, queue_input_current);
      }

      if (mpi_rank == 2) {
        printf("Queue up size: %d, Queue down size: %d\n", queues[QUEUE_UP].size(), queues[QUEUE_DOWN].size());
        printf("%d %d %d %d %d\n", queues[QUEUE_DOWN].empty(), queues[QUEUE_UP].empty(), numbers_total_count, numbers_processed_count, queue_max_size);
      }

      if (queues[QUEUE_DOWN].empty() && !queues[QUEUE_UP].empty() && (numbers_total_count - numbers_processed_count <= queue_max_size)) {
        if (mpi_rank == 2)
        printf("Processor %d has remaining numbers %d\n", mpi_rank,numbers_total_count - numbers_processed_count );
        // Send the remaining numbers to the last processor
        while (!queues[QUEUE_UP].empty()) {
          unsigned char number = queues[QUEUE_UP].front();
          MPI_Send(&number, 1, MPI_UNSIGNED_CHAR, mpi_size - 1, queue_output_current, MPI_COMM_WORLD);
          printf("Processor %d sent number (%d): %d to processor %d\n", mpi_rank, queue_output_current, static_cast<int>(number), mpi_size - 1);
          queues[QUEUE_UP].pop();
          queue_current_sent++;
          numbers_processed_count++;
        }
      }
    }
  }

  MPI_Barrier(MPI_COMM_WORLD); // Synchronize all processes before finalizing
  MPI_Finalize();
  return 0;
}
