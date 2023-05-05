#include <iostream>
#include <thread>
#include <unistd.h>
#include <fstream>

using namespace std;

#define NUM_COMPUTE 3
#define READ_THREAD 0
#define WRITE_THREAD (READ_THREAD + NUM_COMPUTE + 1)
#define ITERATIONS 100
#define NUM_ELEM 100

// Read sync variables
std::atomic_flag input_prod_ready;
std::atomic_flag input_prod_valid;

// Write sync variables
std::atomic_flag output_cons_ready;
std::atomic_flag output_cons_valid;

// Compute sync variables
std::atomic_flag compute_valid[WRITE_THREAD];

std::mutex ping_mutex[WRITE_THREAD];
std::mutex pong_mutex[WRITE_THREAD];

unsigned input_mem[NUM_ELEM];
unsigned ping_mem[WRITE_THREAD][NUM_ELEM];
unsigned pong_mem[WRITE_THREAD][NUM_ELEM];
unsigned output_mem[NUM_ELEM];

void read_input (unsigned exec_time_factor) {
    bool pingpong = true;
    // ofstream outfile;
    // outfile.open("read_input.log");

    // Test and Set producer ready flag
    while(input_prod_ready.test_and_set(std::memory_order_seq_cst));

    // Reset Compute 0 flag
    compute_valid[0].clear(std::memory_order_seq_cst);

    for (unsigned i = 0; i < ITERATIONS; i++) {
        // Wait for new input from producer
        while(!input_prod_valid.test(std::memory_order_seq_cst));

        if (pingpong) {
            // ping_mutex[0].lock();
            for (unsigned j = 0; j < NUM_ELEM; j++) {
                ping_mem[0][j] = input_mem[j]+1;
                // outfile << "iterations = " << i << " input_mem = " << input_mem[j] << " ping_mem = " << ping_mem[0][j] << endl;
            }
            // ping_mutex[0].unlock();
        } else {
            // pong_mutex[0].lock();
            for (unsigned j = 0; j < NUM_ELEM; j++) {
                pong_mem[0][j] = input_mem[j]+1;
                // outfile << "iterations = " << i << " input_mem = " << input_mem[j] << " pong_mem = " << pong_mem[0][j] << endl;
            }
            // pong_mutex[0].unlock();
        }

        // Reset producer valid flag
        input_prod_valid.clear(std::memory_order_seq_cst);

        // Test and Set producer ready flag
        while(input_prod_ready.test_and_set(std::memory_order_seq_cst));

        // Test and Set Compute 1 flag
        while(compute_valid[0].test_and_set(std::memory_order_seq_cst));

        pingpong = !pingpong;
    }
}

void store_output (unsigned exec_time_factor) {
    bool pingpong = true;
    // ofstream outfile;
    // outfile.open("store_output.log");

    for (unsigned i = 0; i < ITERATIONS; i++) {
        // Test Compute <NUM_COMPUTE> flag
        while(!compute_valid[NUM_COMPUTE].test(std::memory_order_seq_cst));

        // Wait for consumer to be ready
        while(!output_cons_ready.test(std::memory_order_seq_cst));

        if (pingpong) {
            // ping_mutex[NUM_COMPUTE].lock();
            for (unsigned j = 0; j < NUM_ELEM; j++) {
                output_mem[j] = ping_mem[NUM_COMPUTE][j]+1;
                // outfile << "iterations = " << i << " ping_mem = " << ping_mem[NUM_COMPUTE][j] << " output_mem = " << output_mem[j] << endl;
            }
            // ping_mutex[NUM_COMPUTE].unlock();
        } else {
            // pong_mutex[NUM_COMPUTE].lock();
            for (unsigned j = 0; j < NUM_ELEM; j++) {
                output_mem[j] = pong_mem[NUM_COMPUTE][j]+1;
                // outfile << "iterations = " << i << " pong_mem = " << pong_mem[NUM_COMPUTE][j] << " output_mem = " << output_mem[j] << endl;
            }
            // pong_mutex[NUM_COMPUTE].unlock();
        }

        // Reset consumer ready flag
        output_cons_ready.clear(std::memory_order_seq_cst);

        // Reset Compute <NUM_COMPUTE> flag
        compute_valid[NUM_COMPUTE].clear(std::memory_order_seq_cst);

        // Test and Set consumer valid flag
        while(output_cons_valid.test_and_set(std::memory_order_seq_cst));

        pingpong = !pingpong;
    }
}

