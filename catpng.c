#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>   
#include <stdlib.h>  
#include <string.h>
#include "crc.h"
#include "zutil.h"
#include "png.h"
#include <arpa/inet.h>

char* initialize( simple_PNG_p newPNG, const char* PNG){
    if(!newPNG){
        perror("Attempted to write to undefined location\n");
        return NULL;
    }
    
    if(!PNG){
        perror("PNG file is undefined\n");
        return NULL;
    }
    
    FILE* first_png = fopen(PNG, "rb");
    
    chunk_p ihdr, idat, iend;
    ihdr = (chunk_p)malloc(sizeof(struct chunk));
    ihdr->ihdr_data = (data_IHDR_p)malloc(DATA_IHDR_SIZE);
    idat = (chunk_p)malloc(sizeof(struct chunk));
    iend = (chunk_p)malloc(sizeof(struct chunk));
    
    //save the 8 header bytes
    char* header = (char*)malloc(PNG_SIG_SIZE*sizeof(U8));
    fread(header, PNG_SIG_SIZE, sizeof(U8), first_png);
    
    //load length, type, data and CRC fields into IHDR chunk, data field is always DATA_IHDR_SIZE(size 13)
    fread(&(ihdr->length), CHUNK_LEN_SIZE, sizeof(U8), first_png);
    fread( ihdr->type, sizeof(U8), CHUNK_TYPE_SIZE, first_png);
    
    fread(&(ihdr->ihdr_data->width), 4, sizeof(U8), first_png);
    fread(&(ihdr->ihdr_data->height), 4, sizeof(U8), first_png);
    fread(&(ihdr->ihdr_data->bit_depth), 1, sizeof(U8), first_png);
    fread(&(ihdr->ihdr_data->color_type), 1, sizeof(U8), first_png);
    fread(&(ihdr->ihdr_data->compression), 1, sizeof(U8), first_png);
    fread(&(ihdr->ihdr_data->filter), 1, sizeof(U8), first_png);
    fread(&(ihdr->ihdr_data->interlace), 1, sizeof(U8), first_png);
    fread(&(ihdr->crc), CHUNK_CRC_SIZE, sizeof(U8), first_png);
    
    //load length, type, data and CRC fields into IDAT chunk
    fread(&(idat->length), CHUNK_LEN_SIZE, 1, first_png);
    
    idat->p_data = (U8*)malloc((ntohl(idat->length))*sizeof(U8));
    
    fread( idat->type, sizeof(U8), CHUNK_TYPE_SIZE, first_png);
    fread( idat->p_data, sizeof(U8), ntohl(idat->length), first_png);
    fread(&(idat->crc), CHUNK_CRC_SIZE, sizeof(U8), first_png);
    
    //load length, type, data and CRC fields into IEND chunk, data field is always empty(size 0)
    fread(&(iend->length), CHUNK_LEN_SIZE, 1, first_png);
    fread( iend->type, sizeof(U8), CHUNK_TYPE_SIZE, first_png);
    fread(&(iend->crc), CHUNK_CRC_SIZE, 1, first_png);
    iend->p_data = NULL;
    
    newPNG->p_IHDR = ihdr;
    newPNG->p_IDAT = idat;
    newPNG->p_IEND = iend;
    
    fclose(first_png);
    return header;
}

