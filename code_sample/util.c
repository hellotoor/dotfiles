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

char *memmem(char *src, int src_len, char *sub, int sub_len)
{
	int i, j;

	if(src == NULL || sub == NULL || src_len <= 0|| sub_len <= 0 || src_len < sub_len)
		return NULL;

	for (i = 0; i < src_len - sub_len; i++) 
	{
		for (j = 0; j < sub_len; j++) 
		{
			if(src[i+j] != sub[j])
				break;
		}

		if(j >= sub_len) 
		{
			return src + i;
		}
	}

	return NULL;
}
