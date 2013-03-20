/*
 * Copyright (C) 2013 Argonne National Laboratory, Department of Energy,
 *                    and UChicago Argonne, LLC.
 * Copyright (C) 2013 The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#include "test_posix.h"
#include "shipper_test.h"
#include "function_shipper_handler.h"
#include "bulk_data_shipper.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

unsigned int finalizing = 0;

int server_finalize(fs_handle_t handle)
{
    int fs_ret = S_SUCCESS;

    /* Get input parameters and data */
    fs_ret = fs_handler_get_input(handle, NULL);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not get function call input\n");
        return fs_ret;
    }

    finalizing++;

    /* Free handle and send response back (and free input struct fields) */
    fs_ret = fs_handler_complete(handle, NULL);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not complete function call\n");
        return fs_ret;
    }

    return fs_ret;
}

int server_posix_open(fs_handle_t handle)
{
    int fs_ret = S_SUCCESS;

    open_in_t  open_in_struct;
    open_out_t open_out_struct;

    const char *path;
    int flags;
    mode_t mode;
    int ret;

    /* Get input parameters and data */
    fs_ret = fs_handler_get_input(handle, &open_in_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not get function call input\n");
        return fs_ret;
    }

    path = open_in_struct.path;
    flags = open_in_struct.flags;
    mode = open_in_struct.mode;

    /* Call bla_open */
    printf("Calling open with path: %s\n", path);
    ret = open(path, flags, mode);

    /* Fill output structure */
    open_out_struct.ret = ret;

    /* Free handle and send response back (and free input struct fields) */
    fs_ret = fs_handler_complete(handle, &open_out_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not complete function call\n");
        return fs_ret;
    }

    return fs_ret;
}

int server_posix_close(fs_handle_t handle)
{
    int fs_ret = S_SUCCESS;

    close_in_t  close_in_struct;
    close_out_t close_out_struct;

    int fd;
    int ret;

    /* Get input parameters and data */
    fs_ret = fs_handler_get_input(handle, &close_in_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not get function call input\n");
        return fs_ret;
    }

    fd = close_in_struct.fd;

    /* Call bla_open */
    printf("Calling close with fd: %d\n", fd);
    ret = close(fd);

    /* Fill output structure */
    close_out_struct.ret = ret;

    /* Free handle and send response back (and free input struct fields) */
    fs_ret = fs_handler_complete(handle, &close_out_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not complete function call\n");
        return fs_ret;
    }

    return fs_ret;
}

int server_posix_write(fs_handle_t handle)
{
    int fs_ret = S_SUCCESS;

    write_in_t  write_in_struct;
    write_out_t write_out_struct;

    na_addr_t source = fs_handler_get_addr(handle);
    bds_handle_t bds_handle = NULL;
    bds_block_handle_t bds_block_handle = NULL;

    int fd;
    void *buf;
    size_t count;
    ssize_t ret;

    /* for debug */
    int i;
    const int *buf_ptr;

    /* Get input parameters and data */
    fs_ret = fs_handler_get_input(handle, &write_in_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not get function call input\n");
        return fs_ret;
    }

    bds_handle = write_in_struct.bds_handle;
    fd = write_in_struct.fd;

    /* Read bulk data here and wait for the data to be here  */
    count = bds_handle_get_size(bds_handle);
    buf = malloc(count);

    bds_block_handle_create(buf, count, BDS_READWRITE, &bds_block_handle);

    fs_ret = bds_read(bds_handle, source, bds_block_handle);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not read bulk data\n");
        return fs_ret;
    }

    fs_ret = bds_wait(bds_block_handle, BDS_MAX_IDLE_TIME);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not complete bulk data read\n");
        return fs_ret;
    }

    /* Check bulk buf */
    buf_ptr = buf;
    for (i = 0; i < (count / sizeof(int)); i++) {
        if (buf_ptr[i] != i) {
            printf("Error detected in bulk transfer, buf[%d] = %d, was expecting %d!\n", i, buf_ptr[i], i);
            break;
        }
    }

    printf("Calling write with fd: %d\n", fd);
    ret = write(fd, buf, count);

    /* Fill output structure */
    write_out_struct.ret = ret;

    /* Free handle and send response back (and free input struct fields) */
    fs_ret = fs_handler_complete(handle, &write_out_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not complete function call\n");
        return fs_ret;
    }

    /* Free block handle */
    fs_ret = bds_block_handle_free(bds_block_handle);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not free block call\n");
        return fs_ret;
    }

    free(buf);

    return fs_ret;
}

