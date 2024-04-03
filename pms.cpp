#include "mpi.h"
#include <fstream>
#include <iostream>
#include <queue>
#include <vector>

static int mpi_rank, mpi_size;

static int QUEUE_UP = 0;
static int QUEUE_DOWN = 1;

static int total_numbers = 6;

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
  return input_numbers; // Return the vector of numbers
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

  printf("Hello from %d of %d\n", mpi_rank, mpi_size);

  if (mpi_rank == 0) {
    // First processor
    vector<unsigned char> input_numbers = readNumbersFromFile();
    printNumbers(input_numbers);

    // Sequentially send numbers down the pipeline
    size_t i = 0;
    while (i < input_numbers.size()) {
      MPI_Send(&input_numbers[i], 1, MPI_UNSIGNED_CHAR, 1, QUEUE_UP,
               MPI_COMM_WORLD);
      cout << "First processor sent number: " << static_cast<int>(input_numbers[i]) << endl;

      MPI_Send(&input_numbers[i+1], 1, MPI_UNSIGNED_CHAR, 1, QUEUE_DOWN,
               MPI_COMM_WORLD);
      cout << "First processor sent number: " << static_cast<int>(input_numbers[i+1]) << endl;
      i += 2;
    }
  } else if (mpi_rank == mpi_size - 1) {
    
    // Last processor
    // While the queue is not empty, receive numbers from the previous processor
    for (int i = 0; i < 6; ++i) {
      unsigned char recv_number;
      MPI_Recv(&recv_number, 1, MPI_UNSIGNED_CHAR, mpi_rank - 1, QUEUE_DOWN,
               MPI_COMM_WORLD, &status);
      cout << "Last processor received number: " << static_cast<int>(recv_number) << endl;
    }

  } else {
    // Middle processors
    queue<unsigned char> queues[2];

    int i = 0;
    while (i < 3) {
      queue<unsigned char> *queue_up = &queues[QUEUE_UP];
      queue<unsigned char> *queue_down = &queues[QUEUE_DOWN];
      static int queue_up_size = pow(2, mpi_rank-1);

      int recv_number;
      MPI_Recv(&recv_number, 1, MPI_UNSIGNED_CHAR, mpi_rank - 1, QUEUE_DOWN,
               MPI_COMM_WORLD, &status);
      cout << mpi_rank << " Received number down: " << static_cast<int>(recv_number) << endl;
      queue_up->push(recv_number);
      
      MPI_Recv(&recv_number, 1, MPI_UNSIGNED_CHAR, mpi_rank - 1, QUEUE_UP,
               MPI_COMM_WORLD, &status);
      cout << mpi_rank << " Received number up: " << static_cast<int>(recv_number) << endl;
      queue_down->push(recv_number);

      // Send the number to the next processor
      MPI_Send(&queue_up->front(), 1, MPI_UNSIGNED_CHAR, mpi_rank + 1, QUEUE_UP,
               MPI_COMM_WORLD);
      cout << mpi_rank << " Sent number: " << static_cast<int>(queue_up->front()) << endl;
      queue_up->pop();
      MPI_Send(&queue_down->front(), 1, MPI_UNSIGNED_CHAR, mpi_rank + 1,
               QUEUE_DOWN, MPI_COMM_WORLD);
      cout << mpi_rank << " Sent number: " << static_cast<int>(queue_down->front()) << endl;
      queue_down->pop();
    }
  }

  MPI_Finalize();
  return 0;
}
