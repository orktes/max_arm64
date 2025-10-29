/* zipfs.h -- ZIP filesystem abstraction providing standard file operations
 *
 * Copyright (C) 2025 Jaakko Lukkari
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */

#ifndef ZIPFS_H
#define ZIPFS_H

#include <stddef.h>
#include <stdio.h>

// Opaque handle for a ZIP filesystem instance
typedef struct zipfs_t zipfs_t;

/**
 * Open a ZIP archive and create a filesystem instance for it
 * @param zip_path Path to the ZIP file
 * @return ZIP filesystem handle, or NULL on error
 */
zipfs_t *zipfs_open(const char *zip_path);

/**
 * Close a ZIP filesystem and free all resources
 * Note: This will invalidate all open file handles from this filesystem
 * @param fs ZIP filesystem handle
 */
void zipfs_close(zipfs_t *fs);

/**
 * Open a file within the ZIP filesystem
 * @param fs ZIP filesystem handle
 * @param path Path to file within the ZIP (case-insensitive, / or \ separators)
 * @param mode File mode (only "r" and "rb" are supported)
 * @return Standard FILE* handle, or NULL if file not found or error
 */
FILE *zipfs_fopen(zipfs_t *fs, const char *path, const char *mode);

/**
 * Close a file handle opened with zipfs_fopen
 * @param file Standard FILE* handle from zipfs_fopen
 * @return 0 on success
 */
int zipfs_fclose(FILE *file);

/**
 * Read data from a file (standard fread compatible)
 * @param ptr Buffer to read into
 * @param size Size of each element
 * @param nmemb Number of elements to read
 * @param file Standard FILE* handle from zipfs_fopen
 * @return Number of elements successfully read
 */
size_t zipfs_fread(void *ptr, size_t size, size_t nmemb, FILE *file);

/**
 * Seek to a position in the file (standard fseek compatible)
 * @param file Standard FILE* handle from zipfs_fopen
 * @param offset Offset from whence
 * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
 * @return 0 on success, -1 on error
 */
int zipfs_fseek(FILE *file, long offset, int whence);

/**
 * Get current position in file (standard ftell compatible)
 * @param file Standard FILE* handle from zipfs_fopen
 * @return Current position, or -1 on error
 */
long zipfs_ftell(FILE *file);

/**
 * Check if a file exists in the ZIP filesystem
 * @param fs ZIP filesystem handle
 * @param path Path to file within the ZIP
 * @return 1 if file exists, 0 otherwise
 */
int zipfs_exists(zipfs_t *fs, const char *path);

/**
 * Get the size of a file in the ZIP filesystem
 * @param fs ZIP filesystem handle
 * @param path Path to file within the ZIP
 * @return File size in bytes, or -1 if file not found
 */
long zipfs_get_size(zipfs_t *fs, const char *path);

#endif // ZIPFS_H
