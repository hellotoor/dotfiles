void hexdump(char *buf, int buf_len)
{
    int i = 0;
    for(i = 0; i < buf_len; i++) {
        printf("%02x", buf[i]);
        if((i+1) % 4 == 0) printf(" ");
    }
    printf("\n");
    for(i = 0; i < buf_len; i++) {
        if(isprint(buf[i]))
            printf("%c", buf[i]);
        else 
            printf(".");
        if((i+1) % 4 == 0) printf("     ");
    }
    printf("\n");
}