int server_posix_read(fs_handle_t handle)
{
    int fs_ret = S_SUCCESS;

    read_in_t  read_in_struct;
    read_out_t read_out_struct;

    na_addr_t dest = fs_handler_get_addr(handle);
    bds_handle_t bds_handle = NULL;
    bds_block_handle_t bds_block_handle = NULL;

    int fd;
    void *buf;
    size_t count;
    ssize_t ret;

    /* for debug */
    int i;
    const int *buf_ptr;

    /* Get input parameters and data */
    fs_ret = fs_handler_get_input(handle, &read_in_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not get function call input\n");
        return fs_ret;
    }

    bds_handle = read_in_struct.bds_handle;
    fd = read_in_struct.fd;

    /* Call read */
    count = bds_handle_get_size(bds_handle);
    buf = malloc(count);

    printf("Calling read with fd: %d\n", fd);
    ret = read(fd, buf, count);

    /* Check bulk buf */
    buf_ptr = buf;
    for (i = 0; i < (count / sizeof(int)); i++) {
        if (buf_ptr[i] != i) {
            printf("Error detected after read, buf[%d] = %d, was expecting %d!\n", i, buf_ptr[i], i);
            break;
        }
    }

    /* Create a new block handle to write the data */
    bds_block_handle_create(buf, ret, BDS_READ_ONLY, &bds_block_handle);

    /* Write bulk data here and wait for the data to be there  */
    fs_ret = bds_write(bds_handle, dest, bds_block_handle);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not write bulk data\n");
        return fs_ret;
    }

    fs_ret = bds_wait(bds_block_handle, BDS_MAX_IDLE_TIME);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not complete bulk data write\n");
        return fs_ret;
    }

    /* Fill output structure */
    read_out_struct.ret = ret;

    /* Free handle and send response back (and free input struct fields) */
    fs_ret = fs_handler_complete(handle, &read_out_struct);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not complete function call\n");
        return fs_ret;
    }

    /* Free block handle */
    fs_ret = bds_block_handle_free(bds_block_handle);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not free block call\n");
        return fs_ret;
    }

    free(buf);

    return fs_ret;
}

/******************************************************************************/
int main(int argc, char *argv[])
{
    na_network_class_t *network_class = NULL;
    unsigned int number_of_peers;
    int fs_ret;

    /* Used by Test Driver */
    printf("Waiting for client...\n");
    fflush(stdout);

    /* Initialize the interface */
    network_class = shipper_test_server_init(argc, argv, &number_of_peers);

    fs_ret = fs_handler_init(network_class);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not initialize function shipper handler\n");
        return EXIT_FAILURE;
    }

    fs_ret = bds_init(network_class);
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not initialize bulk data shipper\n");
        return EXIT_FAILURE;
    }

    /* Register routine */
    IOFSL_SHIPPER_HANDLER_REGISTER("open", server_posix_open, open_in_t, open_out_t);
    IOFSL_SHIPPER_HANDLER_REGISTER("write", server_posix_write, write_in_t, write_out_t);
    IOFSL_SHIPPER_HANDLER_REGISTER("read", server_posix_read, read_in_t, read_out_t);
    IOFSL_SHIPPER_HANDLER_REGISTER("close", server_posix_close, close_in_t, close_out_t);
    IOFSL_SHIPPER_HANDLER_REGISTER_FINALIZE(server_finalize);

    while (finalizing != number_of_peers) {
        /* Receive new function calls */
        fs_ret = fs_handler_process(FS_HANDLER_MAX_IDLE_TIME);
        if (fs_ret != S_SUCCESS) {
            fprintf(stderr, "Could not receive function call\n");
            return EXIT_FAILURE;
        }
    }

    printf("Finalizing...\n");

    /* Finalize the interface */
    fs_ret = bds_finalize();
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not finalize bulk data shipper\n");
        return EXIT_FAILURE;
    }

    fs_ret = fs_handler_finalize();
    if (fs_ret != S_SUCCESS) {
        fprintf(stderr, "Could not finalize function shipper handler\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