void compute_kernel (unsigned id, unsigned exec_time_factor) {
    bool pingpong = true;
    // ofstream outfile;
    // string filename = "compute_kernel_"+to_string(id)+".log";
    // outfile.open(filename);

    // Reset Compute <id+1> flag
    compute_valid[id+1].clear(std::memory_order_seq_cst);

    for (unsigned i = 0; i < ITERATIONS; i++) {
        // Test Compute <id> flag
        while(!compute_valid[id].test(std::memory_order_seq_cst));

        if (pingpong) {
            // ping_mutex[id].lock();
            // ping_mutex[id+1].lock();
            for (unsigned j = 0; j < NUM_ELEM; j++) {
                ping_mem[id+1][j] = ping_mem[id][j]+1;
                // outfile << "iterations = " << i << " ping_mem = " << ping_mem[id][j] << " ping_mem = " << ping_mem[id+1][j] << endl;
            }
            // ping_mutex[id].unlock();
            // ping_mutex[id+1].unlock();
        } else {
            // pong_mutex[id].lock();
            // pong_mutex[id+1].lock();
            for (unsigned j = 0; j < NUM_ELEM; j++) {
                pong_mem[id+1][j] = pong_mem[id][j]+1;
                // outfile << "iterations = " << i << " pong_mem = " << pong_mem[id][j] << " pong_mem = " << pong_mem[id+1][j] << endl;
            }
            // pong_mutex[id].unlock();
            // pong_mutex[id+1].unlock();
        }

        // Reset Compute <id> flag
        compute_valid[id].clear(std::memory_order_seq_cst);

        // Test and Set Compute <id+1> flag
        while(compute_valid[id+1].test_and_set(std::memory_order_seq_cst));

        pingpong = !pingpong;
    }
}

void host_input_write (unsigned exec_time_factor) {
    // ofstream outfile;
    // outfile.open("host_input_write.log");

    for (unsigned i = 0; i < ITERATIONS; i++) {
        // Wait for consumer to be ready to accept new input
        while(!input_prod_ready.test(std::memory_order_seq_cst));

        for (unsigned j = 0; j < NUM_ELEM; j++) {
            input_mem[j] = i+j;
            // outfile << "iterations = " << i << " input_mem = " << input_mem[j] << endl;
        }

        // Reset producer valid flag
        input_prod_ready.clear(std::memory_order_seq_cst);

        // Test and Set producer valid flag
        while(input_prod_valid.test_and_set(std::memory_order_seq_cst));
    }
}

void host_output_read (unsigned exec_time_factor) {
    // ofstream outfile;
    // outfile.open("host_output_read.log");

    for (unsigned i = 0; i < ITERATIONS; i++) {
        unsigned errors = 0;

        // Test and Set consumer ready flag
        while(output_cons_ready.test_and_set(std::memory_order_seq_cst));

        // Test consumer valid flag
        while(!output_cons_valid.test(std::memory_order_seq_cst));

        for (unsigned j = 0; j < NUM_ELEM; j++) {
            if (output_mem[j] != i+j+WRITE_THREAD+1) {
                errors++;
            }
            // outfile << "iterations = " << i << " output_mem = " << output_mem[j] << " expected = " << i+j+WRITE_THREAD+1 << endl;
            // cout << "actual = " << output_mem[j] << " expected = " << i+j+WRITE_THREAD+1 << endl;
        }

        cout << "Errors = " << errors << endl;

        // Reset consumer valid flag
        output_cons_valid.clear(std::memory_order_seq_cst);
    }
}

int main () {
    unsigned host_input_factor = 1;
    unsigned host_output_factor = 1;
    unsigned read_factor = 1;
    unsigned write_factor = 1;
    unsigned compute_factor[NUM_COMPUTE];
    for (int i = 0; i < NUM_COMPUTE; i++) {
        compute_factor[i] = 1;
    }
    
    // Start threads
    thread host_input_thread(host_input_write, host_input_factor);
    thread host_output_thread(host_output_read, host_output_factor);
    thread read_thread(read_input, read_factor);
    thread write_thread(store_output, write_factor);
    thread compute_thread[NUM_COMPUTE];

    for (int i = 0; i < NUM_COMPUTE; i++) {
        compute_thread[i] = thread(compute_kernel, i, compute_factor[i]);
    }

    // End threads
    host_input_thread.join();
    host_output_thread.join();
    read_thread.join();
    write_thread.join();
    for (int i = 0; i < NUM_COMPUTE; i++) {
        compute_thread[i].join();
    }

    return 0;
}