int main(int num_png, const char* png_strips[]){
    
    if( num_png < 2 ){
        perror("Please list some input files\n");
        exit(1);
    }
    
    simple_PNG_p all = (simple_PNG_p)malloc(sizeof(struct simple_PNG));
    
    char* header = initialize(all,png_strips[1]);
    
    int png_index;
    for(png_index=2; png_index<num_png; png_index++){
        
        /*
         * seek past PNG header (8 bytes) 8
         * seek to 4 byte IHDR->data->height field (12 bytes = 4 length + 4 type + 4 data->width) 20 <-of interest
         * read 4 bytes as pixel height of incoming file 24
         * seek to 4 byte IDAT->length field (9 bytes = 5 data + 4 CRC) 33 <- of interest
         * read 4 bytes as compressed data length of incoming file 37
         * seek to IDAT->data field (4 bytes) 41 <- of interest
         * read IDAT->length bytes as compressed data from incoming file
         * now have height, length, and compressed data stored, close input file
         */
        FILE* current_png = fopen(png_strips[png_index], "rb");
        if(!current_png){
            printf("Error opening file %s\n", png_strips[png_index]);
            continue;
        }
        
        U32 height, length;
        fseek(current_png, 20, SEEK_SET);
        fread(&height, sizeof(U32), 1, current_png);
        fseek(current_png, 9, SEEK_CUR);
        fread(&length, sizeof(U32), 1, current_png);
        fseek(current_png, 4, SEEK_CUR);
        
        U8* data = (U8*)malloc(ntohl(length)*sizeof(U8));
        fread(data, sizeof(U8), ntohl(length), current_png);
        
        fclose(current_png);

        /* beyond here will be the actual concatenation details
         * convert all fields to host language
         * inflate all->IDAT->p_data
         * inflate data
         * alloc buffer for all->IDAT->p_data concatenated with data
         * strcat the two uncompressed data buffers
         * deflate the newly allocated buffer
         * realloc size of all->IDAT->p_data to size of previous+length bytes
         * memcpy deflated data to all->IDAT->p_data
         * adjust all->IHDR->ihdr_data->height field to += height
         * adjust all->IDAT->length field to += length
         * dealloc temporary buffer
         * return all fields to network language
         * recalculate CRC codes
         */
               
        length = ntohl(length);
        height = ntohl(height);
        U64 source_len = (U64)ntohl(all->p_IDAT->length);
        all->p_IHDR->ihdr_data->width = ntohl(all->p_IHDR->ihdr_data->width);
        all->p_IHDR->ihdr_data->height = ntohl(all->p_IHDR->ihdr_data->height);
        
        
        U8* old_data = (U8*)malloc((((all->p_IHDR->ihdr_data->width) * 4) + 1)*(all->p_IHDR->ihdr_data->height)*4);
        U8* new_data = (U8*)malloc(((all->p_IHDR->ihdr_data->width * 4) + 1)*(height+(all->p_IHDR->ihdr_data->height))*4);
        
        U64 old_data_size_inf, new_data_size_inf, new_data_size_def;
        
        if( mem_inf( old_data, &old_data_size_inf, all->p_IDAT->p_data, source_len) != 0 ){
            printf("Failed to inflate old data\n");
            exit(2);
        }
        if( mem_inf( new_data, &new_data_size_inf, data, length) != 0 ){
            printf("Failed to inflate new data\n");
            exit(3);
        }
        
        U8* buffer = (U8*)malloc((old_data_size_inf + new_data_size_inf)*sizeof(U8));
        
        memcpy(buffer, old_data, old_data_size_inf);
        memcpy(buffer + old_data_size_inf, new_data, new_data_size_inf);
        all->p_IDAT->p_data = (U8*)realloc(all->p_IDAT->p_data, (source_len + length) );
        
        if( mem_def( (all->p_IDAT->p_data), &new_data_size_def, buffer, (old_data_size_inf + new_data_size_inf), Z_DEFAULT_COMPRESSION) < 0){
            printf("Failed to deflate new data\n");
            exit(4);
        }
        
        all->p_IHDR->ihdr_data->height += height;
        source_len += length;
        
        all->p_IDAT->length = htonl(source_len);
        all->p_IHDR->ihdr_data->width = htonl(all->p_IHDR->ihdr_data->width);
        all->p_IHDR->ihdr_data->height = htonl(all->p_IHDR->ihdr_data->height);
        
        free(old_data);
        free(new_data);
        free(buffer);
        free(data);
    }
    
    //calculating IHDR CRC code
    U8* ihdr_crc_buf = (U8*)malloc(CHUNK_TYPE_SIZE+DATA_IHDR_SIZE);
    memcpy(ihdr_crc_buf, all->p_IHDR->type, CHUNK_TYPE_SIZE);
    memcpy(ihdr_crc_buf+4, &(all->p_IHDR->ihdr_data->width), 4);
    memcpy(ihdr_crc_buf+8, &(all->p_IHDR->ihdr_data->height), 4);
    memcpy(ihdr_crc_buf+12, &(all->p_IHDR->ihdr_data->bit_depth), 1);
    memcpy(ihdr_crc_buf+13, &(all->p_IHDR->ihdr_data->color_type), 1);
    memcpy(ihdr_crc_buf+14, &(all->p_IHDR->ihdr_data->compression), 1);
    memcpy(ihdr_crc_buf+15, &(all->p_IHDR->ihdr_data->filter), 1);
    memcpy(ihdr_crc_buf+16, &(all->p_IHDR->ihdr_data->interlace), 1);
    
    all->p_IHDR->crc = htonl(crc(ihdr_crc_buf, CHUNK_TYPE_SIZE + DATA_IHDR_SIZE));
    free(ihdr_crc_buf);
    
    //calculating IDAT CRC code
    U8* idat_crc_buf = (U8*)malloc(CHUNK_TYPE_SIZE+ntohl(all->p_IDAT->length));
    memcpy(idat_crc_buf, all->p_IDAT->type, CHUNK_TYPE_SIZE);
    memcpy(idat_crc_buf+4, all->p_IDAT->p_data, ntohl(all->p_IDAT->length));
    
    all->p_IDAT->crc = htonl(crc(idat_crc_buf, CHUNK_TYPE_SIZE + ntohl(all->p_IDAT->length)));
    free(idat_crc_buf);
    
    FILE* allPNG = fopen( "all.png", "wb");
    int filesize = fwrite( header, sizeof(char), PNG_SIG_SIZE, allPNG);
    filesize += fwrite( &(all->p_IHDR->length), sizeof(char), CHUNK_LEN_SIZE, allPNG);
    filesize += fwrite( &(all->p_IHDR->type), sizeof(char), CHUNK_TYPE_SIZE, allPNG);
    filesize += fwrite( all->p_IHDR->ihdr_data, sizeof(char), DATA_IHDR_SIZE, allPNG);
    filesize += fwrite( &(all->p_IHDR->crc), sizeof(char), CHUNK_CRC_SIZE, allPNG);
    
    filesize += fwrite( &(all->p_IDAT->length), sizeof(char), CHUNK_LEN_SIZE, allPNG);
    filesize += fwrite( &(all->p_IDAT->type), sizeof(char), CHUNK_TYPE_SIZE, allPNG);
    filesize += fwrite( all->p_IDAT->p_data, sizeof(char), (ntohl(all->p_IDAT->length)), allPNG);
    filesize += fwrite( &(all->p_IDAT->crc), sizeof(char), CHUNK_CRC_SIZE, allPNG);
    
    filesize += fwrite( &(all->p_IEND->length), sizeof(char), CHUNK_LEN_SIZE, allPNG);
    filesize += fwrite( &(all->p_IEND->type), sizeof(char), CHUNK_TYPE_SIZE, allPNG);
    filesize += fwrite( &(all->p_IEND->crc), sizeof(char), CHUNK_CRC_SIZE, allPNG);
    
    fclose( allPNG );
    
    free( header );
    free( all->p_IHDR->ihdr_data );
    free( all->p_IHDR );
    free( all->p_IDAT->p_data );
    free( all->p_IDAT );
    free( all->p_IEND );
    free( all );
    
    return 0;
}
