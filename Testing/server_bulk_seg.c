/*
 * Copyright (C) 2013 Argonne National Laboratory, Department of Energy,
 *                    UChicago Argonne, LLC and The HDF Group.
 * All rights reserved.
 *
 * The full copyright notice, including terms governing use, modification,
 * and redistribution, is contained in the COPYING file that can be
 * found at the root of the source code distribution tree.
 */

#include "test_bulk.h"
#include "mercury_test.h"
#include "mercury_handler.h"
#include "mercury_bulk.h"

#include <stdio.h>
#include <stdlib.h>

/* Actual definition of the function that needs to be executed */
size_t
bla_write(int fildes, const void *buf, size_t nbyte)
{
    size_t i;
    int error = 0;
    const int *bulk_buf = (const int*) buf;

    printf("Executing bla_write with fildes %d...\n", fildes);

    if (nbyte == 0) {
        HG_ERROR_DEFAULT("Error detected in bulk transfer, nbyte is zero!\n");
        error = 1;
    }

    printf("Checking data...\n");

    /* Check bulk buf */
    for (i = 0; i < (nbyte / sizeof(int)); i++) {
        if (bulk_buf[i] != (int) i) {
            printf("Error detected in bulk transfer, bulk_buf[%lu] = %d, "
                    "was expecting %d!\n", i, bulk_buf[i], (int) i);
            error = 1;
            break;
        }
    }
    if (!error) printf("Successfully transfered %lu bytes!\n", nbyte);

    return nbyte;
}

/*****************************************************************************/
static hg_return_t
fs_bla_write(hg_handle_t handle)
{
    hg_return_t ret = HG_SUCCESS;

    bla_write_in_t  bla_write_in_struct;
    bla_write_out_t bla_write_out_struct;

    na_addr_t source = HG_Handler_get_addr(handle);
    hg_bulk_t bla_write_bulk_handle = HG_BULK_NULL;
    hg_bulk_t bla_write_bulk_block_handle = HG_BULK_NULL;
    size_t bla_write_nbytes_read;
    ptrdiff_t bla_write_offset;
    hg_bulk_request_t bla_write_bulk_request1;
    hg_bulk_request_t bla_write_bulk_request2;

    int bla_write_fildes;
    void *bla_write_buf;
    size_t bla_write_nbytes;
    int bla_write_ret;

    /* Get input parameters and data */
    ret = HG_Handler_get_input(handle, &bla_write_in_struct);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not get input\n");
        return ret;
    }

    /* Get parameters */
    bla_write_fildes = bla_write_in_struct.fildes;
    bla_write_bulk_handle = bla_write_in_struct.bulk_handle;

    /* Create a new block handle to read the data */
    bla_write_nbytes = HG_Bulk_handle_get_size(bla_write_bulk_handle);
    bla_write_buf = malloc(bla_write_nbytes);

    HG_Bulk_handle_create(bla_write_buf, bla_write_nbytes, HG_BULK_READWRITE,
            &bla_write_bulk_block_handle);

    /* For testing purposes try to read the data in two blocks of different sizes */
    bla_write_nbytes_read = bla_write_nbytes / 2 + 16;

    printf("Start reading first chunk of %lu bytes...\n", bla_write_nbytes_read);
    ret = HG_Bulk_read(source, bla_write_bulk_handle, 0,
            bla_write_bulk_block_handle, 0, bla_write_nbytes_read, &bla_write_bulk_request1);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not read bulk data\n");
        return ret;
    }
    bla_write_offset = bla_write_nbytes_read;
    bla_write_nbytes_read = bla_write_nbytes - bla_write_nbytes_read;

    printf("Start reading second chunk of %lu bytes...\n", bla_write_nbytes_read);
    ret = HG_Bulk_read(source, bla_write_bulk_handle, bla_write_offset,
            bla_write_bulk_block_handle, bla_write_offset,
            bla_write_nbytes_read, &bla_write_bulk_request2);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not read bulk data\n");
        return ret;
    }

    printf("Waiting for first chunk...\n");
    ret = HG_Bulk_wait(bla_write_bulk_request1, HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not complete bulk data read\n");
        return ret;
    }

    printf("Waiting for second chunk...\n");
    ret = HG_Bulk_wait(bla_write_bulk_request2, HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not complete bulk data read\n");
        return ret;
    }

    /* Call bla_write */
    bla_write_ret = bla_write(bla_write_fildes, bla_write_buf, bla_write_nbytes);

    /* Fill output structure */
    bla_write_out_struct.ret = bla_write_ret;

    /* Free handle and send response back */
    ret = HG_Handler_start_output(handle, &bla_write_out_struct);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not respond\n");
        return ret;
    }

    /* Free block handle */
    ret = HG_Bulk_handle_free(bla_write_bulk_block_handle);
    if (ret != HG_SUCCESS) {
        fprintf(stderr, "Could not free block call\n");
        return ret;
    }

    free(bla_write_buf);

    printf("\n");

    return ret;
}

/*****************************************************************************/
int
main(int argc, char *argv[])
{
    na_class_t *network_class = NULL;
    unsigned int i, number_of_peers;
    int hg_ret;

    /* Initialize the interface */
    network_class = HG_Test_server_init(argc, argv, NULL, NULL, &number_of_peers);

    hg_ret = HG_Handler_init(network_class);
    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "Could not initialize Mercury handler\n");
        return EXIT_FAILURE;
    }

    /* Register routine */
    MERCURY_HANDLER_REGISTER("bla_write", fs_bla_write, bla_write_in_t, bla_write_out_t);

    for (i = 0; i < number_of_peers; i++) {
        /* Receive new function calls */
        hg_ret = HG_Handler_process(HG_MAX_IDLE_TIME, HG_STATUS_IGNORE);
        if (hg_ret != HG_SUCCESS) {
            fprintf(stderr, "Could not receive function call\n");
            return EXIT_FAILURE;
        }
    }

    printf("Finalizing...\n");

    hg_ret = HG_Handler_finalize();
    if (hg_ret != HG_SUCCESS) {
        fprintf(stderr, "Could not finalize Mercury handler\n");
        return EXIT_FAILURE;
    }

    HG_Test_finalize(network_class);

    return EXIT_SUCCESS;
}
