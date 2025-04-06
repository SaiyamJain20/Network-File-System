#include "header.h"


typedef struct {
    int* server_ids;  // Array of storage server IDs
    int capacity;     // Maximum capacity of heap
    int size;         // Current size of heap
} MinHeap;

static void swap(int* x, int* y) {
    int temp = *x;
    *x = *y;
    *y = temp;
}

static int parent(int i) { return (i - 1) / 2; }

// Get left child index
static int left_child(int i) { return 2 * i + 1; }

// Get right child index
static int right_child(int i) { return 2 * i + 2; }

MinHeap* create_min_heap(int capacity) {
    MinHeap* heap = (MinHeap*)malloc(sizeof(MinHeap));
    heap->server_ids = (int*)malloc(capacity * sizeof(int));
    heap->capacity = capacity;
    heap->size = 0;
    return heap;
}

void insert_server(MinHeap* heap, int server_id) {
    if (heap->size >= heap->capacity) {
        return;  // Heap is full
    }

    // Insert new element at the end
    int i = heap->size;
    heap->server_ids[i] = server_id;
    heap->size++;

    // Fix the min heap property
    while (i != 0 && 
           g_storage_servers[heap->server_ids[parent(i)]].backups_hold > 
           g_storage_servers[heap->server_ids[i]].backups_hold) {
        swap(&heap->server_ids[i], &heap->server_ids[parent(i)]);
        i = parent(i);
    }
}

void min_heapify(MinHeap* heap, int i) {
    int smallest = i;
    int left = left_child(i);
    int right = right_child(i);

    if (left < heap->size && 
        g_storage_servers[heap->server_ids[left]].backups_hold < 
        g_storage_servers[heap->server_ids[smallest]].backups_hold)
        smallest = left;

    if (right < heap->size && 
        g_storage_servers[heap->server_ids[right]].backups_hold < 
        g_storage_servers[heap->server_ids[smallest]].backups_hold)
        smallest = right;

    if (smallest != i) {
        swap(&heap->server_ids[i], &heap->server_ids[smallest]);
        min_heapify(heap, smallest);
    }
}

int extract_min(MinHeap* heap) {
    if (heap->size <= 0)
        return -1;

    int min_server_id = heap->server_ids[0];
    heap->server_ids[0] = heap->server_ids[heap->size - 1];
    heap->size--;
    min_heapify(heap, 0);

    return min_server_id;
}

void initiate_backup_transfer(int source, int backup1, int backup2) {
    //see I cant wait for ack here right 

    pthread_mutex_lock(&g_storage_servers[backup1].lock);
    g_storage_servers[backup1].backups_hold++;
    g_storage_servers[backup1].hold_backup[source] = 1;
    pthread_mutex_unlock(&g_storage_servers[backup1].lock);
    //then send the request 
    SS_Request backu;
    memset(backu.info, 0, sizeof(backu.info));
    backu.error_code=OK;
    backu.Thread_id=-1;
    backu.SS_id=backup1;
    backu.type=REQUEST_CREATE_DIRECTORY;
    connect_and_send_to_SS(backu);
    memset(backu.info, 0, sizeof(backu.info));
    backu.type = REQUEST_BACKUP_DATA;
    char buffer[256]; // Adjust size based on expected max length of IP and port
    snprintf(buffer, sizeof(buffer), "%s|%d", g_storage_servers[backup1].ip, g_storage_servers[backup1].ss_port);
    strcpy(backu.info, buffer);
    connect_and_send_to_SS(backu);
    pthread_mutex_lock(&g_storage_servers[backup2].lock);
    g_storage_servers[backup2].backups_hold++;
    g_storage_servers[backup2].hold_backup[source] = 1;
    pthread_mutex_unlock(&g_storage_servers[backup2].lock);
     memset(backu.info, 0, sizeof(backu.info));
    backu.error_code=OK;
    backu.Thread_id=-1;
    backu.SS_id=backup2;
    backu.type=REQUEST_CREATE_DIRECTORY;
    connect_and_send_to_SS(backu);
    memset(backu.info, 0, sizeof(backu.info));
    backu.type = REQUEST_BACKUP_DATA;
    snprintf(buffer, sizeof(buffer), "%s|%d", g_storage_servers[backup2].ip, g_storage_servers[backup2].ss_port);
    strcpy(backu.info, buffer);
    connect_and_send_to_SS(backu);

    return;
}

void backup()
{
    if(g_ss_count<=2)
    {
        printf("CANT DO BACKUP NOW\n"); return;
    }
    for(int i=0;i<g_ss_count;i++)
    {
         if (pthread_mutex_trylock(&g_storage_servers[i].lock) != 0) {
            // Could not acquire lock, will try this server later
            continue;
        }
        if (g_storage_servers[i].is_alive && 
            (g_storage_servers[i].backup1 == -1 && g_storage_servers[i].backup2 == -1)) {
                
            // Attempt to find backup servers
            int backup1 = -1, backup2 = -1;
            
            // Create a temporary min heap for backup selection
            MinHeap* heap = create_min_heap(g_ss_count);
            
        for (int j = 0; j < g_ss_count; j++) {
                // Skip if same server, not alive, or already at max backups
                if (j != i && 
                    g_storage_servers[j].is_alive) {
                    // pthread_mutex_lock(&g_storage_servers[j].lock);
                    insert_server(heap, j);
                }
            }
            
            // Try to extract two backup servers
            if (heap->size >= 2) {
                backup1 = extract_min(heap);
                backup2 = extract_min(heap);
                
                while(heap->size>0)
                {
                    pthread_mutex_unlock(&g_storage_servers[heap->server_ids[0]].lock);
                    extract_min(heap);
                }
                // Update backup information
                g_storage_servers[i].backup1 = backup1;
                g_storage_servers[i].backup2 = backup2;
                
                // Update backups_hold for backup servers
                g_storage_servers[backup1].backups_hold++;
                g_storage_servers[backup2].backups_hold++;
                
                // Add source server to backup servers' hold_backup list
                g_storage_servers[i].hold_backup[backup1] =1;
                g_storage_servers[i].hold_backup[backup2] = 1;
                
                // Log backup assignment
                printf("Backup assigned for Server %d: Backup1 = %d, Backup2 = %d\n", 
                       i, backup1, backup2);
                
                // Initiate backup process
                // You would call your actual backup transfer function here
                // For example:
                initiate_backup_transfer(i, backup1, backup2);
            } else {
                printf("Not enough servers to backup Server %d\n", i);
                while(heap->size>0)
                {
                    // pthread_mutex_unlock(&g_storage_servers[heap->server_ids[0]].lock);
                    extract_min(heap);
                }
            }
            
            // Clean up heap
            free(heap->server_ids);
            free(heap);
            
            // Unlock the current server
            pthread_mutex_unlock(&g_storage_servers[i].lock);
        }
    }
}