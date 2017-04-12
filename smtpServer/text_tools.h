/*************************************
 * Summary: some function tools for text
 * Author: ms
 * Date: 2017/3/22
 *
 ************************************/

#ifndef __OUT_PARAM__
#define __OUT_PARAM__
#endif

int get_line(unsigned char *buf,size_t size,unsigned char* ret);

int find_char(unsigned char * buf,size_t size,unsigned char ch);

unsigned char * read_info(unsigned char * buf,size_t size,unsigned char * key_str,__OUT_PARAM__ unsigned char * value_str);


char * jump_nonprintable_chars(char * buf,size_t size);

char * jump_all_field(char * buf);

bool read_config(char * buf,size_t size,char * key,__OUT_PARAM__ char * value,char spacer);
