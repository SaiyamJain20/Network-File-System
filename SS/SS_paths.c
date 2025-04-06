#include "header.h"

// Head of the linked list
ListNode *accessiblePathsHead = NULL;

// Function to add a path to the accessible paths list
void add_accessible_path(char *path) {
    char *path_copy = strdup(path); // Allocate memory and copy the path
    if (!path_copy) {
        perror("Memory allocation failed");
        return;
    }
    append_to_list(&accessiblePathsHead, path_copy);
}

// Function to add all paths in a directory to the list (recursive)
void add_directory_to_accessible_paths(char *base_path, char *current_path) {
    DIR *dir = opendir(current_path);
    if (!dir) {
        perror("Failed to open directory");
        return;
    }

    struct dirent *entry;
    char full_path[1024];
    char relative_path[1024];
    struct stat path_stat;

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", current_path, entry->d_name);

        // Get file/directory metadata
        if (stat(full_path, &path_stat) < 0) {
            perror("Failed to stat path");
            continue;
        }

        if (S_ISDIR(path_stat.st_mode)) {
            snprintf(relative_path, sizeof(relative_path), "./%s/", full_path + strlen(base_path) + 1);
        } else {
            snprintf(relative_path, sizeof(relative_path), "./%s", full_path + strlen(base_path) + 1);
        }

        // Add the relative path to the list
        add_accessible_path(relative_path);

        // If it's a directory, recurse
        if (S_ISDIR(path_stat.st_mode)) {
            add_directory_to_accessible_paths(base_path, full_path);
        }
    }

    closedir(dir);
}

// Function to create a list of all accessible paths from root_dir
void get_all_paths(char *root_dir) {
    add_directory_to_accessible_paths(root_dir, root_dir);
    return;
}