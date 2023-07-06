#include <iostream>
#include <thread>
#include <unistd.h>
#include <fstream>

using namespace std;

#define NUM_COMPUTE 10
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
std::atomic_flag compute_ready[WRITE_THREAD];
std::atomic_flag compute_valid[WRITE_THREAD];

std::mutex mtx[WRITE_THREAD+1];

unsigned input_mem[NUM_ELEM];
unsigned mem[WRITE_THREAD+1][NUM_ELEM];
unsigned output_mem[NUM_ELEM];

long long unsigned counter;

void read_input (unsigned exec_time_factor) {
    bitset<WRITE_THREAD+1> mem_arbiter;
    mem_arbiter.reset();
    mem_arbiter.set(0);
    // ofstream outfile;
    // outfile.open("read_input.log");

    // Test and Set producer ready flag
    while(input_prod_ready.test_and_set(std::memory_order_seq_cst));

    // Reset Compute 0 flag
    compute_valid[0].clear(std::memory_order_seq_cst);

    for (unsigned i = 0; i < ITERATIONS; i++) {
        // Wait for new input from producer
        while(!input_prod_valid.test(std::memory_order_seq_cst));

        // Wait for Compute 1 to be ready
        while(!compute_ready[0].test(std::memory_order_seq_cst));

        for (unsigned j = 0; j < WRITE_THREAD+1; j++) {
            if (mem_arbiter.test(j)) {
                // mtx[j].lock();
                for (unsigned k = 0; k < NUM_ELEM; k++) {
                    mem[j][k] = input_mem[k]+1;
                    // outfile << counter << " iterations = " << i << " input_mem = " << input_mem[k] << " mem " << j << " = " << mem[j][k] << endl;
                }
                // mtx[j].unlock();

                mem_arbiter.reset();
                if (j == WRITE_THREAD) mem_arbiter.set(0);
                else mem_arbiter.set(j+1);

                break;
            }
        }

        // Reset producer valid flag
        input_prod_valid.clear(std::memory_order_seq_cst);

        // Reset Compute 1 ready flag
        compute_ready[0].clear(std::memory_order_seq_cst);

        // Test and Set producer ready flag
        while(input_prod_ready.test_and_set(std::memory_order_seq_cst));

        // Test and Set Compute 1 flag
        while(compute_valid[0].test_and_set(std::memory_order_seq_cst));
    }
}

void store_output (unsigned exec_time_factor) {
    bitset<WRITE_THREAD+1> mem_arbiter;
    mem_arbiter.reset();
    mem_arbiter.set(0);
    // ofstream outfile;
    // outfile.open("store_output.log");

    // Test and Set Compute <NUM_COMPUTE> ready flag
    while(compute_ready[NUM_COMPUTE].test_and_set(std::memory_order_seq_cst));

    for (unsigned i = 0; i < ITERATIONS; i++) {
        // Test Compute <NUM_COMPUTE> flag
        while(!compute_valid[NUM_COMPUTE].test(std::memory_order_seq_cst));

        // Wait for consumer to be ready
        while(!output_cons_ready.test(std::memory_order_seq_cst));

        for (unsigned j = 0; j < WRITE_THREAD+1; j++) {
            if (mem_arbiter.test(j)) {
                // mtx[j].lock();
                for (unsigned k = 0; k < NUM_ELEM; k++) {
                    output_mem[k] = mem[j][k]+1;
                    // outfile << counter << " iterations = " << i << " mem " << j << " = " << mem[j][k] << " output_mem = " << output_mem[k] << endl;
                }
                // mtx[j].unlock();

                mem_arbiter.reset();
                if (j == WRITE_THREAD) mem_arbiter.set(0);
                else mem_arbiter.set(j+1);

                break;
            }
        }

        // Reset consumer ready flag
        output_cons_ready.clear(std::memory_order_seq_cst);

        // Reset Compute <NUM_COMPUTE> flag
        compute_valid[NUM_COMPUTE].clear(std::memory_order_seq_cst);

        // Test and Set consumer valid flag
        while(output_cons_valid.test_and_set(std::memory_order_seq_cst));

        // Test and Set Compute <NUM_COMPUTE> ready flag
        while(compute_ready[NUM_COMPUTE].test_and_set(std::memory_order_seq_cst));
    }
}

void compute_kernel (unsigned id, unsigned exec_time_factor) {
    bitset<WRITE_THREAD+1> mem_arbiter;
    mem_arbiter.reset();
    mem_arbiter.set(0);
    // ofstream outfile;
    // string filename = "compute_kernel_"+to_string(id)+".log";
    // outfile.open(filename);

    // Reset Compute <id+1> flag
    compute_valid[id+1].clear(std::memory_order_seq_cst);

    // Test and Set Compute <id> ready flag
    while(compute_ready[id].test_and_set(std::memory_order_seq_cst));

    for (unsigned i = 0; i < ITERATIONS; i++) {
        // Test Compute <id> valid flag
        while(!compute_valid[id].test(std::memory_order_seq_cst));

        // Test Compute <id+1> ready flag
        while(!compute_ready[id+1].test(std::memory_order_seq_cst));

        for (unsigned j = 0; j < WRITE_THREAD+1; j++) {
            if (mem_arbiter.test(j)) {
                // mtx[j].lock();
                for (unsigned k = 0; k < NUM_ELEM; k++) {
                    // outfile << counter << " iterations = " << i << " mem " << j << " = " << mem[j][k];
                    mem[j][k] = mem[j][k]+1;
                    // outfile << " mem " << j << " = " << mem[j][k] << endl;
                }
                // mtx[j].unlock();

                mem_arbiter.reset();
                if (j == WRITE_THREAD) mem_arbiter.set(0);
                else mem_arbiter.set(j+1);

                break;
            }
        }

        // Reset Compute <id> valid flag
        compute_valid[id].clear(std::memory_order_seq_cst);

        // Reset Compute <id+1> ready flag
        compute_ready[id+1].clear(std::memory_order_seq_cst);

        // Test and Set Compute <id+1> flag
        while(compute_valid[id+1].test_and_set(std::memory_order_seq_cst));

        // Test and Set Compute <id> ready flag
        while(compute_ready[id].test_and_set(std::memory_order_seq_cst));
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
            // outfile << counter << " iterations = " << i << " input_mem = " << input_mem[j] << endl;
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
            if (output_mem[j] != i+j+1+NUM_COMPUTE+1) {
                errors++;
            }
            // outfile << counter << " iterations = " << i << " output_mem = " << output_mem[j] << " expected = " << i+j+WRITE_THREAD+1 << endl;
            // cout << "actual = " << output_mem[j] << " expected = " << i+j+WRITE_THREAD+1 << endl;
        }

        cout << "Errors = " << errors << endl;

        // Reset consumer valid flag
        output_cons_valid.clear(std::memory_order_seq_cst);
    }
}

void counter_func() {
    while(true) {
        counter++;
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

    // thread counter_thread(counter_func);

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