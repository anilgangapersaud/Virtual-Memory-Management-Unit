#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// constants
#define TLB_SIZE 16
#define FRAME_SIZE 256
#define PAGE_TABLE_SIZE 256

/**
 * Represents a node in the doubly linked list stack
 **/ 
struct node {
    int pageNumber;
    struct node *prev;
    struct node *next;
};

/**
 * Find the page and push it to the top of the stack if it exists in the list, otherwise insert it on top of the stack.
 **/
void pushToFront(struct node **head, int page) {
    // traverse and check if page is in the list
    struct node *curr = *head;
    int pageExistsInList = 0;
    while (curr != NULL && pageExistsInList == 0) {
        if (curr->pageNumber == page) {
            // move this page to the front
            pageExistsInList = 1;
            if (curr->prev != NULL) {
                struct node *curr2 = *head;
                curr2->prev = curr;
                curr->prev->next = curr->next;
                if (curr->next != NULL) {
                    curr->next->prev = curr->prev;
                }
                curr->next = *head;
                curr->prev = NULL;
                *head = curr;
            }
        }
        curr = curr->next;
    }
    if (pageExistsInList == 0) {
        struct node *new_node = malloc(sizeof (struct node));
        // push the node to the front
        if (*head != NULL) {
            new_node->pageNumber = page;
            new_node->next = *head;
            new_node->prev = NULL;
            struct node *ptr = *head;
            ptr->prev = new_node;
            *head = new_node;
        } else {
            new_node->pageNumber = page;
            new_node->prev = NULL;
            new_node->next = NULL;
            *head = new_node;
        }
    }
}

/**
 * Get the tail of the linked list
 **/
int getLRUPage(struct node **head, int newPage) {
    struct node *cur = *head;
    int lru = 0;
    while (cur->next != NULL) {
        cur = cur->next;
    }
    lru = cur->pageNumber;
    cur->pageNumber = newPage;
    struct node *cur2 = *head;
    cur2->prev = cur;
    cur->prev->next = NULL;
    cur->prev = NULL;
    cur->next = *head;
    *head = cur;
    return lru;
}

/**
 * keep a pointer to the head of the stack
 **/
struct node **head = NULL;

/**
 * Get the page number of the logical address
 **/
int getPageNumber(int logicalAddress) {
    logicalAddress = logicalAddress >> 8;
    logicalAddress = logicalAddress & 0xFF;
    return logicalAddress;
}

/**
 *  Get the offset of the logical address
 */
int getOffset(int logicalAddress) {
    return logicalAddress & 0xFF;
}

/**
 * Perform translation and print them out in the csv file
 **/
int translate(int physicalMemorySize, char outputFilename[], int pageReplacement) {
    // initialize data structures
    int page_table[PAGE_TABLE_SIZE];
    char phys_mem[physicalMemorySize];
    char frame[FRAME_SIZE];
    int tlb[TLB_SIZE][2];
    char buffer[10];
    for (int i = 0; i < PAGE_TABLE_SIZE; i++) {
        page_table[i] = -1;
    }

    // open files
    FILE *in;
    FILE *bs;
    FILE *csv;
    in = fopen("/Users/dizaster/Desktop/skull/EECS3221/EECS3221_Project_MMU/StartKit/addresses.txt","r");
    bs = fopen("/Users/dizaster/Desktop/skull/EECS3221/EECS3221_Project_MMU/StartKit/BACKING_STORE.bin","r");
    csv = fopen(outputFilename, "write-mode");
    if (csv != NULL) {
        // initialize counters
        int logical_address = 0;
        int freeFrameIndex = 0;
        int tlbInsertionIndex = 0;
        float countPageFaults = 0;
        float countMemoryAccesses = 0;
        float tlbHitCount = 0;
        if (in != NULL) {
            // read logical address
            while (fgets(buffer,FRAME_SIZE,in) != NULL) {
                countMemoryAccesses++;
                logical_address = atoi(strdup(buffer));
                int pageNumber = getPageNumber(logical_address);
                int offset = getOffset(logical_address);
                char value = 'x';
                int phys_address_index = freeFrameIndex + offset;
                for (int tlbIndex = 0; tlbIndex < TLB_SIZE; tlbIndex++) {
                    if (tlb[tlbIndex][0] == pageNumber) {
                        tlbHitCount++;
                        phys_address_index = tlb[tlbIndex][1] + offset;
                        value = phys_mem[phys_address_index];
                    }
                }
                if (value == 'x') {
                    // cache the page number in tlb
                    tlb[tlbInsertionIndex][0] = pageNumber;
                    if (page_table[pageNumber] == -1) {
                        // page fault
                        countPageFaults++;
                        if (bs != NULL) {
                            if (fseek(bs, pageNumber * FRAME_SIZE, SEEK_SET) == 0) {
                                fread(frame, FRAME_SIZE, 1, bs);
                                if (pageReplacement && freeFrameIndex >= physicalMemorySize) {
                                    // physical memory is full - perform lru replacement
                                    int lruPage = getLRUPage(head, pageNumber);
                                    int frameToReplace = page_table[lruPage];
                                    for (int i = 0, j = frameToReplace; i < FRAME_SIZE; i++, j++) {
                                        phys_mem[j] = frame[i];
                                    }
                                    tlb[tlbInsertionIndex][1] = frameToReplace;
                                    phys_address_index = frameToReplace + offset;
                                    page_table[lruPage] = -1;
                                    page_table[pageNumber] = frameToReplace;
                                    value = phys_mem[phys_address_index];
                                } else {
                                    // physical memory is not full - use a free frame
                                    for (int i = 0, j = freeFrameIndex; i < FRAME_SIZE; i++, j++) {
                                        phys_mem[j] = frame[i];
                                    }
                                    value = phys_mem[phys_address_index];
                                    page_table[pageNumber] = freeFrameIndex;
                                    tlb[tlbInsertionIndex][1] = freeFrameIndex;
                                    freeFrameIndex += FRAME_SIZE;
                                }
                            }
                        } else {
                            return 3; // failed to open backing store
                        }
                    } else {
                        // page table hit
                        int frameIndex = page_table[pageNumber];
                        phys_address_index = frameIndex + offset;
                        value = phys_mem[phys_address_index];
                        tlb[tlbInsertionIndex][1] = frameIndex;
                    }
                    tlbInsertionIndex = (tlbInsertionIndex + 1) % TLB_SIZE;
                }
                if (head == NULL) {
                    head = malloc(sizeof(struct node));
                }
                pushToFront(head, pageNumber);
                fprintf(csv,"%d,%d,%d\n", logical_address, phys_address_index, value);
            }
        } else {
            return 2; // failed to open addresses.txt file
        }
        float pageFaultsRate = (countPageFaults / countMemoryAccesses) * 100;
        float tlbHitRate = (tlbHitCount / countMemoryAccesses) * 100;
        fprintf(csv,"Page Faults Rate, %.2f%%,\n", pageFaultsRate);
        fprintf(csv,"TLB Hits Rate, %.2f%%,\n", tlbHitRate);
    } else {
        return 1; // failed to open csv file for writing
    }
    fclose(bs);
    fclose(in);
    fclose(csv);
    return 0; // return with no error
}

int main(int argc, char *argv[]) {
    int result = -1;
    int PHASE_ONE_BYTES = 256;
    int PHASE_TWO_BYTES = 128;
    if (atoi(argv[1]) == PHASE_TWO_BYTES) {
        result = translate(128*256, "output128.csv", 1);
    } else if (atoi(argv[1]) == PHASE_ONE_BYTES) {
        result = translate(256*256, "output256.csv", 0);
    }
    return result;
}

