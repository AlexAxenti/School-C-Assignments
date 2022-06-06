#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define PAGE_SIZE 256 // 256 bytes
#define OFFSET_MASK 255 // 2^8 - 1
#define OFFSET_BITS 8 // Page size is 256 which is 2^8, so exponent of 8
#define FRAMES 128 // 2^(15-8)
#define BUFFER_SIZE 10
#define BACKING_STORE_SIZE 65536 

// Struct for a entry to the TLB, containing it's page number and corresponding frame number
struct TLBentry{
    int page_number;
    int frame_number;
};

int page_table[256] = {[0 ... 255] = -1}; // 2^16 address space - 2^8 page size = 2^8 or 256 pages, all initiated to -1.

signed char physical_memory[FRAMES * PAGE_SIZE]; // 128 frames, each of size 256
// Once current_memory_frame and oldest_memory_frame sync up, pages will start getting replaced, works as FIFO.
int current_memory_frame = 0;
int oldest_memory_frame = 127;

struct TLBentry TLB[16] = {[0 ... 15] = -1}; // Array containg the 16 TLB entries, all have page_number initiated to -1.
// Once current_TLB_entry and oldest_TLB_entry sync up, pages will start getting replaced, works as FIFO.
int current_TLB_entry = 0;
int oldest_TLB_entry = 15;

// Definitions of functions called below
void backing_store_page_request(int page);
int search_TLB(int page);
void TLB_Add(int page, int frame);
void TLB_Update(int old_page, int new_page, int new_frame);
int find_page_in_memory(int frame);

int main(){
    // Measures that are kept track of
    int page_fault_count = 0;
    int hit_count = 0;
    int address_count = 0;

    // Opens addresses.txt and ensures there was no error
    FILE *fptr = fopen("addresses.txt", "r");
    if (fptr == NULL) {
        printf("ERROR:FileCouldNotBeLoadedException\n");
        return -1;
    }

    char buff[BUFFER_SIZE];

    // Reads one line at a time and places it into buff
    while(fgets(buff, BUFFER_SIZE, fptr) != NULL){
        address_count += 1;
        int virtual_address = atoi(buff); // converts the read string to integer
        int page_number = virtual_address >> OFFSET_BITS; // right shifts the offset bits to get the remaining bits representing page number
        int poffset = virtual_address & OFFSET_MASK; // selects only the bits within the offset mask
        int physical_address;
        int frame_number;

        // Attempts to find the index of a TLB entry with the given page number, and then searches for a page's frame number
        int TLB_lookup = search_TLB(page_number); 
        if (TLB_lookup == -1){ // TLB Miss
            if(page_table[page_number] == -1){ // Page fault
                backing_store_page_request(page_number); // Add the page to memory
                page_fault_count += 1;
            }
            frame_number = page_table[page_number];
            TLB_Add(page_number, frame_number); // Add a TLB entry with the new page and frame
        } else { // TLB Hit
            frame_number = TLB[TLB_lookup].frame_number;
            hit_count += 1;
        }

        physical_address = (frame_number << OFFSET_BITS) | poffset;
        signed char physical_address_value = physical_memory[physical_address]; // Gets the signed byte value stored at the calculated physical address
        
        printf("Virtual Address = %d, Phsyical address = %d, Value = %d\n", virtual_address, physical_address, physical_address_value);
    }
    fclose(fptr);

    printf("Total Addresses = %d \nPage Faults = %d \nTLB Hits = %d\n", address_count, page_fault_count, hit_count);

    return 0;
}

// Given a page number, try to find that page number in the TLB and if it exists then return it's index, otherwise return -1
int search_TLB(int page){
    for(int i = 0; i < 16; i++){
        if (TLB[i].page_number == page){
            return i;
        }
    }
    return -1;
}

// Given a page number and frame number, add a new entry to the TLB array
void TLB_Add(int page, int frame){
    // When the current entry index reaches the oldest entry index, start increasing the oldest entry index and start replacing TLB entries, represents FIFO.
    if(current_TLB_entry == oldest_TLB_entry){
        if(oldest_TLB_entry == 15) {
            oldest_TLB_entry = 0;
        } else {
            oldest_TLB_entry += 1;
        }
    }
    
    // Add the new page number and frame number to the TLB array
    TLB[current_TLB_entry].page_number = page;
    TLB[current_TLB_entry].frame_number = frame;

    // Increment the current entry index so that it keeps repeating and TLB is accessed like a circular array.
    if (current_TLB_entry == 15) {
        current_TLB_entry = 0;
    } else {
        current_TLB_entry += 1;
    }
}

// Given a old page number, a new page number, and a new frame number, replace the entry with the old page number with the new values.
void TLB_Update(int old_page, int new_page, int new_frame){
    // Check if there is an entry in TLB with the old page
    int updated_index = search_TLB(old_page); 
    // If an entry exists, replace it's page number and frame number
    if(updated_index != -1) {
        TLB[updated_index].page_number = new_page;
        TLB[updated_index].frame_number = new_frame;
    }
}

// Given a page number, add it to phyiscal memory from the backing store
void backing_store_page_request(int page) {
    char *mmapfptr; // Pointer to where the memory is mapped
    int mmapfile_fd = open("BACKING_STORE.bin", O_RDONLY); // file descriptor

    // Map the BACKING_STORE.bin file to memory
    mmapfptr = mmap(0, BACKING_STORE_SIZE, PROT_READ, MAP_PRIVATE, mmapfile_fd, 0); // Map the data

    // Checks whether a page is getting replaced or not in the physical memory
    if(current_memory_frame == oldest_memory_frame){
        int replaced_page = find_page_in_memory(oldest_memory_frame); // Finds the page number that is getting replaced
        TLB_Update(replaced_page, page, current_memory_frame); // Updates the replaced page number with the new one in TLB
        page_table[replaced_page] = -1; // Removes the replaced page number from the page table
        // Increments the oldest memory frame index
        if (oldest_memory_frame == 127){
            oldest_memory_frame = 0;
        } else {
            oldest_memory_frame += 1;
        }
    }

    // Changes the value in the phyiscal memory array. Each frame is 256 (same as page size), so goes to current frame index * 256.
    // Each page in the mapped memory is 256 bytes, so goes to the current page number * 256 
    memcpy(physical_memory + current_memory_frame * PAGE_SIZE, mmapfptr + (page * PAGE_SIZE), PAGE_SIZE); 
    page_table[page] = current_memory_frame; // Updates the page table
    
    // Increment the current frame index so that it keeps repeating and the physical address array is accessed like a circular array.
    if(current_memory_frame == 127){
        current_memory_frame = 0;
    } else {
        current_memory_frame += 1;
    }

    // Unmap the memory mapped file and close the file descriptor
    munmap(mmapfptr, BACKING_STORE_SIZE);
    close(mmapfile_fd);
}

// Given a frame number, find the page that is associated with that frame number. -1 if none is found.
int find_page_in_memory(int frame){
    for (int i = 0; i < 256; i++){
        if(page_table[i] == frame){
            return i;
        }
    }
    return -1;
}