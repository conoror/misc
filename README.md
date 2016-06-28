# misc
Some misc code. My own version of deflate (rfc1951) for starters...

## clz
This uses callbacks to read and write data. The default is set to stdin and stdout. To start:
clz_create() will return a pointer to an anonymous state structure.
Then use
clz_setcb_get() and clz_setcb_put() to set the callbacks.
Finally call:
clz_decompress() to decompress.

### Example:
    size_t gzput(void *par, void *buf, size_t buflen)
    {
        int i;
        char *text = buf;
    
        printf("gzput received buffer of size %u\n", buflen);
    }

    ...

    FILE * fp;
    void *gzstate;
    unsigned int crc32;
    ...
    gzstate = clz_create();
    if (!gzstate)
    {
        perror("clz_create fail\n");
        fclose(fp);
        exit(1);
    }
    
    /* Set get to read from file pointer fp */

    if (!clz_setcb_get(gzstate, 0, (void *)fp, 0))
    {
        printf("clz_setcb_get fail\n");
        fclose(fp);
        exit(1);
    }
    
    /* Set put to use the gzput callback */

    if (!clz_setcb_put(gzstate, gzput, 0))
    {
        printf("clz_setcb_put fail\n");
        fclose(fp);
        exit(1);
    }
    
    /* Decompress and calculate crc32 */

    ret = clz_decompress(gzstate, 0, &crc32);
    if (!ret)
    {
        perror("error on decompress");
        fclose(fp);
        exit(1);
    }
    
    /* ret is how much input it ate */

    printf("decompress returns: %d, crc: %08x\n", ret, crc32);


